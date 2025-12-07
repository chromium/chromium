// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.ui.web_app_header;

import android.animation.Animator;
import android.animation.AnimatorSet;
import android.animation.ObjectAnimator;
import android.annotation.SuppressLint;
import android.app.Activity;
import android.content.res.ColorStateList;
import android.content.res.Resources;
import android.graphics.Rect;
import android.os.Build;
import android.os.SystemClock;
import android.view.MotionEvent;
import android.view.View;
import android.view.ViewStub;
import android.widget.ImageButton;
import android.widget.TextView;

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
import org.chromium.chrome.browser.tab.EmptyTabObserver;
import org.chromium.chrome.browser.tab.Tab;
import org.chromium.chrome.browser.tabmodel.IncognitoStateProvider;
import org.chromium.chrome.browser.theme.ThemeColorProvider;
import org.chromium.chrome.browser.toolbar.back_button.BackButtonCoordinator;
import org.chromium.chrome.browser.toolbar.menu_button.MenuButtonCoordinator;
import org.chromium.chrome.browser.toolbar.menu_button.MenuButtonState;
import org.chromium.chrome.browser.toolbar.reload_button.ReloadButtonCoordinator;
import org.chromium.chrome.browser.toolbar.top.NavigationPopup;
import org.chromium.chrome.browser.ui.appmenu.AppMenuCoordinator;
import org.chromium.chrome.browser.ui.theme.BrandedColorScheme;
import org.chromium.chrome.browser.web_app_header.R;
import org.chromium.components.browser_ui.desktop_windowing.AppHeaderState;
import org.chromium.components.browser_ui.desktop_windowing.DesktopWindowStateManager;
import org.chromium.components.browser_ui.widget.animation.CancelAwareAnimatorListener;
import org.chromium.components.browser_ui.widget.scrim.ScrimManager;
import org.chromium.components.embedder_support.util.Origin;
import org.chromium.components.url_formatter.UrlFormatter;
import org.chromium.content_public.browser.NavigationHandle;
import org.chromium.ui.base.WindowAndroid;
import org.chromium.ui.display.DisplayAndroid;
import org.chromium.ui.display.DisplayUtil;
import org.chromium.ui.modelutil.PropertyModel;
import org.chromium.ui.modelutil.PropertyModelChangeProcessor;
import org.chromium.ui.util.TokenHolder;
import org.chromium.ui.widget.ChromeImageButton;
import org.chromium.url.GURL;

import java.util.ArrayList;
import java.util.Arrays;
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
public class WebAppHeaderLayoutCoordinator extends EmptyTabObserver
        implements DesktopWindowStateManager.AppHeaderObserver,
                WebAppHeaderDelegate,
                BrowserControlsStateProvider.Observer,
                ThemeColorProvider.TintObserver {

    private static final int ANIMATION_START_DELAY_MS = 500;
    private static final int ANIMATION_PAUSE_DELAY_MS = 2500;
    private static final int ANIMATION_DURATION_MS = 800;

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
    private int mUIControlsMinWidthPx;
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
    private final @Nullable String mClientPackageName;
    private @Nullable ChromeImageButton mToggleButtonView;
    private @Nullable TextView mAppOriginView;
    private @Nullable String mAppOrigin;
    private final Callback<@Nullable Tab> mOnTabUpdate;
    private final BrowserServicesIntentDataProvider mBrowserServicesIntentDataProvider;

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
            Runnable requestRenderRunnable,
            @Nullable String clientPackageName) {
        assert browserServicesIntentDataProvider.isWebApkActivity()
                || browserServicesIntentDataProvider.isTrustedWebActivity();

        mBrowserServicesIntentDataProvider = browserServicesIntentDataProvider;
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

        mClientPackageName = clientPackageName;

        mViewStub = viewStub;
        mViewStub.setLayoutResource(R.layout.web_app_header_layout);

        mDesktopWindowStateManager = desktopWindowStateManager;
        mDesktopWindowStateManager.addObserver(this);

        mTabSupplier = tabSupplier;
        mThemeColorProvider = themeColorProvider;
        mIncognitoStateProvider = new IncognitoStateProvider();

        mOnUnoccludedWidthCallback = this::onUnoccludedWidthChanged;
        mUIControlsMinWidthPx = 0;
        mAppHeaderUnoccludedWidthPx = 0;
        mLastButtonVisibilityChangeTime = 0;

        final var appHeaderState = desktopWindowStateManager.getAppHeaderState();
        if (appHeaderState != null) {
            onAppHeaderStateChanged(appHeaderState);
        }

        mOnTabUpdate = this::onTabUpdate;
        mTabSupplier.addObserver(mOnTabUpdate);
    }

    @Override
    public void onAppHeaderStateChanged(AppHeaderState newState) {
        ensureInitialized();
    }

    private void onTabUpdate(@Nullable Tab tab) {
        if (tab != null) {
            tab.addObserver(this);
        }
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
                        mSetHeaderAsOverlayCallback,
                        mClientPackageName);
        PropertyModelChangeProcessor.create(model, mView, WebAppHeaderLayoutViewBinder::bind);

        // Initial visibility state must be initialized after mediator is initialized.
        onAndroidControlsVisibilityChanged(
                mBrowserControlsStateProvider.getAndroidControlsVisibility());

        if (mIsTWA && ChromeFeatureList.sAndroidTwaOriginDisplay.isEnabled()) {
            mAppOriginView = (TextView) mView.findViewById(R.id.origin);
        }

        mMediator.getUnoccludedWidthSupplier().addObserver(mOnUnoccludedWidthCallback);
        if (mDisplayMode == DisplayMode.MINIMAL_UI) {
            initMinUiControls();
        }

        if (mDisplayMode == DisplayMode.WINDOW_CONTROLS_OVERLAY) {
            initWCOControls();
        }

        // Determine width of initialized UI controls.
        mUIControlsMinWidthPx = calculateUIControlsMinWidth();
    }

    @SuppressLint("ClickableViewAccessibility")
    private void initWCOControls() {
        assert mView != null;
        assert mMediator != null;

        mToggleButtonView = mView.findViewById(R.id.wco_toggle_button);
        mToggleButtonView.setVisibility(View.VISIBLE);
        syncToggleButtonView();
        mToggleButtonView.setOnTouchListener(
                new View.OnTouchListener() {
                    @SuppressLint("ClickableViewAccessibility")
                    @Override
                    public boolean onTouch(View v, MotionEvent event) {
                        if (event.getAction() == MotionEvent.ACTION_DOWN) return false;
                        assert mMediator != null;
                        mMediator.setUserToggleHeaderAsOverlay(
                                !mMediator.getUserToggleHeaderAsOverlay());
                        syncToggleButtonView();
                        return false;
                    }
                });
        mToggleButtonView.setForegroundTintList(mThemeColorProvider.getTint());

        final ColorStateList iconColorList =
                mThemeColorProvider.getActivityFocusTint() == null
                        ? mToggleButtonView.getImageTintList()
                        : mThemeColorProvider.getActivityFocusTint();
        mToggleButtonView.setImageTintList(iconColorList);
    }

    private void syncToggleButtonView() {
        assert mView != null;
        assert mMediator != null;
        assert mToggleButtonView != null;

        Resources resources = mView.getContext().getResources();
        int level =
                mMediator.getUserToggleHeaderAsOverlay()
                        ? resources.getInteger(
                                R.integer.window_controls_overlay_toggle_level_disable)
                        : resources.getInteger(
                                R.integer.window_controls_overlay_toggle_level_enable);
        mToggleButtonView.getDrawable().setLevel(level);
        mToggleButtonView.setContentDescription(
                mMediator.getUserToggleHeaderAsOverlay()
                        ? mView.getContext()
                                .getString(R.string.web_app_disable_window_controls_overlay_tooltip)
                        : mView.getContext()
                                .getString(
                                        R.string.web_app_enable_window_controls_overlay_tooltip));
    }

    @Override
    public void onTintChanged(
            @Nullable ColorStateList tint,
            @Nullable ColorStateList activityFocusTint,
            @BrandedColorScheme int brandedColorScheme) {
        if (mToggleButtonView != null) {
            mToggleButtonView.setImageTintList(activityFocusTint);
        }
        if (mAppOriginView != null) {
            mAppOriginView.setTextColor(activityFocusTint);
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
                            /* visibilityDelegate= */ null,
                            /* isWebApp= */ true);
        }
        mMediator.setOnButtonBottomInsetChanged(this::onButtonBottomInsetChanged);
    }

    private void onUnoccludedWidthChanged(int newUnoccludedWidthPx) {
        boolean wasShowingButtons = mShowButtons;
        mAppHeaderUnoccludedWidthPx = newUnoccludedWidthPx;
        mShowButtons = mAppHeaderUnoccludedWidthPx >= mUIControlsMinWidthPx;

        if (wasShowingButtons == mShowButtons) return;

        int visibility = mShowButtons ? View.VISIBLE : View.GONE;

        if (mReloadButtonCoordinator != null) {
            mReloadButtonCoordinator.setVisibility(mShowButtons);
        }
        if (mBackButtonCoordinator != null) {
            mBackButtonCoordinator.setVisibility(mShowButtons);
        }
        if (mMenuButtonCoordinator != null) {
            mMenuButtonCoordinator.setVisibility(mShowButtons);
            if (mMenuButtonContainer != null) {
                mMenuButtonContainer.setVisibility(visibility);
            }
        }
        if (mToggleButtonView != null) {
            mToggleButtonView.setVisibility(visibility);
            assert mMediator != null;
            mMediator.didChangeToggleButtonVisiblity(mShowButtons);
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
        assert mView != null;

        final var areas = new ArrayList<Rect>();
        if (mReloadButtonCoordinator != null && mReloadButtonCoordinator.isVisible()) {
            areas.add(mReloadButtonCoordinator.getHitRect());
        }

        if (mBackButtonCoordinator != null && mBackButtonCoordinator.isVisible()) {
            areas.add(mBackButtonCoordinator.getHitRect());
        }

        // getHitRect() provides coordinates relative to its parent View. Use
        // offsetDescendantRectToMyCoords() to take ancestor(s) into account.
        View rightAlignedWrapper = mView.findViewById(R.id.right_aligned_wrapper);
        if (mMenuButtonCoordinator != null && mMenuButtonCoordinator.isVisible()) {
            Rect rect = mMenuButtonCoordinator.getHitRect();
            View menuDescendent = mView.findViewById(R.id.menu_button_wrapper);
            mView.offsetDescendantRectToMyCoords(menuDescendent, rect);
            mView.offsetDescendantRectToMyCoords(rightAlignedWrapper, rect);
            areas.add(rect);
        }

        if (mAppOriginView != null && mAppOriginView.getVisibility() == View.VISIBLE) {
            final var rect = new Rect();
            mAppOriginView.getHitRect(rect);
            mView.offsetDescendantRectToMyCoords(rightAlignedWrapper, rect);
            areas.add(rect);
        }

        if (mToggleButtonView != null && mToggleButtonView.getVisibility() == View.VISIBLE) {
            final var rect = new Rect();
            mToggleButtonView.getHitRect(rect);
            mView.offsetDescendantRectToMyCoords(rightAlignedWrapper, rect);
            areas.add(rect);
        }

        return areas;
    }

    /**
     * @return The total minimum width of the initialized controls in px. This includes display mode
     *     buttons, as well as a space allotment for the header content when in
     *     WINDOW_CONTROLS_OVERLAY mode.
     */
    @VisibleForTesting
    int calculateUIControlsMinWidth() {
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

        if (mAppOriginView != null) {
            totalWidthDp += mAppOriginView.getWidth();
        }

        if (mToggleButtonView != null) {
            // If mToggleButtonView is non-null, we're in WINDOW_CONTROLS_OVERLAY mode. In addition
            // to allowing space for the toggle button, allow a minimal space for the web content
            // in the header.
            totalWidthDp += (mHeaderControlButtonWidthDp * 3);
        }

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

    @VisibleForTesting
    @Nullable ColorStateList getToggleButtonImageTintList() {
        assert mToggleButtonView != null;
        return mToggleButtonView.getImageTintList();
    }

    private void onButtonBottomInsetChanged(int bottomInset) {
        if (mReloadButtonCoordinator != null) {
            mReloadButtonCoordinator.setBackgroundInsets(Insets.of(0, 0, 0, bottomInset));
        }

        if (mBackButtonCoordinator != null) {
            mBackButtonCoordinator.setBackgroundInsets(Insets.of(0, 0, 0, bottomInset));
        }

        if (mMenuButtonCoordinator != null) {
            mMenuButtonCoordinator.setBackgroundInsets(Insets.of(0, 0, 0, bottomInset));
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
        logControlsVisibilityChange(mAppHeaderUnoccludedWidthPx >= mUIControlsMinWidthPx);

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

        final var tab = mTabSupplier.get();
        if (tab != null) {
            tab.removeObserver(this);
        }
        mTabSupplier.removeObserver(mOnTabUpdate);
    }

    @VisibleForTesting
    public @Nullable View getWebAppHeaderLayout() {
        return mView;
    }

    @Override
    public void onAndroidControlsVisibilityChanged(int visibility) {
        if (mMediator == null) return;
        boolean isVisible = visibility == View.VISIBLE;
        if (mToggleButtonView != null) {
            if (isVisible) {
                mToggleButtonView.setVisibility(View.GONE);
            } else {
                mToggleButtonView.setVisibility(View.VISIBLE);
            }
        }
        mMediator.setBrowserControlsVisible(isVisible);
    }

    @Override
    public void onDidFinishNavigationInPrimaryMainFrame(
            Tab tab, NavigationHandle navigationHandle) {
        if (mAppOriginView == null) return;
        if (mBrowserServicesIntentDataProvider == null
                || mBrowserServicesIntentDataProvider.getAllTrustedWebActivityOrigins() == null)
            return;
        GURL origin = navigationHandle.getUrl().getOrigin();
        boolean isTWAOrigin =
                mBrowserServicesIntentDataProvider
                        .getAllTrustedWebActivityOrigins()
                        .contains(Origin.create(origin.getSpec()));
        // If the origin is not new or does not belong to the TWA, do nothing.
        if ((mAppOrigin != null && mAppOrigin.equals(origin.getSpec())) || !isTWAOrigin) {
            return;
        }

        mAppOrigin = origin.getSpec();
        String domain = UrlFormatter.formatUrlForDisplayOmitSchemePathAndTrivialSubdomains(origin);
        mAppOriginView.setText(domain);
        setTextThemeColor();
        runDomainTextAnimation();
    }

    private void runDomainTextAnimation() {
        if (mAppOriginView == null) return;

        AnimatorSet animationSet = new AnimatorSet();

        animationSet.playSequentially(
                Arrays.asList(
                        animateFadeInView(mAppOriginView), animateFadeOutView(mAppOriginView)));
        animationSet.start();
    }

    private Animator animateFadeInView(View view) {
        ObjectAnimator fadeInAnimation = ObjectAnimator.ofFloat(view, View.ALPHA, 0.0f, 1.0f);
        fadeInAnimation.setDuration(ANIMATION_DURATION_MS);
        fadeInAnimation.setStartDelay(ANIMATION_START_DELAY_MS);
        fadeInAnimation.addListener(
                new CancelAwareAnimatorListener() {
                    @Override
                    public void onStart(Animator animation) {
                        view.setVisibility(View.VISIBLE);
                    }
                });
        return fadeInAnimation;
    }

    private Animator animateFadeOutView(View view) {
        ObjectAnimator fadeOutAnimation = ObjectAnimator.ofFloat(view, View.ALPHA, 1.0f, 0.0f);
        fadeOutAnimation.setDuration(ANIMATION_DURATION_MS);
        // Pause before fade out.
        fadeOutAnimation.setStartDelay(ANIMATION_PAUSE_DELAY_MS);
        fadeOutAnimation.addListener(
                new CancelAwareAnimatorListener() {
                    @Override
                    public void onEnd(Animator animation) {
                        view.setVisibility(View.GONE);
                    }
                });
        return fadeOutAnimation;
    }

    private void setTextThemeColor() {
        if (mAppOriginView == null) return;

        final ColorStateList textColorList =
                mThemeColorProvider.getActivityFocusTint() == null
                        ? mAppOriginView.getTextColors()
                        : mThemeColorProvider.getActivityFocusTint();
        mAppOriginView.setTextColor(textColorList);
    }
}
