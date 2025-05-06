// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.ui.web_app_header;

import android.graphics.Rect;

import androidx.annotation.VisibleForTesting;

import org.chromium.base.Callback;
import org.chromium.base.ResettersForTesting;
import org.chromium.base.supplier.ObservableSupplier;
import org.chromium.base.supplier.ObservableSupplierImpl;
import org.chromium.base.supplier.Supplier;
import org.chromium.build.annotations.NullMarked;
import org.chromium.build.annotations.Nullable;
import org.chromium.chrome.browser.tab.Tab;
import org.chromium.chrome.browser.theme.ThemeColorProvider;
import org.chromium.components.browser_ui.desktop_windowing.AppHeaderState;
import org.chromium.components.browser_ui.desktop_windowing.DesktopWindowStateManager;
import org.chromium.components.browser_ui.widget.scrim.ScrimManager;
import org.chromium.ui.modelutil.PropertyModel;
import org.chromium.ui.util.TokenHolder;

import java.util.List;

/**
 * Mediator that listens for window state changes and updates custom webapp header view that should
 * take the place of caption bar/title bar/app header at the top of the window.
 */
@NullMarked
class WebAppHeaderLayoutMediator
        implements DesktopWindowStateManager.AppHeaderObserver,
                ThemeColorProvider.ThemeColorObserver {
    @Nullable static Integer sMinHeaderHeightForTesting;

    private static final Rect EMPTY_NON_DRAGGABLE_AREA = new Rect(0, 0, 0, 0);

    private final PropertyModel mModel;
    private final WebAppHeaderDelegate mHeaderDelegate;
    private final DesktopWindowStateManager mDesktopWindowStateManager;
    private final ObservableSupplier<@Nullable Tab> mTabSupplier;
    private final ScrimManager mScrimManager;
    private final Supplier<List<Rect>> mNonDraggableAreasSupplier;
    private final ObservableSupplierImpl<Integer> mWidthSupplier;
    private final Callback<Integer> mOnWidthChangedCallback;
    private final ThemeColorProvider mThemeColorProvider;
    private final int mWebAppMinHeaderHeight;
    private @Nullable AppHeaderState mCurrentHeaderState;
    private final ObservableSupplierImpl<Integer> mAppHeaderUnoccludedWidthSupplier;
    private final Callback<Boolean> mScrimVisibilityObserver;

    private int mDisabledControlsToken = TokenHolder.INVALID_TOKEN;

    /**
     * Constructs the instance of {@link WebAppHeaderLayoutMediator}.
     *
     * @param model model that encapsulates UI state for the view
     * @param desktopWindowStateManager desktop window state manager that provides updates on window
     *     state
     * @param tabSupplier supplier current active tab
     * @param nonDraggableAreasSupplier provides header's non-draggable areas
     * @param themeColorProvider provides theme for top level controls
     * @param webAppHeaderMinHeightFromResources minimal height from resources in px that web app
     *     header must take
     */
    public WebAppHeaderLayoutMediator(
            PropertyModel model,
            WebAppHeaderDelegate headerDelegate,
            DesktopWindowStateManager desktopWindowStateManager,
            ScrimManager scrimManager,
            ObservableSupplier<@Nullable Tab> tabSupplier,
            Supplier<List<Rect>> nonDraggableAreasSupplier,
            ThemeColorProvider themeColorProvider,
            int webAppHeaderMinHeightFromResources) {
        mThemeColorProvider = themeColorProvider;
        mWebAppMinHeaderHeight = webAppHeaderMinHeightFromResources;
        mHeaderDelegate = headerDelegate;
        mDesktopWindowStateManager = desktopWindowStateManager;
        mTabSupplier = tabSupplier;
        mNonDraggableAreasSupplier = nonDraggableAreasSupplier;

        mScrimVisibilityObserver =
                (isScrimVisible) -> {
                    if (isScrimVisible) {
                        mDisabledControlsToken =
                                mHeaderDelegate.disableControlsAndClearOldToken(
                                        mDisabledControlsToken);
                    } else {
                        mHeaderDelegate.releaseDisabledControlsToken(mDisabledControlsToken);
                    }
                };
        mScrimManager = scrimManager;
        mScrimManager.getScrimVisibilitySupplier().addObserver(mScrimVisibilityObserver);

        mWidthSupplier = new ObservableSupplierImpl<>();
        mAppHeaderUnoccludedWidthSupplier = new ObservableSupplierImpl<>();
        mOnWidthChangedCallback = (width) -> updateNonDraggableAreas();
        mWidthSupplier.addObserver(mOnWidthChangedCallback);

        mModel = model;
        // View should notify us about initial width.
        mModel.set(WebAppHeaderLayoutProperties.WIDTH_CHANGED_CALLBACK, mWidthSupplier::set);

        final var appHeaderState = desktopWindowStateManager.getAppHeaderState();
        if (appHeaderState != null) {
            onAppHeaderStateChanged(appHeaderState);
        }
        mDesktopWindowStateManager.addObserver(this);

        onThemeColorChanged(mThemeColorProvider.getThemeColor(), false);
        mThemeColorProvider.addThemeColorObserver(this);
    }

    @Override
    public void onThemeColorChanged(int color, boolean shouldAnimate) {
        mDesktopWindowStateManager.updateForegroundColor(color);
        mModel.set(WebAppHeaderLayoutProperties.BACKGROUND_COLOR, color);
    }

    @Override
    public void onAppHeaderStateChanged(AppHeaderState newState) {
        mCurrentHeaderState = newState;

        updatePaddings();

        mAppHeaderUnoccludedWidthSupplier.set(mCurrentHeaderState.getUnoccludedRectWidth());
        mModel.set(
                WebAppHeaderLayoutProperties.MIN_HEIGHT,
                Math.max(mCurrentHeaderState.getAppHeaderHeight(), getDefaultMinHeight()));
        mModel.set(
                WebAppHeaderLayoutProperties.IS_VISIBLE, mCurrentHeaderState.isInDesktopWindow());
    }

    public ObservableSupplier<Integer> getUnoccludedWidthSupplier() {
        return mAppHeaderUnoccludedWidthSupplier;
    }

    private void updatePaddings() {
        if (mCurrentHeaderState == null) return;

        // Matching behavior to BrApp: add top padding when caption bar insets are greater than our
        // min height expectations. Rationale - some vendors provide caption bar insets as
        // caption bar + status bar insets, to layout properly we need to deduct status bar insets.
        // This assumption is optimistic - we assume that caption bar matches our min height.
        final int topPadding =
                Math.max(0, mCurrentHeaderState.getAppHeaderHeight() - getDefaultMinHeight());

        final var paddings =
                new Rect(
                        mCurrentHeaderState.getLeftPadding(),
                        topPadding,
                        mCurrentHeaderState.getRightPadding(),
                        0);
        mModel.set(WebAppHeaderLayoutProperties.PADDINGS, paddings);
    }

    private void updateNonDraggableAreas() {
        if (mCurrentHeaderState == null || !mCurrentHeaderState.isInDesktopWindow()) {
            // Should pass non-empty list, otherwise the previous one is kept.
            mModel.set(
                    WebAppHeaderLayoutProperties.NON_DRAGGABLE_AREAS,
                    List.of(EMPTY_NON_DRAGGABLE_AREA));
            return;
        }

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
        mThemeColorProvider.removeThemeColorObserver(this);
        mScrimManager.getScrimVisibilitySupplier().removeObserver(mScrimVisibilityObserver);
    }

    @VisibleForTesting
    Callback<Boolean> getScrimVisibilityObserver() {
        return mScrimVisibilityObserver;
    }
}
