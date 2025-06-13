// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.ui.web_app_header;

import android.graphics.Rect;
import android.view.View;

import androidx.annotation.VisibleForTesting;

import org.chromium.base.Callback;
import org.chromium.base.ResettersForTesting;
import org.chromium.base.metrics.RecordHistogram;
import org.chromium.base.supplier.ObservableSupplier;
import org.chromium.base.supplier.ObservableSupplierImpl;
import org.chromium.base.supplier.Supplier;
import org.chromium.blink.mojom.DisplayMode;
import org.chromium.build.annotations.NullMarked;
import org.chromium.build.annotations.Nullable;
import org.chromium.chrome.browser.tab.Tab;
import org.chromium.chrome.browser.theme.ThemeColorProvider;
import org.chromium.chrome.browser.ui.web_app_header.WebAppHeaderUtils.BackEvent;
import org.chromium.chrome.browser.ui.web_app_header.WebAppHeaderUtils.ReloadType;
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
    private final ThemeColorProvider mThemeColorProvider;
    private final int mWebAppMinHeaderHeight;
    private final int mHeaderButtonHeight;
    private @Nullable AppHeaderState mCurrentHeaderState;
    private final ObservableSupplierImpl<Integer> mAppHeaderUnoccludedWidthSupplier;
    private final Callback<Boolean> mScrimVisibilityObserver;
    private @Nullable Callback<Integer> mOnButtonBottomInsetChanged;
    private int mButtonBottomInset;
    private final @DisplayMode.EnumType int mDisplayMode;

    private int mDisabledControlsToken = TokenHolder.INVALID_TOKEN;
    private boolean mIsFirstAppHeaderStateUpdate = true;

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
            int webAppHeaderMinHeightFromResources,
            int headerButtonHeight,
            int displayMode) {
        mThemeColorProvider = themeColorProvider;
        mWebAppMinHeaderHeight = webAppHeaderMinHeightFromResources;
        mHeaderDelegate = headerDelegate;
        mDesktopWindowStateManager = desktopWindowStateManager;
        mTabSupplier = tabSupplier;
        mNonDraggableAreasSupplier = nonDraggableAreasSupplier;
        mHeaderButtonHeight = headerButtonHeight;
        mDisplayMode = displayMode;

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

        mModel = model;
        // View should notify us about initial width.
        mModel.set(WebAppHeaderLayoutProperties.WIDTH_CHANGED_CALLBACK, this::onLayoutWidthUpdated);
        mModel.set(
                WebAppHeaderLayoutProperties.VISIBILITY_CHANGED_CALLBACK,
                this::onVisibilityChanged);

        final var appHeaderState = desktopWindowStateManager.getAppHeaderState();
        if (appHeaderState != null) {
            onAppHeaderStateChanged(appHeaderState);
        }
        mDesktopWindowStateManager.addObserver(this);

        onThemeColorChanged(mThemeColorProvider.getThemeColor(), false);
        mThemeColorProvider.addThemeColorObserver(this);
    }

    private void onLayoutWidthUpdated(int width) {
        mWidthSupplier.set(width);

        // Update draggable area even if width hasn't changed, because children might've changed.
        updateNonDraggableAreas();
    }

    private void onVisibilityChanged(int visibility) {
        // If the web app header view is GONE, we should update the width to reflect this.
        if (visibility == View.GONE) {
            mWidthSupplier.set(0);
        }
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

        if (mIsFirstAppHeaderStateUpdate && mCurrentHeaderState.isInDesktopWindow()) {
            RecordHistogram.recordEnumeratedHistogram(
                    "CustomTabs.WebAppHeader.DisplayMode2", mDisplayMode, DisplayMode.MAX_VALUE);
            mIsFirstAppHeaderStateUpdate = false;
        }
    }

    /**
     * @return {@link ObservableSupplier} that signal current width of the flexible area in which
     *     the header lays out controls.
     */
    public ObservableSupplier<Integer> getUnoccludedWidthSupplier() {
        return mAppHeaderUnoccludedWidthSupplier;
    }

    private void updatePaddings() {
        if (mCurrentHeaderState == null) return;

        // Some vendors provide caption bar insets as caption bar + status bar
        // insets, to layout properly we need to add status bar insets to the
        // padding.
        int controlsTopOffset = mCurrentHeaderState.getCaptionControlsTopOffset();
        int captionControlsHeight = mCurrentHeaderState.getCaptionControlsHeight();

        if (captionControlsHeight < mHeaderButtonHeight) {
            mButtonBottomInset = mHeaderButtonHeight - captionControlsHeight;
        } else {
            mButtonBottomInset = 0;
        }

        if (mOnButtonBottomInsetChanged != null) {
            mOnButtonBottomInsetChanged.onResult(mButtonBottomInset);
        }

        final var paddings =
                new Rect(
                        mCurrentHeaderState.getLeftPadding(),
                        controlsTopOffset,
                        mCurrentHeaderState.getRightPadding(),
                        0);
        mModel.set(WebAppHeaderLayoutProperties.PADDINGS, paddings);
    }

    /**
     * Sets a callback that will be notified about changes to the bottom inset required to align
     * control button icons with system UI icons.
     *
     * @param onButtonBottomInsetChanged a {@link Callback} that accepts new bottom inset.
     */
    public void setOnButtonBottomInsetChanged(Callback<Integer> onButtonBottomInsetChanged) {
        mOnButtonBottomInsetChanged = onButtonBottomInsetChanged;
        onButtonBottomInsetChanged.onResult(mButtonBottomInset);
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
        mModel.set(
                WebAppHeaderLayoutProperties.NON_DRAGGABLE_AREAS,
                areas == null || areas.isEmpty() ? List.of(EMPTY_NON_DRAGGABLE_AREA) : areas);
    }

    /** Navigates back in the navigation history of the current {@link Tab}. */
    public void goBack() {
        final var tab = mTabSupplier.get();
        if (tab != null && tab.canGoBack()) {
            tab.goBack();
            WebAppHeaderUtils.recordBackButtonEvent(BackEvent.BACK);
        } else {
            WebAppHeaderUtils.recordBackButtonEvent(BackEvent.INVALID);
        }
    }

    /** Records histograms when navigation pop up is shown by long pressing back button */
    public void onNavigationPopupShown() {
        WebAppHeaderUtils.recordBackButtonEvent(BackEvent.NAVIGATION_MENU);
    }

    /**
     * Reloads current visible tab or stops reloading.
     *
     * @param ignoreCache whether to force reload current tab.
     */
    public void refreshTab(boolean ignoreCache) {
        final var tab = mTabSupplier.get();
        if (tab == null) {
            WebAppHeaderUtils.recordReloadButtonEvent(ReloadType.INVALID);
            return;
        }

        if (tab.isLoading()) {
            tab.stopLoading();
            WebAppHeaderUtils.recordReloadButtonEvent(ReloadType.STOP_RELOAD);
        } else if (ignoreCache) {
            tab.reloadIgnoringCache();
            WebAppHeaderUtils.recordReloadButtonEvent(ReloadType.RELOAD_IGNORE_CACHE);
        } else {
            tab.reload();
            WebAppHeaderUtils.recordReloadButtonEvent(ReloadType.RELOAD_FROM_CACHE);
        }
    }

    /**
     * @return true when header is visible, false otherwise.
     */
    public boolean isVisible() {
        return mModel.get(WebAppHeaderLayoutProperties.IS_VISIBLE);
    }

    private int getDefaultMinHeight() {
        if (sMinHeaderHeightForTesting != null) return sMinHeaderHeightForTesting;
        return mWebAppMinHeaderHeight;
    }

    public ObservableSupplierImpl<Integer> getWidthSupplierForTesting() {
        return mWidthSupplier;
    }

    static void setMinHeightForTesting(final int height) {
        sMinHeaderHeightForTesting = height;
        ResettersForTesting.register(() -> sMinHeaderHeightForTesting = null);
    }

    /** Destroys the mediator, existing instance is not usable after this method is called */
    public void destroy() {
        mDesktopWindowStateManager.removeObserver(this);
        mThemeColorProvider.removeThemeColorObserver(this);
        mScrimManager.getScrimVisibilitySupplier().removeObserver(mScrimVisibilityObserver);
    }

    @VisibleForTesting
    Callback<Boolean> getScrimVisibilityObserver() {
        return mScrimVisibilityObserver;
    }

    int getButtonBottomInsetForTesting() {
        return mButtonBottomInset;
    }
}
