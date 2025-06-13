// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.ui.web_app_header;

import android.graphics.Rect;
import android.os.Build;
import android.os.SystemClock;
import android.view.View;
import android.view.ViewStub;
import android.widget.ImageButton;

import androidx.annotation.RequiresApi;
import androidx.annotation.VisibleForTesting;
import androidx.core.graphics.Insets;

import org.chromium.base.Callback;
import org.chromium.base.metrics.RecordHistogram;
import org.chromium.base.supplier.ObservableSupplier;
import org.chromium.base.supplier.ObservableSupplierImpl;
import org.chromium.blink.mojom.DisplayMode;
import org.chromium.build.annotations.NullMarked;
import org.chromium.build.annotations.Nullable;
import org.chromium.chrome.browser.browserservices.intents.BrowserServicesIntentDataProvider;
import org.chromium.chrome.browser.tab.Tab;
import org.chromium.chrome.browser.theme.ThemeColorProvider;
import org.chromium.chrome.browser.toolbar.back_button.BackButtonCoordinator;
import org.chromium.chrome.browser.toolbar.reload_button.ReloadButtonCoordinator;
import org.chromium.chrome.browser.toolbar.top.NavigationPopup;
import org.chromium.chrome.browser.web_app_header.R;
import org.chromium.components.browser_ui.desktop_windowing.AppHeaderState;
import org.chromium.components.browser_ui.desktop_windowing.DesktopWindowStateManager;
import org.chromium.components.browser_ui.widget.scrim.ScrimManager;
import org.chromium.ui.display.DisplayAndroid;
import org.chromium.ui.display.DisplayUtil;
import org.chromium.ui.modelutil.PropertyModel;
import org.chromium.ui.modelutil.PropertyModelChangeProcessor;
import org.chromium.ui.util.TokenHolder;
import org.chromium.ui.widget.ChromeImageButton;

import java.util.ArrayList;
import java.util.List;
import java.util.concurrent.TimeUnit;

/**
 * Root component to interact with web app header. This coordinator lazily initializes web app
 * header when {@link DesktopWindowStateManager} indicates that the view hierarchy is in the desktop
 * window.
 */
@NullMarked
@RequiresApi(api = Build.VERSION_CODES.VANILLA_ICE_CREAM)
public class WebAppHeaderLayoutCoordinator
        implements DesktopWindowStateManager.AppHeaderObserver, WebAppHeaderDelegate {

    // 48dp * 2 (back and reload button) + 4dp (start padding).
    static final int MIN_HEADER_WIDTH_DP = 100;

    private @Nullable WebAppHeaderLayoutMediator mMediator;
    private @Nullable WebAppHeaderLayout mView;
    private @Nullable ReloadButtonCoordinator mReloadButtonCoordinator;
    private @Nullable BackButtonCoordinator mBackButtonCoordinator;
    private final ViewStub mViewStub;
    private final DesktopWindowStateManager mDesktopWindowStateManager;
    private final ObservableSupplier<@Nullable Tab> mTabSupplier;
    private final ScrimManager mScrimManager;
    private final ThemeColorProvider mThemeColorProvider;
    private final @DisplayMode.EnumType int mDisplayMode;
    private final NavigationPopup.HistoryDelegate mHistoryDelegate;
    private int mMinUIControlsMinWidthPx;
    private int mAppHeaderUnoccludedWidthPx;
    private final Callback<Integer> mOnUnoccludedWidthCallback;
    private final ObservableSupplierImpl<Boolean> mControlsEnabledSupplier;
    private final TokenHolder mDisabledControlsHolder;
    private long mLastButtonVisibilityChangeTime;

    /**
     * Creates an instance of {@link WebAppHeaderLayoutCoordinator}.
     *
     * @param viewStub a stub in which web app header will be inflated into.
     * @param desktopWindowStateManager a class that notifies about desktop windowing state changes.
     */
    public WebAppHeaderLayoutCoordinator(
            ViewStub viewStub,
            DesktopWindowStateManager desktopWindowStateManager,
            ObservableSupplier<@Nullable Tab> tabSupplier,
            ThemeColorProvider themeColorProvider,
            BrowserServicesIntentDataProvider browserServicesIntentDataProvider,
            ScrimManager scrimManager,
            NavigationPopup.HistoryDelegate historyDelegate) {
        assert browserServicesIntentDataProvider.isWebApkActivity()
                || browserServicesIntentDataProvider.isTrustedWebActivity();

        mDisplayMode = browserServicesIntentDataProvider.getResolvedDisplayMode();
        mHistoryDelegate = historyDelegate;
        mControlsEnabledSupplier = new ObservableSupplierImpl<>(true);
        mDisabledControlsHolder = new TokenHolder(this::updateControlsEnabledState);
        mScrimManager = scrimManager;

        mViewStub = viewStub;
        mViewStub.setLayoutResource(R.layout.web_app_header_layout);

        mDesktopWindowStateManager = desktopWindowStateManager;
        mDesktopWindowStateManager.addObserver(this);

        mTabSupplier = tabSupplier;
        mThemeColorProvider = themeColorProvider;

        mOnUnoccludedWidthCallback = this::onUnoccludedWidthChanged;
        mMinUIControlsMinWidthPx = 0;
        mAppHeaderUnoccludedWidthPx = 0;
        mLastButtonVisibilityChangeTime = 0;

        final var appHeaderState = desktopWindowStateManager.getAppHeaderState();
        if (appHeaderState != null) {
            onAppHeaderStateChanged(appHeaderState);
        }
    }

    @Override
    public void onAppHeaderStateChanged(AppHeaderState newState) {
        ensureInitialized();
    }

    private void ensureInitialized() {
        if (mView != null) return;

        mView = (WebAppHeaderLayout) mViewStub.inflate();
        final var model = new PropertyModel.Builder(WebAppHeaderLayoutProperties.ALL_KEYS).build();
        final int headerMinHeight =
                mView.getResources().getDimensionPixelSize(R.dimen.web_app_header_min_height);
        final int headerButtonHeight =
                mView.getResources().getDimensionPixelSize(R.dimen.header_button_height);

        mMinUIControlsMinWidthPx =
                DisplayUtil.dpToPx(
                        DisplayAndroid.getNonMultiDisplay(mView.getContext()), MIN_HEADER_WIDTH_DP);
        mMediator =
                new WebAppHeaderLayoutMediator(
                        model,
                        this,
                        mDesktopWindowStateManager,
                        mScrimManager,
                        mTabSupplier,
                        this::collectNonDraggableAreas,
                        mThemeColorProvider,
                        headerMinHeight,
                        headerButtonHeight,
                        mDisplayMode);
        PropertyModelChangeProcessor.create(model, mView, WebAppHeaderLayoutViewBinder::bind);

        mMediator.getUnoccludedWidthSupplier().addObserver(mOnUnoccludedWidthCallback);
        if (mDisplayMode == DisplayMode.MINIMAL_UI) {
            initMinUiControls();
        }
    }

    private void initMinUiControls() {
        assert mView != null;
        assert mMediator != null;

        final ImageButton reloadButton = mView.findViewById(R.id.refresh_button);
        mReloadButtonCoordinator =
                new ReloadButtonCoordinator(
                        reloadButton,
                        (ignoreCache) -> {
                            if (mMediator != null) mMediator.refreshTab(ignoreCache);
                        },
                        mTabSupplier,
                        new ObservableSupplierImpl<>(),
                        mControlsEnabledSupplier,
                        mThemeColorProvider,
                        /* isWebApp= */ true);

        final ChromeImageButton backButton = mView.findViewById(R.id.back_button);
        mBackButtonCoordinator =
                new BackButtonCoordinator(
                        backButton,
                        (ignored) -> {
                            if (mMediator != null) mMediator.goBack();
                        },
                        mThemeColorProvider,
                        mTabSupplier,
                        mControlsEnabledSupplier,
                        () -> {
                            if (mMediator != null) mMediator.onNavigationPopupShown();
                        },
                        mHistoryDelegate,
                        /* isWebApp= */ true);

        mMediator.setOnButtonBottomInsetChanged(this::onButtonBottomInsetChanged);
    }

    private void onUnoccludedWidthChanged(int newUnoccludedWidthPx) {
        boolean wasShowingButtons = mAppHeaderUnoccludedWidthPx >= mMinUIControlsMinWidthPx;
        mAppHeaderUnoccludedWidthPx = newUnoccludedWidthPx;
        boolean showButtons = mAppHeaderUnoccludedWidthPx >= mMinUIControlsMinWidthPx;

        if (wasShowingButtons == showButtons) return;

        if (mReloadButtonCoordinator != null) {
            mReloadButtonCoordinator.setVisibility(showButtons);
        }
        if (mBackButtonCoordinator != null) {
            mBackButtonCoordinator.setVisibility(showButtons);
        }
        logControlsVisibilityChange(wasShowingButtons);
    }

    private void logControlsVisibilityChange(boolean wasShowingButtons) {
        if (mLastButtonVisibilityChangeTime != 0) {
            long duration =
                    TimeUnit.MILLISECONDS.toSeconds(
                            SystemClock.elapsedRealtime() - mLastButtonVisibilityChangeTime);
            if (wasShowingButtons) {
                RecordHistogram.recordLongTimesHistogram(
                        "CustomTabs.WebAppHeader.ControlsShownTime2", duration);
            } else {
                RecordHistogram.recordLongTimesHistogram(
                        "CustomTabs.WebAppHeader.ControlsHiddenTime2", duration);
            }
        }
        mLastButtonVisibilityChangeTime = SystemClock.elapsedRealtime();
    }

    private void updateControlsEnabledState() {
        mControlsEnabledSupplier.set(!mDisabledControlsHolder.hasTokens());
    }

    @VisibleForTesting
    List<Rect> collectNonDraggableAreas() {
        final var areas = new ArrayList<Rect>();
        if (mReloadButtonCoordinator != null && mReloadButtonCoordinator.isVisibile()) {
            areas.add(mReloadButtonCoordinator.getHitRect());
        }

        if (mBackButtonCoordinator != null && mBackButtonCoordinator.isVisible()) {
            areas.add(mBackButtonCoordinator.getHitRect());
        }

        return areas;
    }

    private void onButtonBottomInsetChanged(int bottomInset) {
        if (mReloadButtonCoordinator != null) {
            mReloadButtonCoordinator.setBackgroundInsets(Insets.of(0, 0, 0, bottomInset));
        }

        if (mBackButtonCoordinator != null) {
            mBackButtonCoordinator.setBackgroundInsets(Insets.of(0, 0, 0, bottomInset));
        }
    }

    /**
     * @return true when header is visible, false otherwise.
     */
    public boolean isVisible() {
        return mMediator != null && mMediator.isVisible();
    }

    @Override
    public int disableControlsAndClearOldToken(int token) {
        int newToken = mDisabledControlsHolder.acquireToken();
        releaseDisabledControlsToken(token);
        return newToken;
    }

    @Override
    public void releaseDisabledControlsToken(int token) {
        mDisabledControlsHolder.releaseToken(token);
    }

    /**
     * Cleans up resources and subscriptions. This class should not be used after this method is
     * called.
     */
    public void destroy() {
        logControlsVisibilityChange(mAppHeaderUnoccludedWidthPx >= mMinUIControlsMinWidthPx);

        mDesktopWindowStateManager.removeObserver(this);

        if (mView != null) {
            mView.destroy();
        }

        if (mMediator != null) {
            mMediator.getUnoccludedWidthSupplier().removeObserver(mOnUnoccludedWidthCallback);
            mMediator.destroy();
        }

        if (mBackButtonCoordinator != null) {
            mBackButtonCoordinator.destroy();
            mBackButtonCoordinator = null;
        }

        if (mReloadButtonCoordinator != null) {
            mReloadButtonCoordinator.destroy();
            mReloadButtonCoordinator = null;
        }
    }

    @VisibleForTesting
    public @Nullable View getWebAppHeaderLayout() {
        return mView;
    }
}
