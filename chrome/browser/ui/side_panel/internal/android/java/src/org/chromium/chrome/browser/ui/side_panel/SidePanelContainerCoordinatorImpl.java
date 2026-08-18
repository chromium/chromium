// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.ui.side_panel;

import static org.chromium.build.NullUtil.assertNonNull;
import static org.chromium.chrome.browser.ui.side_panel.SidePanelUtils.log;

import android.graphics.Rect;
import android.view.LayoutInflater;
import android.view.View;
import android.view.ViewGroup;
import android.view.ViewGroup.LayoutParams;
import android.view.accessibility.AccessibilityEvent;
import android.view.accessibility.AccessibilityNodeInfo;
import android.widget.LinearLayout;
import android.widget.TextView;

import androidx.annotation.Px;
import androidx.annotation.VisibleForTesting;
import androidx.core.view.ViewCompat;

import org.chromium.base.ThreadUtils;
import org.chromium.build.BuildConfig;
import org.chromium.build.annotations.NullMarked;
import org.chromium.build.annotations.Nullable;
import org.chromium.chrome.browser.tabmodel.TabModelSelector;
import org.chromium.chrome.browser.ui.side_ui.SideUiContainer;
import org.chromium.chrome.browser.ui.side_ui.SideUiCoordinator;
import org.chromium.chrome.browser.ui.side_ui.SideUiCoordinator.AnchorSide;
import org.chromium.chrome.browser.ui.side_ui.SideUiCoordinator.HeightType;
import org.chromium.chrome.browser.ui.side_ui.SideUiCoordinator.SideUiId;
import org.chromium.chrome.browser.ui.side_ui.SideUiCoordinator.SideUiSpecs.SideUiSize;
import org.chromium.chrome.browser.ui.side_ui.SideUiCoordinator.UiUpdateRequest;
import org.chromium.chrome.browser.ui.vertical_tabs.VerticalTabUtils;
import org.chromium.components.thinwebview.ThinWebView;
import org.chromium.ui.accessibility.AccessibilityState;
import org.chromium.ui.base.ActivityWindowAndroid;
import org.chromium.ui.base.ViewUtils;

/** Implementation of {@link SidePanelContainerCoordinator}. */
@NullMarked
final class SidePanelContainerCoordinatorImpl
        implements SidePanelContainerCoordinator, SideUiContainer {
    private static final String TAG = "SidePanelContainerCoordinatorImpl";

    private static final @AnchorSide int SIDE_PANEL_DEFAULT_ANCHOR_SIDE = AnchorSide.RIGHT;

    private final LinearLayout mContainerView;
    private final SidePanelNativeBridgeSelector mNativeBridgeSelector;
    private final SideUiCoordinator mSideUiCoordinator;

    private @Nullable SidePanelContent mCurrentContent;

    /** {@link Runnable} for {@link #startReplacingPanelContent} to remove the old content View. */
    private @Nullable Runnable mPendingReplaceRunnable;

    /**
     * Whether {@link #onWillAutoClose} is running.
     *
     * <p>This flag prevents {@link #onWillAutoClose} from triggering another UI update, which isn't
     * allowed.
     *
     * <p>The C++ {@code SidePanelCoordinatorAndroid} calls {@link #startClosingPanel} during {@link
     * #onWillAutoClose}. {@link #startClosingPanel} is also for non-auto-closing cases where a call
     * to {@link SideUiCoordinator#updateUi} is required, so we need this flag to avoid calling
     * {@link SideUiCoordinator#updateUi} for the auto-close case.
     *
     * <p>TODO(crbug.com/527985639): Refactor the C++ side and remove this flag.
     */
    private boolean mIsPreparingForAutoClose;

    /**
     * Whether {@link #onWillAutoRestore} is running.
     *
     * <p>This flag prevents {@link #onWillAutoRestore} from triggering another UI update, which
     * isn't allowed.
     *
     * <p>TODO(crbug.com/527985639): Refactor the C++ side and remove this flag.
     *
     * @see #mIsPreparingForAutoClose
     */
    private boolean mIsPreparingForAutoRestore;

    private boolean mEnableDeferredViewReplacementForTesting;
    private boolean mSimulateAutoCloseConditionForTesting;

    SidePanelContainerCoordinatorImpl(
            ActivityWindowAndroid windowAndroid,
            SideUiCoordinator sideUiCoordinator,
            TabModelSelector tabModelSelector) {
        log(TAG, "constructor");

        var activity = assertNonNull(windowAndroid.getActivity().get());
        mContainerView =
                (LinearLayout)
                        LayoutInflater.from(activity)
                                .inflate(R.layout.side_panel_container, /* root= */ null);

        mNativeBridgeSelector =
                new SidePanelNativeBridgeSelector(
                        windowAndroid, /* sidePanelContainerCoordinator= */ this, tabModelSelector);
        mSideUiCoordinator = sideUiCoordinator;
    }

    ///////////////////////////////////////////////////////////////////////////////////////////////
    //              Start of SidePanelContainerCoordinator Implementation                        //
    ///////////////////////////////////////////////////////////////////////////////////////////////

    @Override
    public void init() {
        log(TAG, "init");
        ThreadUtils.assertOnUiThread();
        mSideUiCoordinator.registerSideUiContainer(this);
        mNativeBridgeSelector.init();
    }

    /**
     * Returns whether this side panel container <i>can</i> be shown, i.e., whether there is enough
     * space for it.
     *
     * @see org.chromium.chrome.browser.ui.side_ui.SideUiStateProvider#canShowSideUi
     */
    boolean canShow() {
        ThreadUtils.assertOnUiThread();
        boolean canShow = mSideUiCoordinator.canShowSideUi(SideUiId.SIDE_PANEL);

        log(TAG, "canShow", canShow);
        return canShow;
    }

    /**
     * Starts opening this side panel container with the given {@link SidePanelContent}.
     *
     * <p>This method is intended for a side panel feature and should only be called when the side
     * panel isn't shown.
     *
     * @param content Wrapper object for the content to show in the side panel.
     * @param startingBounds Optional bounds for the animation to start from.
     * @param suppressAnimations Whether or not to suppress animations for this populate request.
     */
    void startOpeningPanel(
            SidePanelContent content, @Nullable Rect startingBounds, boolean suppressAnimations) {
        log(TAG, "startOpeningPanel", content, startingBounds, suppressAnimations);
        ThreadUtils.assertOnUiThread();

        // TODO(crbug.com/513302000): assert the side panel is currently closed.

        mCurrentContent = content;
        ViewGroup contentContainer = getContentContainer();
        contentContainer.removeAllViews();
        configureHeader(content);
        contentContainer.addView(content.mView);

        assert !mIsPreparingForAutoClose;
        if (!mIsPreparingForAutoRestore) {
            mSideUiCoordinator.updateUi(
                    new UiUpdateRequest(SideUiId.SIDE_PANEL, suppressAnimations));
        }
    }

    /**
     * Starts closing this side panel container.
     *
     * <p>This method is for a side panel feature.
     *
     * @param suppressAnimations Whether or not to suppress animations for this removal.
     */
    void startClosingPanel(boolean suppressAnimations) {
        log(TAG, "startClosingPanel", suppressAnimations);
        ThreadUtils.assertOnUiThread();

        assert !mIsPreparingForAutoRestore;
        if (!mIsPreparingForAutoClose) {
            mSideUiCoordinator.updateUi(
                    new UiUpdateRequest(SideUiId.SIDE_PANEL, suppressAnimations));
        }
    }

    /**
     * Starts replacing the {@link SidePanelContent} inside this container.
     *
     * <p>This method is for a side panel feature and should only be called when the side panel is
     * shown.
     *
     * <p>Note that replacing the content shouldn't have animations, but it still needs to be async
     * to make the UI smooth. For example, if the new content is a {@code ThinWebView}, we need to
     * wait for the first frame of its web contents before removing the old content.
     *
     * @param newContent Wrapper object for the new content to show in the side panel.
     */
    void startReplacingPanelContent(SidePanelContent newContent) {
        log(TAG, "startReplacingPanelContent", newContent);
        ThreadUtils.assertOnUiThread();

        // TODO(crbug.com/513302000): assert the side panel is currently open.
        // TODO(crbug.com/513302000): assert the side panel isn't preparing for auto-restore/close.

        assert mCurrentContent != null : "no content to replace";
        View oldContentView = mCurrentContent.mView;
        mCurrentContent = newContent;

        configureHeader(newContent);
        ViewGroup contentContainer = getContentContainer();
        contentContainer.addView(newContent.mView, /* index= */ 0);

        // We use a custom Runnable class with a `mRan` flag because ThinWebView's runOnNextFrame()
        // does not support cancellation.
        //
        // If a new content replacement happens before the next frame renders, we must immediately
        // run the pending runnable to clean up the old state. When the next frame eventually fires
        // for that older replacement, its local `removeOldViewRunnable` will run again. The `mRan`
        // guard flag prevents running the cleanup logic (and JNI callbacks) a second time.
        //
        // We also check `mPendingReplaceRunnable == this` before clearing the member variable. This
        // is because `mPendingReplaceRunnable` always tracks the *latest* replacement request. If
        // an older runnable runs (either immediately because it was superseded, or late because of
        // the frame callback), it must not clear `mPendingReplaceRunnable` if a newer replacement
        // is now pending.
        Runnable removeOldViewRunnable =
                new Runnable() {
                    private boolean mRan;

                    @Override
                    public void run() {
                        if (mRan) return;

                        // Immediately set mRan to true to prevent re-entrancy.
                        mRan = true;

                        contentContainer.removeView(oldContentView);
                        assertNonNull(mNativeBridgeSelector.getCurrentCoordinatorBridge())
                                .onPanelContentReplaced();

                        notifyAccessibilityStateChanged(
                                AccessibilityEvent.CONTENT_CHANGE_TYPE_PANE_TITLE,
                                newContent.mTitle,
                                /* requestFocus= */ true);

                        // If the work is for the current runnable, clear the runnable.
                        if (mPendingReplaceRunnable == this) {
                            mPendingReplaceRunnable = null;
                        }
                    }
                };

        mPendingReplaceRunnable = removeOldViewRunnable;
        ThinWebView thinWebView = findThinWebView(newContent.mView);

        // If there is no ThinWebView, immediately complete the content View replacement since this
        // won't cause UI flickers.
        if (thinWebView == null) {
            completePendingContentReplacementInternal();
            return;
        }

        // If there is a ThinWebView, but we are in a test that doesn't explicitly enable the
        // deferred View removal, also complete the View replacement immediately.
        if (BuildConfig.IS_FOR_TEST && !mEnableDeferredViewReplacementForTesting) {
            completePendingContentReplacementInternal();
            return;
        }

        // Otherwise, remove the old content View when ThinWebView has rendered the first frame.
        // This is to prevent UI flickers.
        thinWebView.runOnNextFrame(removeOldViewRunnable);
    }

    /** Immediately completes any pending content replacement. */
    void completePendingContentReplacement() {
        log(TAG, "completePendingContentReplacement");
        ThreadUtils.assertOnUiThread();
        completePendingContentReplacementInternal();
    }

    /** Immediately ends all ongoing animations. */
    void endAnimations() {
        mSideUiCoordinator.endAnimations();
    }

    @Override
    public @Nullable View getContentView() {
        ThreadUtils.assertOnUiThread();
        return mCurrentContent != null ? mCurrentContent.mView : null;
    }

    @Override
    public void destroy() {
        log(TAG, "destroy");
        ThreadUtils.assertOnUiThread();
        mSideUiCoordinator.unregisterSideUiContainer(this);

        // Detach the side panel content View.
        //
        // A side panel feature may choose to reuse its content View in a different
        // SidePanelContainerCoordinator instance, such as the instance in a new window.
        //
        // So we need to detach the content view when this container is destroyed. Otherwise, the
        // content view will keep a reference to this container as the parent View, which will
        // cause:
        //
        // (1) memory leaks, and
        // (2) a crash when the content View is added to another container instance.
        getContentContainer().removeAllViews();
        mCurrentContent = null;
        mNativeBridgeSelector.destroy();
    }

    /**
     * Enables or disables deferred content View replacement for testing.
     *
     * <p>When (1) this is enabled and (2) the new active tab during a tab switch requires replacing
     * the side panel content View with a {@code ThinWebView}, the old content View won't be removed
     * until the {@code ThinWebView} has rendered the first frame.
     *
     * <p>(2) is <i>always</i> enabled in production to prevent UI flickers during tab switches.
     *
     * <p>In tests, (2) needs to be explicitly enabled since tests covering the deferred content
     * View replacement need to wait for the replacement to complete. Not all tests have the "wait"
     * logic, and it's hard to add it since there are many existing cross-platform side panel
     * browser tests that assume synchronous replacement.
     *
     * @param enable Whether deferred View replacement is enabled.
     */
    void configDeferredViewReplacementForTesting(boolean enable) {
        mEnableDeferredViewReplacementForTesting = enable;
    }

    /**
     * Simulates the condition that will cause the side panel container to auto-close.
     *
     * <p>If the side panel is open when this method is called, we'll run the same code responding
     * to a {@code Configuration} change or other events that force the side panel to auto-close.
     *
     * <p>If the side panel is closed when this method is called, subsequent attempts to show the
     * side panel won't open the panel until {@link #simulateAutoRestoreConditionForTesting()} is
     * called.
     *
     * @see org.chromium.chrome.browser.ui.side_ui.SideUiContainer#onWillAutoClose()
     */
    void simulateAutoCloseConditionForTesting() {
        mSimulateAutoCloseConditionForTesting = true;

        // Don't pass a SideUiId in UiUpdateRequest. In production, auto-close is triggered by
        // events outside the side panel container, such as a Configuration change. The side panel
        // container will never _request_ to be auto-closed.
        mSideUiCoordinator.updateUi(
                new UiUpdateRequest(/* sideUiId= */ null, /* suppressAnimations= */ true));
    }

    /**
     * Simulates the condition that will cause the side panel container to auto-restore.
     *
     * @see #simulateAutoCloseConditionForTesting()
     */
    void simulateAutoRestoreConditionForTesting() {
        mSimulateAutoCloseConditionForTesting = false;

        // Don't pass a SideUiId in UiUpdateRequest. In production, auto-restore is triggered by
        // events outside the side panel container, such as a Configuration change. The side panel
        // container will never _request_ to be auto-restored.
        mSideUiCoordinator.updateUi(
                new UiUpdateRequest(/* sideUiId= */ null, /* suppressAnimations= */ true));
    }

    ///////////////////////////////////////////////////////////////////////////////////////////////
    //              End of SidePanelContainerCoordinator Implementation                          //
    ///////////////////////////////////////////////////////////////////////////////////////////////

    ///////////////////////////////////////////////////////////////////////////////////////////////
    //              Start of SideUiContainer Implementation                                      //
    ///////////////////////////////////////////////////////////////////////////////////////////////

    @Override
    public View getView() {
        log(TAG, "getView");
        ThreadUtils.assertOnUiThread();
        return mContainerView;
    }

    @Override
    public @SideUiId int getSideUiId() {
        return SideUiId.SIDE_PANEL;
    }

    @Override
    @AnchorSide
    public int getAnchorSide() {
        log(TAG, "getAnchorSide");
        ThreadUtils.assertOnUiThread();
        return SIDE_PANEL_DEFAULT_ANCHOR_SIDE;
    }

    @Override
    public SideUiSize determineShowableSize(
            @Px int availableWidth, @Px int windowWidth, boolean isFullscreen) {
        log(TAG, "determineShowableSize", availableWidth, windowWidth, isFullscreen);
        ThreadUtils.assertOnUiThread();

        if (mSimulateAutoCloseConditionForTesting) {
            return new SideUiSize(0, HeightType.NOT_APPLICABLE);
        }

        var context = mContainerView.getContext();
        int availableWidthDp = ViewUtils.pxToDp(context, availableWidth);
        int windowWidthDp = ViewUtils.pxToDp(context, windowWidth);

        int horizontalPaddingDp =
                ViewUtils.pxToDp(
                        context,
                        mContainerView.getPaddingLeft() + mContainerView.getPaddingRight());
        int minSidePanelContainerWidthDp = horizontalPaddingDp + MIN_SIDE_PANEL_CONTENT_WIDTH_DP;

        int showableWidthDp =
                determineShowableWidthDp(
                        availableWidthDp, windowWidthDp, minSidePanelContainerWidthDp);
        @HeightType
        int heightType =
                determineHeightType(
                        showableWidthDp, VerticalTabUtils.isVerticalTabsEnabled(context));

        return new SideUiSize(ViewUtils.dpToPx(context, showableWidthDp), heightType);
    }

    @Override
    public boolean hasContentToShow() {
        ThreadUtils.assertOnUiThread();
        var sidePanelCoordinatorAndroidBridge = mNativeBridgeSelector.getCurrentCoordinatorBridge();
        return sidePanelCoordinatorAndroidBridge != null
                && sidePanelCoordinatorAndroidBridge.hasContentToShow();
    }

    @Override
    public void setWidth(@Px int width) {
        log(TAG, "setWidth", width);
        ThreadUtils.assertOnUiThread();

        LayoutParams layoutParams = mContainerView.getLayoutParams();
        assert layoutParams != null
                : "setWidth() should be called after the container View is attached";
        assert layoutParams.height == LayoutParams.MATCH_PARENT
                : "the container View's height should match its parent";

        if (layoutParams.width != width) {
            layoutParams.width = width;
            mContainerView.setLayoutParams(layoutParams);
        }

        // Remove the content if setting the width the 0 (i.e. hiding the panel).
        if (width == 0) {
            getContentContainer().removeAllViews();
            mCurrentContent = null;
        }

        // TODO(http://crbug.com/488047364): Notify the SidePanelContent View of the width change.
    }

    @Override
    public void onUiUpdateCompleted(
            @Px int oldWidth,
            @Px int newWidth,
            @HeightType int oldHeightType,
            @HeightType int newHeightType) {
        assertNonNull(mNativeBridgeSelector.getCurrentCoordinatorBridge())
                .onPanelContainerUpdated(oldWidth, newWidth);

        // Accessibility support for opening/closing the panel.
        if (oldWidth == 0 && newWidth > 0) {
            CharSequence paneTitle = mCurrentContent != null ? mCurrentContent.mTitle : null;
            notifyAccessibilityStateChanged(
                    AccessibilityEvent.CONTENT_CHANGE_TYPE_PANE_APPEARED,
                    paneTitle,
                    /* requestFocus= */ true);
        } else if (oldWidth > 0 && newWidth == 0) {
            notifyAccessibilityStateChanged(
                    AccessibilityEvent.CONTENT_CHANGE_TYPE_PANE_DISAPPEARED,
                    /* title= */ null,
                    /* requestFocus= */ false);
        }
    }

    @SuppressWarnings("AccessibilityFocus")
    private void notifyAccessibilityStateChanged(
            int eventType, @Nullable CharSequence title, boolean requestFocus) {
        CharSequence oldTitle = ViewCompat.getAccessibilityPaneTitle(mContainerView);
        ViewCompat.setAccessibilityPaneTitle(mContainerView, title);

        AccessibilityEvent event =
                AccessibilityEvent.obtain(AccessibilityEvent.TYPE_WINDOW_STATE_CHANGED);
        event.setContentChangeTypes(eventType);

        CharSequence eventText =
                eventType == AccessibilityEvent.CONTENT_CHANGE_TYPE_PANE_DISAPPEARED
                        ? oldTitle
                        : title;
        if (eventText != null) {
            event.getText().add(eventText);
        }
        event.setSource(mContainerView);
        AccessibilityState.sendAccessibilityEvent(event);

        if (requestFocus) {
            // The focus change needs to happen after the view has measured its bounds per the
            // a11y contract, or else TalkBack will lose focus or not focus at all. Posting
            // gives a chance for this to come afterwards.
            mContainerView.post(
                    () -> {
                        mContainerView.performAccessibilityAction(
                                AccessibilityNodeInfo.ACTION_ACCESSIBILITY_FOCUS, null);
                    });
        }
    }

    @Override
    public void onWillAutoClose() {
        mIsPreparingForAutoClose = true;
        assertNonNull(mNativeBridgeSelector.getCurrentCoordinatorBridge()).onWillAutoClose();
        mIsPreparingForAutoClose = false;
    }

    @Override
    public void onWillAutoRestore() {
        mIsPreparingForAutoRestore = true;
        assertNonNull(mNativeBridgeSelector.getCurrentCoordinatorBridge()).onWillAutoRestore();
        mIsPreparingForAutoRestore = false;
    }

    ///////////////////////////////////////////////////////////////////////////////////////////////
    //              End of SideUiContainer Implementation                                        //
    ///////////////////////////////////////////////////////////////////////////////////////////////

    /**
     * Returns the final width (in dp) of the side panel container given the available width in the
     * window, the window width, and the minimum side panel container width.
     */
    @VisibleForTesting
    static int determineShowableWidthDp(
            int availableWidthDp, int windowWidthDp, int minSidePanelContainerWidthDp) {
        // 1. Check if we can use the fixed, larger width.
        if (windowWidthDp >= MIN_WINDOW_WIDTH_DP_FOR_WIDE_SIDE_PANEL) {
            assert availableWidthDp >= WIDE_SIDE_PANEL_WIDTH_DP;
            return WIDE_SIDE_PANEL_WIDTH_DP;
        }

        // 2. Check if we can use the fixed, smaller width.
        if (availableWidthDp >= NARROW_SIDE_PANEL_WIDTH_DP) {
            return NARROW_SIDE_PANEL_WIDTH_DP;
        }

        // 3. If we can't use the fixed, smaller width, but the available space is more than the
        // minimum container width, we'll fill the available space.
        if (availableWidthDp >= minSidePanelContainerWidthDp) {
            return availableWidthDp;
        }

        // 4. Return 0 if available space can't accommodate the minimum side panel width.
        return 0;
    }

    @VisibleForTesting
    static @HeightType int determineHeightType(int showableWidthDp, boolean isVerticalTabsEnabled) {
        @HeightType int heightType = HeightType.NOT_APPLICABLE;
        if (showableWidthDp != 0) {
            heightType = isVerticalTabsEnabled ? HeightType.WEB_CONTENTS : HeightType.TOOLBAR;
        }
        return heightType;
    }

    private @Nullable ThinWebView findThinWebView(View view) {
        if (view instanceof ThinWebView) {
            return (ThinWebView) view;
        }
        if (view instanceof ViewGroup) {
            ViewGroup group = (ViewGroup) view;
            for (int i = 0; i < group.getChildCount(); i++) {
                ThinWebView child = findThinWebView(group.getChildAt(i));
                if (child != null) {
                    return child;
                }
            }
        }
        return null;
    }

    private void configureHeader(SidePanelContent content) {
        int vis;
        if (!content.mShowHeader) {
            vis = View.GONE;
        } else {
            vis = View.VISIBLE;
            assert content.mTitle != null;
            TextView titleView = mContainerView.findViewById(R.id.side_panel_title);
            titleView.setText(content.mTitle);
            mContainerView
                    .findViewById(R.id.side_panel_close_button)
                    .setOnClickListener(
                            v ->
                                    assertNonNull(
                                                    mNativeBridgeSelector
                                                            .getCurrentCoordinatorBridge())
                                            .closePanel());
        }
        View headerView = mContainerView.findViewById(R.id.side_panel_header);
        headerView.setVisibility(vis);
    }

    private void completePendingContentReplacementInternal() {
        if (mPendingReplaceRunnable != null) {
            mPendingReplaceRunnable.run();

            // Explicitly set to null for readability, though it is also handled
            // internally by the runnable's run() method.
            mPendingReplaceRunnable = null;
        }
    }

    private ViewGroup getContentContainer() {
        return (ViewGroup) mContainerView.findViewById(R.id.side_panel_content_container);
    }
}
