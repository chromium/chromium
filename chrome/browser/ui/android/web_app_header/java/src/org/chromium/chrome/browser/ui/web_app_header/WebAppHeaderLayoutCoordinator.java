// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.ui.web_app_header;

import android.app.Activity;
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
import org.chromium.base.supplier.OneshotSupplier;
import org.chromium.blink.mojom.DisplayMode;
import org.chromium.build.annotations.NullMarked;
import org.chromium.build.annotations.Nullable;
import org.chromium.chrome.browser.browser_controls.BrowserControlsStateProvider;
import org.chromium.chrome.browser.browser_controls.BrowserStateBrowserControlsVisibilityDelegate;
import org.chromium.chrome.browser.browserservices.intents.BrowserServicesIntentDataProvider;
import org.chromium.chrome.browser.flags.ChromeFeatureList;
import org.chromium.chrome.browser.tab.Tab;
import org.chromium.chrome.browser.tabmodel.IncognitoStateProvider;
import org.chromium.chrome.browser.theme.ThemeColorProvider;
import org.chromium.chrome.browser.toolbar.back_button.BackButtonCoordinator;
import org.chromium.chrome.browser.toolbar.menu_button.MenuButtonCoordinator;
import org.chromium.chrome.browser.toolbar.menu_button.MenuButtonState;
import org.chromium.chrome.browser.toolbar.reload_button.ReloadButtonCoordinator;
import org.chromium.chrome.browser.toolbar.top.NavigationPopup;
import org.chromium.chrome.browser.ui.appmenu.AppMenuCoordinator;
import org.chromium.chrome.browser.web_app_header.R;
import org.chromium.components.browser_ui.desktop_windowing.AppHeaderState;
import org.chromium.components.browser_ui.desktop_windowing.DesktopWindowStateManager;
import org.chromium.components.browser_ui.widget.scrim.ScrimManager;
import org.chromium.ui.base.WindowAndroid;
import org.chromium.ui.display.DisplayAndroid;
import org.chromium.ui.display.DisplayUtil;
import org.chromium.ui.modelutil.PropertyModel;
import org.chromium.ui.modelutil.PropertyModelChangeProcessor;
import org.chromium.ui.util.TokenHolder;
import org.chromium.ui.widget.ChromeImageButton;

import java.util.ArrayList;
import java.util.List;
import java.util.concurrent.TimeUnit;
import java.util.function.Supplier;

/**
 * Root component to interact with web app header. This coordinator lazily initializes web app
 * header when {@link DesktopWindowStateManager} indicates that the view hierarchy is in the desktop
 * window.
 */
@NullMarked
@RequiresApi(api = Build.VERSION_CODES.VANILLA_ICE_CREAM)
public class WebAppHeaderLayoutCoordinator
        implements DesktopWindowStateManager.AppHeaderObserver,
                WebAppHeaderDelegate,
                BrowserControlsStateProvider.Observer {

    private int mHeaderControlButtonWidthDp;
    private int mHeaderButtonPaddingDp;

    private @Nullable WebAppHeaderLayoutMediator mMediator;
    private @Nullable WebAppHeaderLayout mView;
    private @Nullable ReloadButtonCoordinator mReloadButtonCoordinator;
    private @Nullable BackButtonCoordinator mBackButtonCoordinator;
    private @Nullable MenuButtonCoordinator mMenuButtonCoordinator;
    private final ViewStub mViewStub;
    private final DesktopWindowStateManager mDesktopWindowStateManager;
    private final ObservableSupplier<@Nullable Tab> mTabSupplier;
    private final ScrimManager mScrimManager;
    private final ThemeColorProvider mThemeColorProvider;
    private final IncognitoStateProvider mIncognitoStateProvider;
    private final @DisplayMode.EnumType int mDisplayMode;
    private final NavigationPopup.HistoryDelegate mHistoryDelegate;
    private int mMinUIControlsMinWidthPx;
    private int mAppHeaderUnoccludedWidthPx;
    private final Callback<Integer> mOnUnoccludedWidthCallback;
    private final ObservableSupplierImpl<Boolean> mControlsEnabledSupplier;
    private final TokenHolder mDisabledControlsHolder;
    private boolean mShowButtons;
    private long mLastButtonVisibilityChangeTime;
    private final Callback<Boolean> mSetHeaderAsOverlayCallback;
    private final BrowserControlsStateProvider mBrowserControlsStateProvider;
    private final OneshotSupplier<AppMenuCoordinator> mAppMenuCoordinatorSupplier;
    private final BrowserStateBrowserControlsVisibilityDelegate
            mBrowserStateBrowserControlsVisibilityDelegate;
    private final WindowAndroid mActivityWindowAndroid;
    private final Runnable mRequestRenderRunnable;
    private final Activity mActivity;
    private final boolean mIsTWA;
    private final ObservableSupplierImpl<MenuButtonState> mMenuButtonStateSupplier =
            new ObservableSupplierImpl<>();
    private @Nullable View mMenuButtonContainer;

    /**
     * Creates an instance of {@link WebAppHeaderLayoutCoordinator}.
     *
     * @param viewStub a stub in which web app header will be inflated into.
     * @param desktopWindowStateManager a class that notifies about desktop windowing state changes.
     */
    public WebAppHeaderLayoutCoordinator(
            Activity activity,
            ViewStub viewStub,
            DesktopWindowStateManager desktopWindowStateManager,
            ObservableSupplier<@Nullable Tab> tabSupplier,
            ThemeColorProvider themeColorProvider,
            BrowserServicesIntentDataProvider browserServicesIntentDataProvider,
            ScrimManager scrimManager,
            NavigationPopup.HistoryDelegate historyDelegate,
            Callback<Boolean> setHeaderAsOverlayCallback,
            BrowserControlsStateProvider browserControlsStateProvider,
            OneshotSupplier<AppMenuCoordinator> appMenuCoordinatorSupplier,
            BrowserStateBrowserControlsVisibilityDelegate
                    browserStateBrowserControlsVisibilityDelegate,
            WindowAndroid activityWindowAndroid,
            Runnable requestRenderRunnable) {
        assert browserServicesIntentDataProvider.isWebApkActivity()
                || browserServicesIntentDataProvider.isTrustedWebActivity();

        mIsTWA = browserServicesIntentDataProvider.isTrustedWebActivity();
        mDisplayMode = browserServicesIntentDataProvider.getResolvedDisplayMode();
        mHistoryDelegate = historyDelegate;
        mControlsEnabledSupplier = new ObservableSupplierImpl<>(true);
        mDisabledControlsHolder = new TokenHolder(this::updateControlsEnabledState);
        mScrimManager = scrimManager;
        mSetHeaderAsOverlayCallback = setHeaderAsOverlayCallback;

        mBrowserControlsStateProvider = browserControlsStateProvider;
        mBrowserControlsStateProvider.addObserver(this);

        // MenuButtonCoordinator
        mAppMenuCoordinatorSupplier = appMenuCoordinatorSupplier;
        mBrowserStateBrowserControlsVisibilityDelegate =
                browserStateBrowserControlsVisibilityDelegate;
        mActivityWindowAndroid = activityWindowAndroid;
        mRequestRenderRunnable = requestRenderRunnable;
        mActivity = activity;
        MenuButtonState buttonState = new MenuButtonState();
        buttonState.menuContentDescription = R.string.accessibility_toolbar_btn_menu_update;
        buttonState.darkBadgeIcon = R.drawable.badge_update_dark;
        buttonState.lightBadgeIcon = R.drawable.badge_update_light;
        buttonState.adaptiveBadgeIcon = R.drawable.badge_update;
        mMenuButtonStateSupplier.set(buttonState);

        mViewStub = viewStub;
        mViewStub.setLayoutResource(R.layout.web_app_header_layout);

        mDesktopWindowStateManager = desktopWindowStateManager;
        mDesktopWindowStateManager.addObserver(this);

        mTabSupplier = tabSupplier;
        mThemeColorProvider = themeColorProvider;
        mIncognitoStateProvider = new IncognitoStateProvider();

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
        mHeaderControlButtonWidthDp =
                mView.getResources().getDimensionPixelSize(R.dimen.header_button_width);
        mHeaderButtonPaddingDp =
                mView.getResources().getDimensionPixelSize(R.dimen.header_button_padding);
        final var model = new PropertyModel.Builder(WebAppHeaderLayoutProperties.ALL_KEYS).build();
        final int headerMinHeight =
                mView.getResources().getDimensionPixelSize(R.dimen.web_app_header_min_height);
        final int headerButtonHeight =
                mView.getResources().getDimensionPixelSize(R.dimen.header_button_height);

        mMediator =
                new WebAppHeaderLayoutMediator(
                        model,
                        this,
                        mDesktopWindowStateManager,
                        mScrimManager,
                        mTabSupplier,
                        this::collectControlPositions,
                        mThemeColorProvider,
                        headerMinHeight,
                        headerButtonHeight,
                        mDisplayMode,
                        mSetHeaderAsOverlayCallback);
        PropertyModelChangeProcessor.create(model, mView, WebAppHeaderLayoutViewBinder::bind);

        // Initial visibility state must be initialized after mediator is initialized.
        onAndroidControlsVisibilityChanged(
                mBrowserControlsStateProvider.getAndroidControlsVisibility());

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
                        mIncognitoStateProvider,
                        /* isWebApp= */ true);

        final ChromeImageButton backButton = mView.findViewById(R.id.back_button);
        mBackButtonCoordinator =
                new BackButtonCoordinator(
                        backButton,
                        (ignored) -> {
                            if (mMediator != null) mMediator.goBack();
                        },
                        mThemeColorProvider,
                        mIncognitoStateProvider,
                        mTabSupplier,
                        mControlsEnabledSupplier,
                        () -> {
                            if (mMediator != null) mMediator.onNavigationPopupShown();
                        },
                        mHistoryDelegate,
                        /* isWebApp= */ true);

        if (mIsTWA && ChromeFeatureList.sAndroidWebAppMenuButton.isEnabled()) {
            mMenuButtonContainer = mView.findViewById(R.id.web_app_menu_button_wrapper);
            mMenuButtonContainer.setVisibility(View.VISIBLE);

            // TODO(crbug.com/453007852): When ObservableSupplier<E> extends Supplier<@Nullable E>,
            // remove cast to Supplier<@Nullable MenuButtonState>,
            mMenuButtonCoordinator =
                    new MenuButtonCoordinator(
                            mActivity,
                            mAppMenuCoordinatorSupplier,
                            mBrowserStateBrowserControlsVisibilityDelegate,
                            mActivityWindowAndroid,
                            /* setUrlBarFocusFunction= */ (should, reason) -> {},
                            mRequestRenderRunnable,
                            /* canShowAppUpdateBadge= */ false,
                            /* isInOverviewModeSupplier= */ () -> false,
                            mThemeColorProvider,
                            mIncognitoStateProvider,
                            (Supplier<@Nullable MenuButtonState>) mMenuButtonStateSupplier,
                            /* onMenuButtonClicked= */ () -> {},
                            R.id.menu_button_wrapper,
                            /* visibilityDelegate= */ null);
        }
        // Determine width of initialized minUI controls.
        mMinUIControlsMinWidthPx = getControlButtonsWidthPx();
        mMediator.setOnButtonBottomInsetChanged(this::onButtonBottomInsetChanged);
    }

    private void onUnoccludedWidthChanged(int newUnoccludedWidthPx) {
        boolean wasShowingButtons = mShowButtons;
        mAppHeaderUnoccludedWidthPx = newUnoccludedWidthPx;
        mShowButtons = mAppHeaderUnoccludedWidthPx >= mMinUIControlsMinWidthPx;

        if (wasShowingButtons == mShowButtons) return;

        if (mReloadButtonCoordinator != null) {
            mReloadButtonCoordinator.setVisibility(mShowButtons);
        }
        if (mBackButtonCoordinator != null) {
            mBackButtonCoordinator.setVisibility(mShowButtons);
        }
        if (mMenuButtonCoordinator != null) {
            mMenuButtonCoordinator.setVisibility(mShowButtons);
            if (mMenuButtonContainer != null) {
                mMenuButtonContainer.setVisibility(mShowButtons ? View.VISIBLE : View.GONE);
            }
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
    List<Rect> collectControlPositions() {
        final var areas = new ArrayList<Rect>();
        if (mReloadButtonCoordinator != null && mReloadButtonCoordinator.isVisible()) {
            areas.add(mReloadButtonCoordinator.getHitRect());
        }

        if (mBackButtonCoordinator != null && mBackButtonCoordinator.isVisible()) {
            areas.add(mBackButtonCoordinator.getHitRect());
        }

        if (mMenuButtonCoordinator != null && mMenuButtonCoordinator.isVisible()) {
            assert mView != null;
            Rect rect = mMenuButtonCoordinator.getHitRect();
            View menuDescendent = mView.findViewById(R.id.menu_button_wrapper);
            mView.offsetDescendantRectToMyCoords(menuDescendent, rect);
            areas.add(rect);
        }

        return areas;
    }

    /**
     * @return The total width of the initialized controls in px.
     */
    @VisibleForTesting
    int getControlButtonsWidthPx() {
        if (mView == null) return 0;

        int totalWidthDp = 0;
        if (mReloadButtonCoordinator != null) {
            totalWidthDp += mHeaderControlButtonWidthDp;
        }

        if (mBackButtonCoordinator != null) {
            totalWidthDp += mHeaderControlButtonWidthDp;
        }

        if (mMenuButtonCoordinator != null) {
            totalWidthDp += mHeaderControlButtonWidthDp;
        }

        // Add button padding.
        totalWidthDp += mHeaderButtonPaddingDp;

        int totalWidthPx =
                DisplayUtil.dpToPx(
                        DisplayAndroid.getNonMultiDisplay(mView.getContext()), totalWidthDp);

        return totalWidthPx;
    }

    /**
     * @return The header control button width in dp.
     */
    @VisibleForTesting
    int getHeaderControlButtonWidthDp() {
        return mHeaderControlButtonWidthDp;
    }

    /**
     * @return The header button padding in dp.
     */
    @VisibleForTesting
    int getHeaderButtonPaddingDp() {
        return mHeaderButtonPaddingDp;
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

    /**
     * @return true when back&refresh buttons are visible, false otherwise.
     */
    public boolean isShowingButtons() {
        return mShowButtons;
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
        mBrowserControlsStateProvider.removeObserver(this);

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

        if (mMenuButtonCoordinator != null) {
            mMenuButtonCoordinator.destroy();
            mMenuButtonCoordinator = null;
        }
    }

    @VisibleForTesting
    public @Nullable View getWebAppHeaderLayout() {
        return mView;
    }

    @Override
    public void onAndroidControlsVisibilityChanged(int visibility) {
        if (mMediator == null) return;
        mMediator.setBrowserControlsVisible(visibility == View.VISIBLE);
    }
}
