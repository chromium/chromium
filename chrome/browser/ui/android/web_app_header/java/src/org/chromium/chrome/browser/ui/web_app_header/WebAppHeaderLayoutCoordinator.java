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
import org.chromium.base.CallbackUtils;
import org.chromium.base.metrics.RecordHistogram;
import org.chromium.base.supplier.NullableObservableSupplier;
import org.chromium.base.supplier.ObservableSuppliers;
import org.chromium.base.supplier.OneshotSupplier;
import org.chromium.base.supplier.SettableNonNullObservableSupplier;
import org.chromium.base.supplier.SupplierUtils;
import org.chromium.blink.mojom.DisplayMode;
import org.chromium.build.annotations.NullMarked;
import org.chromium.build.annotations.Nullable;
import org.chromium.chrome.browser.browser_controls.BrowserControlsStateProvider;
import org.chromium.chrome.browser.browser_controls.BrowserStateBrowserControlsVisibilityDelegate;
import org.chromium.chrome.browser.browserservices.intents.BrowserServicesIntentDataProvider;
import org.chromium.chrome.browser.flags.ChromeFeatureList;
import org.chromium.chrome.browser.profiles.Profile;
import org.chromium.chrome.browser.tab.Tab;
import org.chromium.chrome.browser.tab.TabObserver;
import org.chromium.chrome.browser.tabmodel.IncognitoStateProvider;
import org.chromium.chrome.browser.tabmodel.TabCreator;
import org.chromium.chrome.browser.tabmodel.TabModel;
import org.chromium.chrome.browser.tabmodel.TabModelSelector;
import org.chromium.chrome.browser.theme.ThemeColorProvider;
import org.chromium.chrome.browser.toolbar.back_button.BackButtonCoordinator;
import org.chromium.chrome.browser.toolbar.extensions.ExtensionsToolbarCoordinator;
import org.chromium.chrome.browser.toolbar.menu_button.MenuButtonCoordinator;
import org.chromium.chrome.browser.toolbar.reload_button.ReloadButtonCoordinator;
import org.chromium.chrome.browser.toolbar.top.NavigationPopup;
import org.chromium.chrome.browser.ui.actions.appmenu.MenuButtonState;
import org.chromium.chrome.browser.ui.appmenu.AppMenuCoordinator;
import org.chromium.chrome.browser.ui.browser_window.ChromeAndroidTask;
import org.chromium.chrome.browser.ui.browser_window.ChromeAndroidTaskFeatureKey;
import org.chromium.chrome.browser.ui.theme.BrandedColorScheme;
import org.chromium.chrome.browser.web_app_header.R;
import org.chromium.components.browser_ui.desktop_windowing.AppHeaderState;
import org.chromium.components.browser_ui.desktop_windowing.DesktopWindowStateManager;
import org.chromium.components.browser_ui.widget.animation.CancelAwareAnimatorListener;
import org.chromium.components.browser_ui.widget.scrim.ScrimManager;
import org.chromium.components.embedder_support.util.Origin;
import org.chromium.components.url_formatter.UrlFormatter;
import org.chromium.components.webapps.WebappsUtils;
import org.chromium.content_public.browser.NavigationHandle;
import org.chromium.ui.base.ActivityWindowAndroid;
import org.chromium.ui.base.DeviceFormFactor;
import org.chromium.ui.base.WindowAndroid;
import org.chromium.ui.modaldialog.ModalDialogManager;
import org.chromium.ui.modelutil.PropertyModel;
import org.chromium.ui.modelutil.PropertyModelChangeProcessor;
import org.chromium.ui.util.AttrUtils;
import org.chromium.ui.util.TokenHolder;
import org.chromium.ui.widget.ChromeImageButton;
import org.chromium.url.GURL;

import java.util.ArrayList;
import java.util.Arrays;
import java.util.List;
import java.util.Set;
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
        implements TabObserver,
                DesktopWindowStateManager.AppHeaderObserver,
                WebAppHeaderDelegate,
                BrowserControlsStateProvider.Observer,
                ThemeColorProvider.TintObserver {

    private static final int ANIMATION_START_DELAY_MS = 500;
    private static final int ANIMATION_PAUSE_DELAY_MS = 2500;
    private static final int ANIMATION_DURATION_MS = 800;

    private int mHeaderControlButtonWidthPx;
    private int mHeaderButtonPaddingPx;

    private @Nullable WebAppHeaderLayoutMediator mMediator;
    private @Nullable WebAppHeaderLayout mView;
    private @Nullable ReloadButtonCoordinator mReloadButtonCoordinator;
    private @Nullable BackButtonCoordinator mBackButtonCoordinator;
    private @Nullable MenuButtonCoordinator mMenuButtonCoordinator;
    private final ViewStub mViewStub;
    private final DesktopWindowStateManager mDesktopWindowStateManager;
    private final NullableObservableSupplier<Tab> mTabSupplier;
    private final ScrimManager mScrimManager;
    private final ThemeColorProvider mThemeColorProvider;
    private final IncognitoStateProvider mIncognitoStateProvider;
    private final @DisplayMode.EnumType int mDisplayMode;
    private final NavigationPopup.HistoryDelegate mHistoryDelegate;
    private int mUIControlsMinWidthPx;
    private int mAppHeaderUnoccludedWidthPx;
    private final Callback<Integer> mOnUnoccludedWidthCallback;
    private final SettableNonNullObservableSupplier<Boolean> mControlsEnabledSupplier =
            ObservableSuppliers.createNonNull(true);

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
    private final Supplier<MenuButtonState> mMenuButtonStateSupplier;
    private @Nullable View mMenuButtonContainer;
    private final @Nullable String mClientPackageName;
    private @Nullable ChromeImageButton mToggleButtonView;
    private @Nullable TextView mAppOriginView;
    private @Nullable String mAppOrigin;
    private @Nullable Tab mObservedTab;
    private final Callback<@Nullable Tab> mOnTabUpdate;
    private final BrowserServicesIntentDataProvider mBrowserServicesIntentDataProvider;
    private final OneshotSupplier<ChromeAndroidTask> mChromeAndroidTaskSupplier;
    private final TabModelSelector mTabModelSelector;
    private final TabCreator mTabCreator;
    private final ModalDialogManager mModalDialogManager;
    private @Nullable ExtensionsToolbarCoordinator mExtensionsToolbarCoordinator;
    private boolean mIsDestroyed;

    /**
     * Creates an instance of {@link WebAppHeaderLayoutCoordinator}.
     *
     * @param viewStub a stub in which web app header will be inflated into.
     * @param desktopWindowStateManager a class that notifies about desktop windowing state changes.
     * @param tabCreator a {@link TabCreator} used by the extensions toolbar to open external URLs
     *     (e.g., Chrome Web Store or extension management) in a standard browser window rather than
     *     inside the web app.
     */
    public WebAppHeaderLayoutCoordinator(
            Activity activity,
            ViewStub viewStub,
            DesktopWindowStateManager desktopWindowStateManager,
            NullableObservableSupplier<Tab> tabSupplier,
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
            @Nullable String clientPackageName,
            OneshotSupplier<ChromeAndroidTask> chromeAndroidTaskSupplier,
            TabModelSelector tabModelSelector,
            TabCreator tabCreator,
            ModalDialogManager modalDialogManager) {
        assert browserServicesIntentDataProvider.isWebApkActivity()
                || browserServicesIntentDataProvider.isTrustedWebActivity();

        mBrowserServicesIntentDataProvider = browserServicesIntentDataProvider;
        mIsTWA = browserServicesIntentDataProvider.isTrustedWebActivity();
        mDisplayMode = browserServicesIntentDataProvider.getResolvedDisplayMode();
        mHistoryDelegate = historyDelegate;
        mDisabledControlsHolder = new TokenHolder(this::updateControlsEnabledState);
        mScrimManager = scrimManager;
        mSetHeaderAsOverlayCallback = setHeaderAsOverlayCallback;
        mChromeAndroidTaskSupplier = chromeAndroidTaskSupplier;
        mTabModelSelector = tabModelSelector;
        mTabCreator = tabCreator;
        mModalDialogManager = modalDialogManager;

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
        mMenuButtonStateSupplier = ObservableSuppliers.createNonNull(buttonState);

        mClientPackageName = clientPackageName;

        mViewStub = viewStub;
        mViewStub.setLayoutResource(R.layout.web_app_header_layout);

        mDesktopWindowStateManager = desktopWindowStateManager;
        mDesktopWindowStateManager.addObserver(this);

        mTabSupplier = tabSupplier;
        mThemeColorProvider = themeColorProvider;
        mThemeColorProvider.addTintObserver(this);
        onTintChanged(
                mThemeColorProvider.getTint(),
                mThemeColorProvider.getActivityFocusTint(),
                mThemeColorProvider.getBrandedColorScheme());
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
        mTabSupplier.addSyncObserverAndPostIfNonNull(mOnTabUpdate);
    }

    @Override
    public void onAppHeaderStateChanged(AppHeaderState newState) {
        ensureInitialized();
    }

    private void onTabUpdate(@Nullable Tab tab) {
        if (mObservedTab == tab) {
            return;
        }
        if (mObservedTab != null) {
            mObservedTab.removeObserver(this);
        }
        mObservedTab = tab;
        if (mObservedTab != null) {
            mObservedTab.addObserver(this);
        }
    }

    private void ensureInitialized() {
        if (mView != null) return;

        mView = (WebAppHeaderLayout) mViewStub.inflate();
        int headerButtonSize =
                AttrUtils.getDimensionPixelSize(mView.getContext(), R.attr.webAppHeaderButtonSize);
        if (headerButtonSize == -1) {
            headerButtonSize =
                    mView.getResources().getDimensionPixelSize(R.dimen.header_button_size);
        }

        mHeaderControlButtonWidthPx = headerButtonSize;
        mHeaderButtonPaddingPx =
                mView.getResources().getDimensionPixelSize(R.dimen.header_button_padding);
        final var model = new PropertyModel.Builder(WebAppHeaderLayoutProperties.ALL_KEYS).build();
        final int headerMinHeight =
                mView.getResources().getDimensionPixelSize(R.dimen.web_app_header_min_height);

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
                        headerButtonSize,
                        mDisplayMode,
                        mSetHeaderAsOverlayCallback,
                        mClientPackageName);
        PropertyModelChangeProcessor.create(model, mView, WebAppHeaderLayoutViewBinder::bind);

        // Initial visibility state must be initialized after mediator is initialized.
        onAndroidControlsVisibilityChanged(
                mBrowserControlsStateProvider.getAndroidControlsVisibility());

        if (mIsTWA) {
            // Show origin for Android large form factors for TWAs.
            if (ChromeFeatureList.sDesktopAndroidTWADisclosures.isEnabled()
                    && DeviceFormFactor.isWindowOnTablet(mActivityWindowAndroid)) {
                mAppOriginView = (TextView) mView.findViewById(R.id.origin);
            } else if (mClientPackageName != null) {
                // Show origin only for TWA Installer installed apps.
                // TODO(crbug.com/545324369): Remove this code once the
                // DESKTOP_ANDROID_TWA_DISCLOSURES feature flag is enabled by default,
                // in which case, we will no longer have to worry about the
                // installer package check.
                WebappsUtils.isTwaInstallerPackage(
                        mClientPackageName,
                        (isTwaInstallerPackage) -> {
                            if (isTwaInstallerPackage) {
                                assert mView != null;
                                mAppOriginView = (TextView) mView.findViewById(R.id.origin);
                            }
                        });
            }
        }

        mMediator
                .getUnoccludedWidthSupplier()
                .addSyncObserverAndPostIfNonNull(mOnUnoccludedWidthCallback);
        if (mDisplayMode == DisplayMode.MINIMAL_UI) {
            initMinUiControls();
        }

        if (mDisplayMode == DisplayMode.WINDOW_CONTROLS_OVERLAY) {
            initWCOControls();
        }

        initMenuButton();

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
                (v, event) -> {
                    if (event.getAction() != MotionEvent.ACTION_UP) return false;
                    assert mMediator != null;
                    mMediator.setUserToggleHeaderAsOverlay(
                            !mMediator.getUserToggleHeaderAsOverlay());
                    syncToggleButtonView();
                    return false;
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
    @SuppressWarnings("SetTextColorAndSetTextSizeCheck")
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
                        ObservableSuppliers.alwaysFalse(),
                        mControlsEnabledSupplier,
                        mThemeColorProvider,
                        mIncognitoStateProvider,
                        /* isWebApp= */ true);

        final ChromeImageButton backButton = mView.findViewById(R.id.back_button);
        mBackButtonCoordinator =
                new BackButtonCoordinator(
                        backButton,
                        (metaState, buttonState) -> {
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

        mMediator.setOnButtonBottomInsetChanged(this::onButtonBottomInsetChanged);
    }

    private void initExtensionsToolbar() {
        assert mExtensionsToolbarCoordinator == null;
        if (!mIsTWA || mView == null) return;

        mChromeAndroidTaskSupplier.onAvailable(
                (task) -> {
                    if (mIsDestroyed || mExtensionsToolbarCoordinator != null || mView == null) {
                        return;
                    }
                    final WebAppHeaderLayout view = mView;
                    ViewStub stub = view.findViewById(R.id.extensions_toolbar_container_stub);
                    if (stub == null) return;

                    TabModel currentModel = mTabModelSelector.getCurrentModel();
                    if (currentModel == null || currentModel.getProfile() == null) return;
                    Profile profile = currentModel.getProfile();

                    Runnable cleanup = () -> mExtensionsToolbarCoordinator = null;
                    mExtensionsToolbarCoordinator =
                            (ExtensionsToolbarCoordinator)
                                    task.addFeature(
                                            new ChromeAndroidTaskFeatureKey(
                                                    ExtensionsToolbarCoordinator.class,
                                                    profile,
                                                    (ActivityWindowAndroid) mActivityWindowAndroid),
                                            () ->
                                                    ExtensionsToolbarCoordinator.maybeCreate(
                                                            mActivity,
                                                            stub,
                                                            mActivityWindowAndroid,
                                                            task,
                                                            profile,
                                                            mTabSupplier,
                                                            mTabCreator,
                                                            mThemeColorProvider,
                                                            view,
                                                            /* contextMenuPopulatorFactory= */ null,
                                                            /* selectionDropdownMenuDelegate= */ null,
                                                            mTabModelSelector,
                                                            mModalDialogManager,
                                                            cleanup,
                                                            /* isWebApp= */ true));
                });
    }

    public @Nullable ExtensionsToolbarCoordinator getExtensionsToolbarCoordinator() {
        return mExtensionsToolbarCoordinator;
    }

    private void initMenuButton() {
        assert mView != null;
        assert mMenuButtonContainer == null;
        assert mMenuButtonCoordinator == null;
        if (!mIsTWA) return;
        if (mDisplayMode == DisplayMode.MINIMAL_UI
                || mDisplayMode == DisplayMode.STANDALONE
                || mDisplayMode == DisplayMode.WINDOW_CONTROLS_OVERLAY) {
            mMenuButtonContainer = mView.findViewById(R.id.web_app_menu_button_wrapper);
            mMenuButtonContainer.setVisibility(View.VISIBLE);

            // TODO(crbug.com/453007852): When ObservableSupplier<E> extends Supplier<@Nullable E>,
            // remove cast to Supplier<@Nullable MenuButtonState>,
            // Pass View.NO_ID to prevent MenuButtonCoordinator from searching mActivity for
            // R.id.menu_button_wrapper, which would incorrectly bind to CustomTabToolbar's
            // MenuButton. Explicitly set the MenuButton view resolved from the header container
            // instead.
            mMenuButtonCoordinator =
                    new MenuButtonCoordinator(
                            mActivity,
                            mAppMenuCoordinatorSupplier,
                            mBrowserStateBrowserControlsVisibilityDelegate,
                            mActivityWindowAndroid,
                            /* clearOmniboxFocus= */ CallbackUtils.emptyRunnable(),
                            mRequestRenderRunnable,
                            /* canShowAppUpdateBadge= */ false,
                            /* isInOverviewModeSupplier= */ SupplierUtils.alwaysFalse(),
                            mThemeColorProvider,
                            mIncognitoStateProvider,
                            (Supplier<@Nullable MenuButtonState>) mMenuButtonStateSupplier,
                            this::onMenuButtonClicked,
                            View.NO_ID,
                            /* visibilityDelegate= */ null,
                            /* isWebApp= */ true);
            mMenuButtonCoordinator.setMenuButton(
                    mMenuButtonContainer.findViewById(R.id.menu_button_wrapper));

            initExtensionsToolbar();
        }
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

        View relativeLayout = mView.findViewById(R.id.web_app_header_relative);
        final var areas = new ArrayList<Rect>();
        if (mReloadButtonCoordinator != null && mReloadButtonCoordinator.isVisible()) {
            Rect rect = mReloadButtonCoordinator.getHitRect();
            mView.offsetDescendantRectToMyCoords(relativeLayout, rect);
            areas.add(rect);
        }

        if (mBackButtonCoordinator != null && mBackButtonCoordinator.isVisible()) {
            Rect rect = mBackButtonCoordinator.getHitRect();
            mView.offsetDescendantRectToMyCoords(relativeLayout, rect);
            areas.add(rect);
        }

        // getHitRect() provides coordinates relative to its parent View. Use
        // offsetDescendantRectToMyCoords() to take ancestor(s) into account.
        View rightAlignedWrapper = mView.findViewById(R.id.right_aligned_wrapper);
        if (mMenuButtonCoordinator != null && mMenuButtonCoordinator.isVisible()) {
            Rect rect = mMenuButtonCoordinator.getHitRect();
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

        View extensionsToolbar = mView.findViewById(R.id.extensions_toolbar_container);
        if (extensionsToolbar != null
                && extensionsToolbar.getVisibility() == View.VISIBLE
                && extensionsToolbar.getWidth() > 0) {
            final var rect = new Rect();
            extensionsToolbar.getHitRect(rect);
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

        int totalWidthPx = 0;
        if (mReloadButtonCoordinator != null) {
            totalWidthPx += mHeaderControlButtonWidthPx;
        }

        if (mBackButtonCoordinator != null) {
            totalWidthPx += mHeaderControlButtonWidthPx;
        }

        if (mMenuButtonCoordinator != null) {
            totalWidthPx += mHeaderControlButtonWidthPx;
        }

        // Add button padding.
        totalWidthPx += mHeaderButtonPaddingPx;

        if (mToggleButtonView != null) {
            // If mToggleButtonView is non-null, we're in WINDOW_CONTROLS_OVERLAY mode. In addition
            // to allowing space for the toggle button, allow a minimal space for the web content
            // in the header.
            totalWidthPx += (mHeaderControlButtonWidthPx * 3);
        }

        if (mAppOriginView != null) {
            totalWidthPx += mAppOriginView.getWidth();
        }

        return totalWidthPx;
    }

    /**
     * @return The header control button width in dp.
     */
    @VisibleForTesting
    int getHeaderControlButtonWidthDp() {
        if (mView == null) return 0;
        float density = mView.getResources().getDisplayMetrics().density;
        return Math.round(mHeaderControlButtonWidthPx / density);
    }

    /**
     * @return The header button padding in dp.
     */
    @VisibleForTesting
    int getHeaderButtonPaddingDp() {
        if (mView == null) return 0;
        float density = mView.getResources().getDisplayMetrics().density;
        return Math.round(mHeaderButtonPaddingPx / density);
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

        if (mAppOriginView != null) {
            mAppOriginView.setPadding(0, 0, 0, bottomInset);
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
        mThemeColorProvider.removeTintObserver(this);

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

        if (mObservedTab != null) {
            mObservedTab.removeObserver(this);
            mObservedTab = null;
        }
        mTabSupplier.removeObserver(mOnTabUpdate);

        if (mExtensionsToolbarCoordinator != null) {
            mExtensionsToolbarCoordinator.destroy();
            mExtensionsToolbarCoordinator = null;
        }

        mIsDestroyed = true;
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
        if (mAppOriginView == null || mBrowserServicesIntentDataProvider == null) {
            return;
        }
        Set<Origin> origins = mBrowserServicesIntentDataProvider.getAllTrustedWebActivityOrigins();
        if (origins == null) {
            return;
        }
        GURL origin = navigationHandle.getUrl().getOrigin();
        String originSpec = origin.getSpec();
        boolean isTWAOrigin = origins.contains(Origin.create(originSpec));
        // If the origin is not new or does not belong to the TWA, do nothing.
        if ((mAppOrigin != null && mAppOrigin.equals(originSpec)) || !isTWAOrigin) {
            return;
        }

        mAppOrigin = originSpec;
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

    // This helps ensure that the presubmit warning to set a pre-defined text appearance
    // no longer occurs, as the origin text is set according to the theme color, and
    // a predefined text appearance style cannot be used in a dynamic context like this.
    // Same for wherever else mAppOriginView.setTextColor() is called in this file.
    @SuppressWarnings("SetTextColorAndSetTextSizeCheck")
    private void setTextThemeColor() {
        if (mAppOriginView == null) return;

        final ColorStateList textColorList =
                mThemeColorProvider.getActivityFocusTint() == null
                        ? mAppOriginView.getTextColors()
                        : mThemeColorProvider.getActivityFocusTint();
        mAppOriginView.setTextColor(textColorList);
    }

    private void onMenuButtonClicked() {
        AppMenuCoordinator appMenuCoordinator = mAppMenuCoordinatorSupplier.get();
        if (appMenuCoordinator == null || mView == null) return;

        View menuButtonView = mView.findViewById(R.id.menu_button_wrapper);
        assert menuButtonView != null;
        // Post the show call to handle keyboard click events and enure the View is laid out
        // appropriately.
        menuButtonView.post(
                () -> {
                    appMenuCoordinator
                            .getAppMenuHandler()
                            .showAppMenu(menuButtonView, /* startDragging= */ false);
                });
    }
}
