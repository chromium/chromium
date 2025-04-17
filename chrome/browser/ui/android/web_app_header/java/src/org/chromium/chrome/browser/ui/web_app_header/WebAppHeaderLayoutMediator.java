// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.ui.web_app_header;

import android.graphics.Rect;

import org.chromium.base.Callback;
import org.chromium.base.ResettersForTesting;
import org.chromium.base.supplier.ObservableSupplier;
import org.chromium.base.supplier.ObservableSupplierImpl;
import org.chromium.base.supplier.Supplier;
import org.chromium.build.annotations.NullMarked;
import org.chromium.build.annotations.Nullable;
import org.chromium.chrome.browser.tab.Tab;
import org.chromium.components.browser_ui.desktop_windowing.AppHeaderState;
import org.chromium.components.browser_ui.desktop_windowing.DesktopWindowStateManager;
import org.chromium.ui.modelutil.PropertyModel;

import java.util.List;

/**
 * Mediator that listens for window state changes and updates custom webapp header view that should
 * take the place of caption bar/title bar/app header at the top of the window.
 */
@NullMarked
class WebAppHeaderLayoutMediator implements DesktopWindowStateManager.AppHeaderObserver {
    @Nullable static Integer sMinHeaderHeightForTesting;

    private static final Rect EMPTY_NON_DRAGGABLE_AREA = new Rect(0, 0, 0, 0);

    private final Rect mCachedPaddings;
    private final PropertyModel mModel;
    private final DesktopWindowStateManager mDesktopWindowStateManager;
    private final ObservableSupplier<Tab> mTabSupplier;
    private final Supplier<List<Rect>> mNonDraggableAreasSupplier;
    private final Supplier<Boolean> mIsPendingLayoutSupplier;
    private final ObservableSupplierImpl<Integer> mWidthSupplier;
    private final Callback<Integer> mOnWidthChangedCallback;
    private final int mWebAppMinHeaderHeight;
    private @Nullable AppHeaderState mCurrentHeaderState;

    /**
     * Constructs the instance of {@link WebAppHeaderLayoutMediator}.
     *
     * @param model model that encapsulates UI state for the view
     * @param desktopWindowStateManager desktop window state manager that provides updates on window
     *     state
     * @param webAppHeaderMinHeightFromResources minimal height from resources in px that web app
     *     header must take
     */
    public WebAppHeaderLayoutMediator(
            PropertyModel model,
            DesktopWindowStateManager desktopWindowStateManager,
            ObservableSupplier<Tab> tabSupplier,
            Supplier<List<Rect>> nonDraggableAreasSupplier,
            Supplier<Boolean> isPendingLayoutSupplier,
            int webAppHeaderMinHeightFromResources) {
        mWebAppMinHeaderHeight = webAppHeaderMinHeightFromResources;
        mDesktopWindowStateManager = desktopWindowStateManager;
        mTabSupplier = tabSupplier;
        mNonDraggableAreasSupplier = nonDraggableAreasSupplier;
        mIsPendingLayoutSupplier = isPendingLayoutSupplier;

        mWidthSupplier = new ObservableSupplierImpl<>();
        mOnWidthChangedCallback = (width) -> updateNonDraggableAreas();
        mWidthSupplier.addObserver(mOnWidthChangedCallback);

        final var modelPaddings = model.get(WebAppHeaderLayoutProperties.PADDINGS);
        mCachedPaddings = modelPaddings != null ? modelPaddings : new Rect(0, 0, 0, 0);

        mModel = model;
        // View should notify us about initial width.
        mModel.set(WebAppHeaderLayoutProperties.WIDTH_CHANGED_CALLBACK, mWidthSupplier::set);

        final var appHeaderState = desktopWindowStateManager.getAppHeaderState();
        if (appHeaderState != null) {
            onAppHeaderStateChanged(appHeaderState);
        }
        mDesktopWindowStateManager.addObserver(this);
    }

    @Override
    public void onAppHeaderStateChanged(AppHeaderState newState) {
        mCurrentHeaderState = newState;

        updatePaddings();
        mModel.set(
                WebAppHeaderLayoutProperties.MIN_HEIGHT,
                Math.max(mCurrentHeaderState.getAppHeaderHeight(), getDefaultMinHeight()));
        mModel.set(
                WebAppHeaderLayoutProperties.IS_VISIBLE, mCurrentHeaderState.isInDesktopWindow());
    }

    private void updatePaddings() {
        if (mCurrentHeaderState == null) return;

        // Matching behavior to BrApp: add top padding when caption bar insets are greater than our
        // min height expectations. Rationale - some vendors provide caption bar insets as
        // caption bar + status bar insets, to layout properly we need to deduct status bar insets.
        // This assumption is optimistic - we assume that caption bar matches our min height.
        final int topPadding =
                Math.max(0, mCurrentHeaderState.getAppHeaderHeight() - getDefaultMinHeight());

        mCachedPaddings.set(
                mCurrentHeaderState.getLeftPadding(),
                topPadding,
                mCurrentHeaderState.getRightPadding(),
                mCachedPaddings.bottom);
        mModel.set(WebAppHeaderLayoutProperties.PADDINGS, mCachedPaddings);
    }

    private void updateNonDraggableAreas() {
        if (mCurrentHeaderState == null || !mCurrentHeaderState.isInDesktopWindow()) {
            // Should pass non-empty list, otherwise the previous one is kept.
            mModel.set(
                    WebAppHeaderLayoutProperties.NON_DRAGGABLE_AREAS,
                    List.of(EMPTY_NON_DRAGGABLE_AREA));
            return;
        }

        // Skip an update, because header is stale anyway. We will get a new width after the layout
        // and there we will update a non-draggable area.
        if (mIsPendingLayoutSupplier.get()) return;

        final var areas = mNonDraggableAreasSupplier.get();
        mModel.set(WebAppHeaderLayoutProperties.NON_DRAGGABLE_AREAS, areas);
    }

    /** Navigates back in the navigation history of the current {@link Tab}. */
    public void goBack() {
        final var tab = mTabSupplier.get();
        if (tab != null && tab.canGoBack()) {
            tab.goBack();
        }
    }

    private int getDefaultMinHeight() {
        if (sMinHeaderHeightForTesting != null) return sMinHeaderHeightForTesting;
        return mWebAppMinHeaderHeight;
    }

    static void setMinHeightForTesting(final int height) {
        sMinHeaderHeightForTesting = height;
        ResettersForTesting.register(() -> sMinHeaderHeightForTesting = null);
    }

    /** Destroys the mediator, existing instance is not usable after this method is called */
    public void destroy() {
        mDesktopWindowStateManager.removeObserver(this);
        mWidthSupplier.removeObserver(mOnWidthChangedCallback);
    }
}
