// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.components.browser_ui.bottomsheet;

import android.view.LayoutInflater;
import android.view.View;
import android.view.ViewGroup;
import android.view.Window;

import androidx.annotation.VisibleForTesting;

import org.chromium.base.Callback;
import org.chromium.base.supplier.Supplier;
import org.chromium.components.browser_ui.widget.scrim.ScrimCoordinator;
import org.chromium.components.browser_ui.widget.scrim.ScrimProperties;
import org.chromium.ui.KeyboardVisibilityDelegate;
import org.chromium.ui.modelutil.PropertyModel;
import org.chromium.ui.util.AccessibilityUtil;
import org.chromium.ui.util.TokenHolder;

import java.util.ArrayList;
import java.util.Iterator;
import java.util.List;
import java.util.PriorityQueue;

/**
 * This class is responsible for managing the content shown by the {@link BottomSheet}. Features
 * wishing to show content in the {@link BottomSheet} UI must implement {@link BottomSheetContent}
 * and call {@link #requestShowContent(BottomSheetContent, boolean)} which will return true if the
 * content was actually shown (see full doc on method).
 */
class BottomSheetControllerImpl implements ManagedBottomSheetController {
    /** The initial capacity for the priority queue handling pending content show requests. */
    private static final int INITIAL_QUEUE_CAPACITY = 1;

    /** A handle to the {@link BottomSheet} that this class controls. */
    private BottomSheet mBottomSheet;

    /** A queue for content that is waiting to be shown in the {@link BottomSheet}. */
    private PriorityQueue<BottomSheetContent> mContentQueue;

    /** Whether the controller is already processing a hide request for the tab. */
    private boolean mIsProcessingHideRequest;

    /** A runnable that initializes the bottom sheet when necessary. */
    private Runnable mSheetInitializer;

    /**
     * A list of observers maintained by this controller until the bottom sheet is created, at which
     * point they will be added to the bottom  sheet.
     */
    private List<BottomSheetObserver> mPendingSheetObservers;

    /** The state of the sheet so it can be returned to what it was prior to suppression. */
    @SheetState
    private int mSheetStateBeforeSuppress;

    /** The content being shown prior to the sheet being suppressed. */
    private BottomSheetContent mContentWhenSuppressed;

    /** A means of accessing the ScrimCoordinator. */
    private Supplier<ScrimCoordinator> mScrimCoordinatorSupplier;

    /**
     * A set of tokens for features suppressing the bottom sheet. If this holder has tokens, the
     * sheet is suppressed.
     */
    private final TokenHolder mSuppressionTokens;

    /** A means of checking whether accessibility is currently enabled. */
    private AccessibilityUtil mAccessibilityUtil;

    /**
     * Build a new controller of the bottom sheet.
     * @param scrim A supplier of the scrim that shows when the bottom sheet is opened.
     * @param initializedCallback A callback for the sheet being created (as the sheet is not
     *                            initialized until first use.
     * @param window A means of accessing the screen size.
     * @param keyboardDelegate A means of hiding the keyboard.
     * @param root The view that should contain the sheet.
     */
    public BottomSheetControllerImpl(final Supplier<ScrimCoordinator> scrim,
            Callback<View> initializedCallback, Window window,
            KeyboardVisibilityDelegate keyboardDelegate, Supplier<ViewGroup> root) {
        mScrimCoordinatorSupplier = scrim;
        mPendingSheetObservers = new ArrayList<>();
        mSuppressionTokens = new TokenHolder(() -> onSuppressionTokensChanged());

        mSheetInitializer = () -> {
            initializeSheet(initializedCallback, window, keyboardDelegate, root);
        };
    }

    /**
     * Do the actual initialization of the bottom sheet.
     * @param initializedCallback A callback for the creation of the sheet.
     * @param window A means of accessing the screen size.
     * @param keyboardDelegate A means of hiding the keyboard.
     * @param root The view that should contain the sheet.
     */
    private void initializeSheet(Callback<View> initializedCallback, Window window,
            KeyboardVisibilityDelegate keyboardDelegate, Supplier<ViewGroup> root) {
        LayoutInflater.from(root.get().getContext()).inflate(R.layout.bottom_sheet, root.get());
        mBottomSheet = (BottomSheet) root.get().findViewById(R.id.bottom_sheet);
        initializedCallback.onResult(mBottomSheet);

        mBottomSheet.init(window, keyboardDelegate);
        mBottomSheet.setAccssibilityUtil(mAccessibilityUtil);

        // Initialize the queue with a comparator that checks content priority.
        mContentQueue = new PriorityQueue<>(INITIAL_QUEUE_CAPACITY,
                (content1, content2) -> content1.getPriority() - content2.getPriority());

        PropertyModel scrimProperties =
                new PropertyModel.Builder(ScrimProperties.REQUIRED_KEYS)
                        .with(ScrimProperties.TOP_MARGIN, 0)
                        .with(ScrimProperties.AFFECTS_STATUS_BAR, true)
                        .with(ScrimProperties.ANCHOR_VIEW, mBottomSheet)
                        .with(ScrimProperties.SHOW_IN_FRONT_OF_ANCHOR_VIEW, false)
                        .with(ScrimProperties.CLICK_DELEGATE,
                                () -> {
                                    if (!mBottomSheet.isSheetOpen()) return;
                                    mBottomSheet.setSheetState(
                                            mBottomSheet.getMinSwipableSheetState(), true,
                                            StateChangeReason.TAP_SCRIM);
                                })
                        .build();

        mBottomSheet.addObserver(new EmptyBottomSheetObserver() {
            /**
             * Whether the scrim was shown for the last content.
             * TODO(mdjones): We should try to make sure the content in the sheet is not nulled
             *                prior to the close event occurring; sheets that don't have a peek
             *                state make this difficult since the sheet needs to be hidden before it
             *                is closed.
             */
            private boolean mScrimShown;

            @Override
            public void onSheetOpened(@StateChangeReason int reason) {
                if (mBottomSheet.getCurrentSheetContent() != null
                        && mBottomSheet.getCurrentSheetContent().hasCustomScrimLifecycle()) {
                    return;
                }

                mScrimCoordinatorSupplier.get().showScrim(scrimProperties);
                mScrimShown = true;
            }

            @Override
            public void onSheetClosed(@StateChangeReason int reason) {
                // Hide the scrim if the current content doesn't have a custom scrim lifecycle.
                if (mScrimShown) {
                    mScrimCoordinatorSupplier.get().hideScrim(true);
                    mScrimShown = false;
                }

                // Try to swap contents unless the sheet's content has a custom lifecycle.
                if (mBottomSheet.getCurrentSheetContent() != null
                        && !mBottomSheet.getCurrentSheetContent().hasCustomLifecycle()) {
                    // If the sheet is closed, it is an opportunity for another content to try to
                    // take its place if it is a higher priority.
                    BottomSheetContent content = mBottomSheet.getCurrentSheetContent();
                    BottomSheetContent nextContent = mContentQueue.peek();
                    if (content != null && nextContent != null
                            && nextContent.getPriority() < content.getPriority()) {
                        mContentQueue.add(content);
                        mBottomSheet.setSheetState(SheetState.HIDDEN, true);
                    }
                }
            }

            @Override
            public void onSheetStateChanged(@SheetState int state) {
                // If hiding request is in progress, destroy the current sheet content being hidden
                // even when it is in suppressed state. See https://crbug.com/1057966.
                if (state != SheetState.HIDDEN
                        || (!mIsProcessingHideRequest && mSuppressionTokens.hasTokens())) {
                    return;
                }
                if (mBottomSheet.getCurrentSheetContent() != null) {
                    mBottomSheet.getCurrentSheetContent().destroy();
                }
                mIsProcessingHideRequest = false;
                showNextContent(true);
            }
        });

        // Add any of the pending observers that were added prior to the sheet being created.
        for (int i = 0; i < mPendingSheetObservers.size(); i++) {
            mBottomSheet.addObserver(mPendingSheetObservers.get(i));
        }
        mPendingSheetObservers.clear();

        mSheetInitializer = null;
    }

    @Override
    public void setBrowserControlsHiddenRatio(float ratio) {
        if (mBottomSheet != null) mBottomSheet.setBrowserControlsHiddenRatio(ratio);
    }

    @Override
    public ScrimCoordinator getScrimCoordinator() {
        return mScrimCoordinatorSupplier.get();
    }

    @Override
    public PropertyModel createScrimParams() {
        return new PropertyModel.Builder(ScrimProperties.REQUIRED_KEYS)
                .with(ScrimProperties.TOP_MARGIN, 0)
                .with(ScrimProperties.AFFECTS_STATUS_BAR, true)
                .with(ScrimProperties.ANCHOR_VIEW, mBottomSheet)
                .with(ScrimProperties.SHOW_IN_FRONT_OF_ANCHOR_VIEW, false)
                .with(ScrimProperties.CLICK_DELEGATE,
                        () -> {
                            if (!mBottomSheet.isSheetOpen()) return;
                            mBottomSheet.setSheetState(mBottomSheet.getMinSwipableSheetState(),
                                    true, StateChangeReason.TAP_SCRIM);
                        })
                .build();
    }

    // Destroyable implementation.
    @Override
    public void destroy() {
        if (mBottomSheet != null) mBottomSheet.destroy();
    }

    @Override
    public boolean handleBackPress() {
        // If suppressed (therefore invisible), users are likely to expect for Chrome
        // browser, not the bottom sheet, to react. Do not consume the event.
        if (mBottomSheet == null || mSuppressionTokens.hasTokens()) return false;

        // Give the sheet the opportunity to handle the back press itself before falling to the
        // default "close" behavior.
        if (getCurrentSheetContent() != null && getCurrentSheetContent().handleBackPress()) {
            return true;
        }

        if (!mBottomSheet.isSheetOpen()) return false;
        int sheetState = mBottomSheet.getMinSwipableSheetState();
        mBottomSheet.setSheetState(sheetState, true, StateChangeReason.BACK_PRESS);
        return true;
    }

    @Override
    public BottomSheetContent getCurrentSheetContent() {
        return mBottomSheet == null ? null : mBottomSheet.getCurrentSheetContent();
    }

    @Override
    @SheetState
    public int getSheetState() {
        return mBottomSheet == null ? SheetState.HIDDEN : mBottomSheet.getSheetState();
    }

    @Override
    @SheetState
    public int getTargetSheetState() {
        return mBottomSheet == null ? SheetState.NONE : mBottomSheet.getTargetSheetState();
    }

    @Override
    public boolean isSheetOpen() {
        return mBottomSheet != null && mBottomSheet.isSheetOpen();
    }

    @Override
    public boolean isSheetHiding() {
        return mBottomSheet == null ? false : mBottomSheet.isHiding();
    }

    @Override
    public int getCurrentOffset() {
        return mBottomSheet == null ? 0 : (int) mBottomSheet.getCurrentOffsetPx();
    }

    @Override
    public int getContainerHeight() {
        return mBottomSheet != null ? (int) mBottomSheet.getSheetContainerHeight() : 0;
    }

    @Override
    public int getTopShadowHeight() {
        return mBottomSheet != null ? (int) mBottomSheet.getToolbarShadowHeight() : 0;
    }

    @Override
    public void addObserver(BottomSheetObserver observer) {
        if (mBottomSheet == null) {
            mPendingSheetObservers.add(observer);
            return;
        }
        mBottomSheet.addObserver(observer);
    }

    @Override
    public void removeObserver(BottomSheetObserver observer) {
        if (mBottomSheet != null) {
            mBottomSheet.removeObserver(observer);
        } else {
            mPendingSheetObservers.remove(observer);
        }
    }

    /** Handle a change in the state of the token holder responsible for the suppression tokens. */
    private void onSuppressionTokensChanged() {
        if (!mSuppressionTokens.hasTokens()) doUnsuppression();
    }

    @Override
    public int suppressSheet(@StateChangeReason int reason) {
        boolean hadTokens = mSuppressionTokens.hasTokens();
        int token = mSuppressionTokens.acquireToken();
        if (!hadTokens && mBottomSheet != null) {
            mSheetStateBeforeSuppress = getSheetState();
            mContentWhenSuppressed = getCurrentSheetContent();
            mBottomSheet.setSheetState(SheetState.HIDDEN, false, reason);
        }

        return token;
    }

    @Override
    public void unsuppressSheet(int token) {
        mSuppressionTokens.releaseToken(token);
    }

    private void doUnsuppression() {
        if (mBottomSheet == null) return;

        if (mBottomSheet.getCurrentSheetContent() != null) {
            @SheetState
            int openState = mContentWhenSuppressed == getCurrentSheetContent()
                    ? mSheetStateBeforeSuppress
                    : mBottomSheet.getOpeningState();
            mBottomSheet.setSheetState(openState, true);
        } else {
            // In the event the previous content was hidden, try to show the next one.
            showNextContent(true);
        }
        mContentWhenSuppressed = null;
        mSheetStateBeforeSuppress = SheetState.NONE;
    }

    @Override
    public void setAccssibilityUtil(AccessibilityUtil enabledSupplier) {
        mAccessibilityUtil = enabledSupplier;
    }

    @VisibleForTesting
    void setSheetStateForTesting(@SheetState int state, boolean animate) {
        mBottomSheet.setSheetState(state, animate);
    }

    @VisibleForTesting
    View getBottomSheetViewForTesting() {
        return mBottomSheet;
    }

    @VisibleForTesting
    public void endAnimationsForTesting() {
        mBottomSheet.endAnimations();
    }

    @VisibleForTesting
    public void forceDismissAllContent() {
        clearRequestsAndHide();

        // Handle content that has a custom lifecycle.
        hideContent(mBottomSheet.getCurrentSheetContent(), /* animate= */ true);
    }

    @Override
    public boolean requestShowContent(BottomSheetContent content, boolean animate) {
        if (content == null) {
            throw new RuntimeException("Attempting to show null content in the sheet!");
        }

        if (mBottomSheet == null) mSheetInitializer.run();

        // If already showing the requested content, do nothing.
        if (content == mBottomSheet.getCurrentSheetContent()) return true;

        boolean shouldSwapForPriorityContent = mBottomSheet.getCurrentSheetContent() != null
                && content.getPriority() < mBottomSheet.getCurrentSheetContent().getPriority()
                && canBottomSheetSwitchContent();

        // Always add the content to the queue, it will be handled after the sheet closes if
        // necessary. If already hidden, |showNextContent| will handle the request.
        mContentQueue.add(content);

        if (mBottomSheet.getCurrentSheetContent() == null && !mSuppressionTokens.hasTokens()) {
            showNextContent(animate);
            return true;
        } else if (shouldSwapForPriorityContent) {
            mContentQueue.add(mBottomSheet.getCurrentSheetContent());
            if (!mSuppressionTokens.hasTokens()) {
                mBottomSheet.setSheetState(SheetState.HIDDEN, animate);
                return true;
            } else {
                // Since the sheet is already suppressed and hidden, clear the sheet's content if
                // the requested content is higher priority. The unsuppression logic will figure out
                // which content to show next.
                mBottomSheet.showContent(null);
            }
        }
        return false;
    }

    @Override
    public void hideContent(
            BottomSheetContent content, boolean animate, @StateChangeReason int hideReason) {
        if (mBottomSheet == null) return;

        if (content != mBottomSheet.getCurrentSheetContent()) {
            mContentQueue.remove(content);
            return;
        }

        if (mIsProcessingHideRequest) return;

        // Handle showing the next content if it exists.
        if (mBottomSheet.getSheetState() == SheetState.HIDDEN) {
            // If the sheet is already hidden, destroy it and simply show the next content.
            // TODO(mdjones): Add tests to make sure the content is being destroyed as expected.
            if (mBottomSheet.getCurrentSheetContent() != null) {
                mBottomSheet.getCurrentSheetContent().destroy();
            }
            showNextContent(animate);
        } else {
            mIsProcessingHideRequest = true;
            mBottomSheet.setSheetState(SheetState.HIDDEN, animate, hideReason);
        }
    }

    @Override
    public void hideContent(BottomSheetContent content, boolean animate) {
        hideContent(content, animate, StateChangeReason.NONE);
    }

    @Override
    public void expandSheet() {
        if (mBottomSheet == null || mSuppressionTokens.hasTokens()) return;

        if (mBottomSheet.getCurrentSheetContent() == null) return;
        mBottomSheet.setSheetState(SheetState.HALF, true);
    }

    @Override
    public boolean collapseSheet(boolean animate) {
        if (mBottomSheet == null || mSuppressionTokens.hasTokens()) return false;
        if (mBottomSheet.isSheetOpen() && mBottomSheet.isPeekStateEnabled()) {
            mBottomSheet.setSheetState(SheetState.PEEK, animate);
            return true;
        }
        return false;
    }

    /**
     * Show the next {@link BottomSheetContent} if it is available and peek the sheet. If no content
     * is available the sheet's content is set to null.
     * @param animate Whether the sheet should animate opened.
     */
    private void showNextContent(boolean animate) {
        if (mBottomSheet.getSheetState() != SheetState.HIDDEN) {
            throw new RuntimeException("Showing next content before sheet is hidden!");
        }

        if (mContentQueue.isEmpty()) {
            mBottomSheet.showContent(null);
            return;
        }

        BottomSheetContent nextContent = mContentQueue.poll();
        mBottomSheet.showContent(nextContent);
        mBottomSheet.setSheetState(mBottomSheet.getOpeningState(), animate);
    }

    @Override
    public void clearRequestsAndHide() {
        if (mBottomSheet == null) return;

        clearRequests(mContentQueue.iterator());

        BottomSheetContent currentContent = mBottomSheet.getCurrentSheetContent();
        if (currentContent == null || !currentContent.hasCustomLifecycle()) {
            hideContent(currentContent, /* animate= */ true);
        }
        mContentWhenSuppressed = null;
        mSheetStateBeforeSuppress = SheetState.NONE;
    }

    /**
     * Remove all contents from {@code iterator} that don't have a custom lifecycle.
     * @param iterator The iterator whose items must be removed.
     */
    private void clearRequests(Iterator<BottomSheetContent> iterator) {
        while (iterator.hasNext()) {
            if (!iterator.next().hasCustomLifecycle()) {
                iterator.remove();
            }
        }
    }

    /**
     * The bottom sheet cannot change content while it is open. If the user has the bottom sheet
     * open, they are currently engaged in a task and shouldn't be interrupted.
     * @return Whether the sheet currently supports switching its content.
     */
    private boolean canBottomSheetSwitchContent() {
        return !mBottomSheet.isSheetOpen();
    }
}
