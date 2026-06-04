// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.omnibox;

import static org.chromium.build.NullUtil.assertNonNull;
import static org.chromium.build.NullUtil.assumeNonNull;

import android.animation.Animator;
import android.animation.AnimatorListenerAdapter;
import android.animation.AnimatorSet;
import android.animation.ObjectAnimator;
import android.annotation.SuppressLint;
import android.app.Activity;
import android.content.ComponentCallbacks;
import android.content.Context;
import android.content.res.ColorStateList;
import android.content.res.Configuration;
import android.graphics.Rect;
import android.text.TextUtils;
import android.util.FloatProperty;
import android.util.Range;
import android.view.KeyEvent;
import android.view.View;
import android.view.View.OnKeyListener;
import android.view.ViewGroup;
import android.view.ViewGroup.MarginLayoutParams;
import android.view.inputmethod.EditorInfo;
import android.widget.TextView;

import androidx.annotation.DrawableRes;
import androidx.annotation.VisibleForTesting;
import androidx.appcompat.content.res.AppCompatResources;
import androidx.constraintlayout.widget.ConstraintSet;

import org.chromium.base.Callback;
import org.chromium.base.CallbackController;
import org.chromium.base.CallbackUtils;
import org.chromium.base.CommandLine;
import org.chromium.base.ObserverList;
import org.chromium.base.TraceEvent;
import org.chromium.base.metrics.RecordHistogram;
import org.chromium.base.metrics.RecordUserAction;
import org.chromium.base.metrics.TimingMetric;
import org.chromium.base.supplier.MonotonicObservableSupplier;
import org.chromium.base.supplier.NonNullObservableSupplier;
import org.chromium.base.supplier.NullableObservableSupplier;
import org.chromium.base.supplier.ObservableSuppliers;
import org.chromium.base.supplier.OneshotSupplier;
import org.chromium.base.supplier.SettableNonNullObservableSupplier;
import org.chromium.base.supplier.SettableNullableObservableSupplier;
import org.chromium.base.task.PostTask;
import org.chromium.base.task.TaskTraits;
import org.chromium.base.ui.KeyboardUtils;
import org.chromium.build.annotations.EnsuresNonNullIf;
import org.chromium.build.annotations.Initializer;
import org.chromium.build.annotations.NullMarked;
import org.chromium.build.annotations.Nullable;
import org.chromium.build.annotations.RequiresNonNull;
import org.chromium.chrome.browser.banners.AppMenuVerbiage;
import org.chromium.chrome.browser.browser_controls.BrowserControlsStateProvider;
import org.chromium.chrome.browser.browser_controls.BrowserControlsStateProvider.ControlsPosition;
import org.chromium.chrome.browser.contextual_tasks.ContextualTasksUtils;
import org.chromium.chrome.browser.device.DeviceClassManager;
import org.chromium.chrome.browser.flags.ChromeFeatureList;
import org.chromium.chrome.browser.flags.ChromeSwitches;
import org.chromium.chrome.browser.layouts.toolbar.ToolbarWidthConsumer;
import org.chromium.chrome.browser.lens.LensController;
import org.chromium.chrome.browser.lens.LensEntryPoint;
import org.chromium.chrome.browser.lens.LensIntentParams;
import org.chromium.chrome.browser.lens.LensMetrics;
import org.chromium.chrome.browser.lens.LensQueryParams;
import org.chromium.chrome.browser.lifecycle.PauseResumeWithNativeObserver;
import org.chromium.chrome.browser.locale.LocaleManager;
import org.chromium.chrome.browser.multiwindow.MultiInstanceOrchestratorFactory;
import org.chromium.chrome.browser.omnibox.LocationBarDataProvider.Observer;
import org.chromium.chrome.browser.omnibox.SearchEngineUtils.SearchBoxHintTextObserver;
import org.chromium.chrome.browser.omnibox.UrlBar.UrlBarDelegate;
import org.chromium.chrome.browser.omnibox.fusebox.FuseboxAttachmentModelList;
import org.chromium.chrome.browser.omnibox.fusebox.FuseboxAttachmentModelList.FuseboxAttachmentChangeListener;
import org.chromium.chrome.browser.omnibox.fusebox.FuseboxCoordinator;
import org.chromium.chrome.browser.omnibox.fusebox.FuseboxCoordinator.FuseboxLayoutMode;
import org.chromium.chrome.browser.omnibox.fusebox.FuseboxCoordinator.FuseboxState;
import org.chromium.chrome.browser.omnibox.fusebox.FuseboxCoordinator.PopupState;
import org.chromium.chrome.browser.omnibox.geo.GeolocationHeader;
import org.chromium.chrome.browser.omnibox.status.StatusCoordinator;
import org.chromium.chrome.browser.omnibox.styles.OmniboxResourceProvider;
import org.chromium.chrome.browser.omnibox.suggestions.AutocompleteCoordinator;
import org.chromium.chrome.browser.omnibox.suggestions.OmniboxLoadUrlParams;
import org.chromium.chrome.browser.omnibox.suggestions.OmniboxSuggestionsContainer;
import org.chromium.chrome.browser.omnibox.suggestions.OmniboxSuggestionsDropdownScrollListener;
import org.chromium.chrome.browser.omnibox.suggestions.SiteSearchActivationSource;
import org.chromium.chrome.browser.omnibox.voice.VoiceRecognitionHandler;
import org.chromium.chrome.browser.prefetch.settings.PreloadPagesSettingsBridge;
import org.chromium.chrome.browser.prefetch.settings.PreloadPagesState;
import org.chromium.chrome.browser.profiles.Profile;
import org.chromium.chrome.browser.tab.EmptyTabObserver;
import org.chromium.chrome.browser.tab.Tab;
import org.chromium.chrome.browser.tab.Tab.LoadUrlResult;
import org.chromium.chrome.browser.tab.TabLaunchType;
import org.chromium.chrome.browser.tabmodel.TabModelSelector;
import org.chromium.chrome.browser.theme.ThemeUtils;
import org.chromium.chrome.browser.toolbar.ToolbarVariationUtils;
import org.chromium.chrome.browser.ui.default_browser_promo.DefaultBrowserPromoUtils;
import org.chromium.chrome.browser.ui.extensions.ExtensionUi;
import org.chromium.chrome.browser.ui.theme.BrandedColorScheme;
import org.chromium.chrome.browser.util.ChromeAccessibilityUtil;
import org.chromium.components.browser_ui.accessibility.PageZoomIndicatorCoordinator;
import org.chromium.components.browser_ui.share.ShareHelper;
import org.chromium.components.browser_ui.share.ShareParams;
import org.chromium.components.browser_ui.styles.ChromeColors;
import org.chromium.components.browser_ui.widget.animation.CancelAwareAnimatorListener;
import org.chromium.components.browser_ui.widget.gesture.BackPressHandler;
import org.chromium.components.embedder_support.util.UrlConstants;
import org.chromium.components.embedder_support.util.UrlUtilities;
import org.chromium.components.metrics.OmniboxEventProtos.OmniboxEventProto.PageClassification;
import org.chromium.components.omnibox.AutocompleteInput;
import org.chromium.components.omnibox.AutocompleteInput.AutocompleteState;
import org.chromium.components.omnibox.AutocompleteMatch;
import org.chromium.components.omnibox.AutocompleteRequestType;
import org.chromium.components.omnibox.OmniboxCapabilities;
import org.chromium.components.omnibox.OmniboxFeatures;
import org.chromium.components.omnibox.OmniboxFocusReason;
import org.chromium.components.search_engines.TemplateUrlService;
import org.chromium.components.search_engines.TemplateUrlService.TemplateUrlServiceObserver;
import org.chromium.components.webapps.AddToHomescreenCoordinator;
import org.chromium.components.webapps.AppBannerManager;
import org.chromium.content_public.browser.LoadUrlParams;
import org.chromium.content_public.browser.WebContents;
import org.chromium.content_public.common.ResourceRequestBody;
import org.chromium.ui.base.DeviceFormFactor;
import org.chromium.ui.base.KeyNavigationUtil;
import org.chromium.ui.base.PageTransition;
import org.chromium.ui.base.WindowAndroid;
import org.chromium.ui.interpolators.Interpolators;
import org.chromium.ui.modaldialog.ModalDialogManager;
import org.chromium.url.GURL;

import java.util.ArrayList;
import java.util.Collection;
import java.util.List;
import java.util.function.BooleanSupplier;
import java.util.function.Supplier;

/**
 * Mediator for the LocationBar component. Intended location for LocationBar business logic;
 * currently, migration of this logic out of LocationBarLayout is in progress.
 */
@NullMarked
class LocationBarMediator
        implements Observer,
                OmniboxStub,
                VoiceRecognitionHandler.Observer,
                UrlBarDelegate,
                OnKeyListener,
                FuseboxAttachmentChangeListener,
                ComponentCallbacks,
                TemplateUrlServiceObserver,
                BackPressHandler,
                PauseResumeWithNativeObserver,
                SearchBoxHintTextObserver,
                AppBannerManager.Observer,
                OmniboxSuggestionsDropdownScrollListener {

    private static final int ICON_FADE_ANIMATION_DURATION_MS = 150;
    private static final int ICON_FADE_ANIMATION_DELAY_MS = 75;
    private static final long NTP_KEYBOARD_FOCUS_DURATION_MS = 200;
    private static final int WIDTH_CHANGE_ANIMATION_DURATION_MS = 225;
    private static final int WIDTH_CHANGE_ANIMATION_DELAY_MS = 75;
    public static final int POPOVER_FADE_DURATION_MS = 150;
    private @Nullable Boolean mIsLensOnOmniboxEnabled;
    private @Nullable ViewGroup mToolbarParent;
    private int mIndexInToolbar;
    private @Nullable View mDropdown;
    private boolean mIsReparenting;

    /** Uma methods for omnibox. */
    public interface OmniboxUma {
        /**
         * Record the NTP navigation events on omnibox.
         *
         * @param url The URL to which the user navigated.
         * @param transition The transition type of the navigation.
         * @param isNtp Whether the current page is a NewTabPage.
         */
        void recordNavigationOnNtp(String url, int transition, boolean isNtp);
    }

    private final FloatProperty<LocationBarMediator> mUrlFocusChangeFractionProperty =
            new FloatProperty<>("") {
                @Override
                public Float get(LocationBarMediator object) {
                    return mUrlFocusChangeFraction;
                }

                @Override
                public void setValue(LocationBarMediator object, float value) {
                    setUrlFocusChangeFraction(value, value);
                }
            };

    private final FloatProperty<LocationBarMediator> mWidthChangeFractionPropertyTablet =
            new FloatProperty<>("") {
                @Override
                public Float get(LocationBarMediator object) {
                    return ((LocationBarTablet) mLocationBarLayout).getWidthChangeFraction();
                }

                @Override
                public void setValue(LocationBarMediator object, float value) {
                    ((LocationBarTablet) mLocationBarLayout).setWidthChangeAnimationFraction(value);
                    if (mUrlHasFocus) {
                        mEmbedderImpl.recalculateOmniboxAlignment();
                    }
                }
            };

    private final LocationBarLayout mLocationBarLayout;
    private VoiceRecognitionHandler mVoiceRecognitionHandler;
    private final LocationBarDataProvider mLocationBarDataProvider;
    private final @Nullable BrowserControlsStateProvider mBrowserControlsStateProvider;
    private final LocationBarEmbedderUiOverrides mEmbedderUiOverrides;
    private final LocationBarEmbedder mLocationBarEmbedder;
    private StatusCoordinator mStatusCoordinator;
    private @Nullable AutocompleteCoordinator mAutocompleteCoordinator;
    private @Nullable OmniboxPrerender mOmniboxPrerender;
    private UrlBarCoordinator mUrlCoordinator;
    private final MonotonicObservableSupplier<Profile> mProfileSupplier;
    private final CallbackController mCallbackController = new CallbackController();
    private final OverrideUrlLoadingDelegate mOverrideUrlLoadingDelegate;
    private final LocaleManager mLocaleManager;
    private final OneshotSupplier<TemplateUrlService> mTemplateUrlServiceSupplier;
    private final Context mContext;
    private final BackKeyBehaviorDelegate mBackKeyBehavior;
    private final WindowAndroid mWindowAndroid;
    private GURL mOriginalUrl = GURL.emptyGURL();
    private @Nullable Animator mUrlFocusChangeAnimator;
    private final ObserverList<UrlFocusChangeListener> mUrlFocusChangeListeners =
            new ObserverList<>();
    private final Rect mRootViewBounds = new Rect();
    private final OmniboxUma mOmniboxUma;
    private final OmniboxSuggestionsDropdownEmbedderImpl mEmbedderImpl;
    private final @Nullable PageZoomIndicatorCoordinator mPageZoomIndicatorCoordinator;
    private final @Nullable LocationBarFocusScrimHandler mScrimHandler;

    private boolean mNativeInitialized;
    private boolean mUrlFocusedWithoutAnimations;
    private boolean mIsUrlFocusChangeInProgress;
    private final boolean mIsTablet;
    private boolean mShouldShowLensButtonWhenUnfocused;
    private boolean mShouldShowMicButtonWhenUnfocused;
    // Whether the microphone and bookmark buttons should be shown in the tablet location bar. These
    // buttons are hidden if the window size is < 600dp.
    private boolean mShouldShowButtonsWhenUnfocused;
    private float mUrlFocusChangeFraction;
    private boolean mUrlHasFocus;
    private @Nullable Boolean mPreviousDeleteButtonVisible;
    private @Nullable Boolean mPreviousInstallButtonVisible;
    private @Nullable Boolean mPreviousMicButtonVisible;
    private @Nullable Boolean mPreviousLensButtonVisible;
    private @Nullable Boolean mPreviousBookmarkButtonVisible;
    private @Nullable Boolean mPreviousZoomButtonVisible;
    private LensController mLensController;
    private final BooleanSupplier mIsToolbarMicEnabledSupplier;
    // Tracks if the location bar is laid out in a focused state due to an ntp scroll.
    private boolean mIsLocationBarFocusedFromNtpScroll;
    private boolean mAccessibilityFocusWorkaroundInProgress;
    private @BrandedColorScheme int mBrandedColorScheme = BrandedColorScheme.APP_DEFAULT;
    // TODO(https://crbug.com/481357849): Remove this.
    private boolean mHasEverUpdatedBrandedColorScheme;
    private final SettableNonNullObservableSupplier<Boolean> mBackPressStateSupplier =
            ObservableSuppliers.createNonNull(false);
    private final MonotonicObservableSupplier<TabModelSelector> mTabModelSelectorSupplier;
    private @Nullable SearchEngineUtils mSearchEngineUtils;
    private @Nullable AddToHomescreenCoordinator mAddToHomescreenCoordinatorForTesting;
    private final Supplier<@Nullable ModalDialogManager> mModalDialogManagerSupplier;
    private final FuseboxCoordinator mFuseboxCoordinator;
    private @Nullable AutocompleteInput mCurrentInput;
    private @Nullable FuseboxAttachmentModelList mFuseboxAttachmentModelList;
    private final Callback<@AutocompleteRequestType Integer> mAutocompleteRequestTypeObserver =
            this::onAutocompleteRequestTypeChanged;
    private final Callback<AutocompleteInput.@Nullable SiteSearchData> mSiteSearchDataObserver =
            (siteSearchData) -> onSearchBoxHintTextChanged();
    private @Nullable Callback<Boolean> mOnSpecializedFuseboxModeActivatedCallback;

    private final ButtonToolbarWidthConsumer mBookmarkButtonToolbarWidthConsumer;
    private final ButtonToolbarWidthConsumer mInstallButtonToolbarWidthConsumer;
    private final ButtonToolbarWidthConsumer mMicButtonToolbarWidthConsumer;
    private final ButtonToolbarWidthConsumer mLensButtonToolbarWidthConsumer;
    private final ButtonToolbarWidthConsumer mZoomButtonToolbarWidthConsumer;
    private final @Nullable OmniboxChipManager mOmniboxChipManager;
    private final SettableNullableObservableSupplier<GURL> mExactMatchUrlSupplier =
            ObservableSuppliers.createNullable();
    private boolean mMiniOriginMode;

    /*package */ LocationBarMediator(
            Context context,
            LocationBarLayout locationBarLayout,
            LocationBarDataProvider locationBarDataProvider,
            LocationBarEmbedderUiOverrides embedderUiOverrides,
            MonotonicObservableSupplier<Profile> profileSupplier,
            OverrideUrlLoadingDelegate overrideUrlLoadingDelegate,
            LocaleManager localeManager,
            OneshotSupplier<TemplateUrlService> templateUrlServiceSupplier,
            BackKeyBehaviorDelegate backKeyBehavior,
            WindowAndroid windowAndroid,
            boolean isTablet,
            LensController lensController,
            OmniboxUma omniboxUma,
            BooleanSupplier isToolbarMicEnabledSupplier,
            OmniboxSuggestionsDropdownEmbedderImpl dropdownEmbedder,
            MonotonicObservableSupplier<TabModelSelector> tabModelSelectorSupplier,
            @Nullable BrowserControlsStateProvider browserControlsStateProvider,
            Supplier<@Nullable ModalDialogManager> modalDialogManagerSupplier,
            @Nullable PageZoomIndicatorCoordinator pageZoomIndicatorCoordinator,
            FuseboxCoordinator fuseboxCoordinator,
            LocationBarEmbedder locationBarEmbedder,
            @Nullable OmniboxChipManager omniboxChipManager,
            @Nullable LocationBarFocusScrimHandler scrimHandler) {
        mContext = context;
        mLocationBarLayout = locationBarLayout;
        mLocationBarDataProvider = locationBarDataProvider;
        mLocationBarEmbedder = locationBarEmbedder;
        mFuseboxCoordinator = fuseboxCoordinator;
        mLocationBarDataProvider.addObserver(this);
        mEmbedderUiOverrides = embedderUiOverrides;
        mOverrideUrlLoadingDelegate = overrideUrlLoadingDelegate;
        mLocaleManager = localeManager;
        mProfileSupplier = profileSupplier;
        mProfileSupplier.addSyncObserverAndPostIfNonNull(
                mCallbackController.makeCancelable(this::setProfile));
        mTemplateUrlServiceSupplier = templateUrlServiceSupplier;
        mBackKeyBehavior = backKeyBehavior;
        mWindowAndroid = windowAndroid;
        mIsTablet = isTablet;
        mShouldShowButtonsWhenUnfocused = isTablet;
        mLensController = lensController;
        mOmniboxUma = omniboxUma;
        mIsToolbarMicEnabledSupplier = isToolbarMicEnabledSupplier;
        mEmbedderImpl = dropdownEmbedder;
        mTabModelSelectorSupplier = tabModelSelectorSupplier;
        mBrowserControlsStateProvider = browserControlsStateProvider;
        mModalDialogManagerSupplier = modalDialogManagerSupplier;
        mPageZoomIndicatorCoordinator = pageZoomIndicatorCoordinator;
        if (mPageZoomIndicatorCoordinator != null) {
            mPageZoomIndicatorCoordinator.setOnDismissCallbacks(
                    () -> updateZoomButtonVisibility(/* notifyEmbedder= */ true));
        }
        AppBannerManager.addObserver(this);
        mScrimHandler = scrimHandler;

        mBookmarkButtonToolbarWidthConsumer =
                new ButtonToolbarWidthConsumer(
                        mContext,
                        mIsTablet,
                        this::shouldShowBookmarkButton,
                        this::setBookmarkButtonVisibility);
        mInstallButtonToolbarWidthConsumer =
                new ButtonToolbarWidthConsumer(
                        mContext,
                        mIsTablet,
                        this::shouldShowInstallButton,
                        this::setInstallButtonVisibility);
        mMicButtonToolbarWidthConsumer =
                new ButtonToolbarWidthConsumer(
                        mContext,
                        mIsTablet,
                        this::shouldShowMicButton,
                        this::setMicButtonVisibility);
        mLensButtonToolbarWidthConsumer =
                new ButtonToolbarWidthConsumer(
                        mContext,
                        mIsTablet,
                        this::shouldShowLensButton,
                        this::setLensButtonVisibility);
        mZoomButtonToolbarWidthConsumer =
                new ButtonToolbarWidthConsumer(
                        mContext,
                        mIsTablet,
                        this::shouldShowZoomButton,
                        this::setZoomButtonVisibility);

        mFuseboxCoordinator
                .getFuseboxStateSupplier()
                .addSyncObserverAndPostIfNonNull(
                        mCallbackController.makeCancelable(this::onFuseboxStateChanged));
        mFuseboxCoordinator.setOnInteractionCompletedCallback(this::onFuseboxInteractionCompleted);
        mFuseboxCoordinator.setOnFirstPickerInteractionCanceledCallback(this::endInput);
        mOmniboxChipManager = omniboxChipManager;
    }

    /**
     * Sets coordinators post-construction; they can't be set at construction time since
     * LocationBarMediator is a delegate for them, so is constructed beforehand.
     *
     * @param urlCoordinator Coordinator for the url bar.
     * @param autocompleteCoordinator Coordinator for the autocomplete component.
     * @param statusCoordinator Coordinator for the status icon.
     */
    @Initializer
    /*package */ void setCoordinators(
            UrlBarCoordinator urlCoordinator,
            AutocompleteCoordinator autocompleteCoordinator,
            StatusCoordinator statusCoordinator) {
        mUrlCoordinator = urlCoordinator;
        mAutocompleteCoordinator = autocompleteCoordinator;
        mStatusCoordinator = statusCoordinator;

        // Set up VoiceRecognitionHandler once mAutocompleteCoordinator is set.
        if (mVoiceRecognitionHandler == null) {
            mVoiceRecognitionHandler =
                    new VoiceRecognitionHandler(
                            this,
                            mLocationBarDataProvider,
                            mAutocompleteCoordinator,
                            mWindowAndroid,
                            mProfileSupplier);
            mVoiceRecognitionHandler.addObserver(this);
        }

        mAutocompleteCoordinator.addOmniboxSuggestionsDropdownScrollListener(this);

        updateShouldAnimateIconChanges();
        updateButtonVisibility();
        updateSearchEngineStatusIconShownState();
    }

    @SuppressWarnings("NullAway")
    /* package */ void destroy() {
        mCallbackController.destroy();
        endInputInternal();
        TemplateUrlService templateUrlService = mTemplateUrlServiceSupplier.get();
        if (templateUrlService != null) {
            templateUrlService.removeObserver(this);
        }
        if (mSearchEngineUtils != null) {
            mSearchEngineUtils.removeSearchBoxHintTextObserver(this);
        }
        mStatusCoordinator = null;
        mAutocompleteCoordinator.removeOmniboxSuggestionsDropdownScrollListener(this);
        mAutocompleteCoordinator = null;
        mUrlCoordinator = null;
        mVoiceRecognitionHandler.removeObserver(this);
        mVoiceRecognitionHandler.destroy();
        mVoiceRecognitionHandler = null;
        mLocationBarDataProvider.removeObserver(this);
        mUrlFocusChangeListeners.clear();
        if (mPageZoomIndicatorCoordinator != null) {
            mPageZoomIndicatorCoordinator.setOnDismissCallbacks(null);
        }
        AppBannerManager.removeObserver(this);
    }

    /**
     * Returns a supplier that provides the URL of the default match if it is a non-search
     * navigation suggestion, or null otherwise.
     */
    public NullableObservableSupplier<GURL> getExactMatchUrlSupplier() {
        return mExactMatchUrlSupplier;
    }

    /*package */ void onUrlFocusChange(boolean hasFocus) {
        if (mAccessibilityFocusWorkaroundInProgress) {
            return;
        }
        if (!hasFocus) {
            mPreviousLensButtonVisible = null;
        }
        setUrlFocusChangeInProgress(true);
        mUrlHasFocus = hasFocus;
        // Intercept back press if it has focus.
        mBackPressStateSupplier.set(mUrlHasFocus);
        updateButtonVisibility();
        updateShouldAnimateIconChanges();
        onPrimaryColorChanged();

        if (hasFocus) {
            if (mNativeInitialized) RecordUserAction.record("FocusLocation");
        } else {
            mUrlFocusedWithoutAnimations = false;
        }

        if (!mUrlFocusedWithoutAnimations) handleUrlFocusAnimation(hasFocus);

        if (hasFocus
                && mLocationBarDataProvider.hasTab()
                && !mLocationBarDataProvider.isIncognito()) {
            TemplateUrlService templateUrlService = mTemplateUrlServiceSupplier.get();
            if (templateUrlService != null) {
                GeolocationHeader.primeLocationForGeoHeaderIfEnabled(
                        assertNonNull(mProfileSupplier.get()), templateUrlService);
            } else {
                mTemplateUrlServiceSupplier.onAvailable(
                        (service) -> {
                            GeolocationHeader.primeLocationForGeoHeaderIfEnabled(
                                    assertNonNull(mProfileSupplier.get()), service);
                        });
            }
            hintZeroSuggestRefresh();
        } // Focus change caused by a closed tab may result in there not being an active tab.
        if (!hasFocus && mLocationBarDataProvider.hasTab()) {
            setUrl(
                    mLocationBarDataProvider.getCurrentGurl(),
                    mLocationBarDataProvider.getUrlBarData());
        }
    }

    /*package */ void onFinishNativeInitialization() {
        mNativeInitialized = true;
        mOmniboxPrerender = new OmniboxPrerender();
        mTemplateUrlServiceSupplier.onAvailable(
                mCallbackController.makeCancelable(
                        (templateUrlService) -> {
                            Profile profile = mProfileSupplier.get();
                            assert profile != null;
                            templateUrlService.addObserver(this);
                            GeolocationHeader.primeLocationForGeoHeaderIfEnabled(
                                    profile, templateUrlService);
                        }));

        mLocationBarLayout.onFinishNativeInitialization();
        Profile profile = mProfileSupplier.get();
        if (profile != null) {
            setProfile(profile);
        }

        mLocationBarLayout.setMicButtonDrawable(
                AppCompatResources.getDrawable(mContext, R.drawable.ic_mic_white_24dp));

        onPrimaryColorChanged();

        updateButtonVisibility();
    }

    /* package */ void setUrlFocusChangeFraction(
            float ntpSearchBoxScrollFraction, float urlFocusChangeFraction) {
        float fraction = Math.max(ntpSearchBoxScrollFraction, urlFocusChangeFraction);
        mUrlFocusChangeFraction = fraction;
        if (mIsTablet) {
            mLocationBarDataProvider
                    .getNewTabPageDelegate()
                    .setUrlFocusChangeAnimationPercent(fraction);
            mLocationBarLayout.setUrlFocusChangePercent(
                    fraction, fraction, mIsUrlFocusChangeInProgress);
        } else {
            // Determine when the focus state changes as a result of ntp scrolling.
            boolean isLocationBarFocusedFromNtpScroll =
                    fraction > 0f && !mIsUrlFocusChangeInProgress;
            if (isLocationBarFocusedFromNtpScroll != mIsLocationBarFocusedFromNtpScroll) {
                mIsLocationBarFocusedFromNtpScroll = isLocationBarFocusedFromNtpScroll;
                onUrlFocusedFromNtpScrollChanged();
            }

            if (!ChromeFeatureList.sToolbarPhoneAnimationRefactor.isEnabled()) {
                if (fraction > 0f) {
                    setUrlActionContainerVisibility(true);
                } else if (fraction == 0f && !mIsUrlFocusChangeInProgress) {
                    // If a URL focus change is in progress, then it will handle setting the
                    // visibility correctly after it completes.  If done here, it would cause the
                    // URL to jump due to a badly timed layout call.
                    setUrlActionContainerVisibility(false);
                }
            }

            // Add expansion animation for the space besides status view in location bar.
            mLocationBarLayout.setUrlFocusChangePercent(
                    ntpSearchBoxScrollFraction,
                    urlFocusChangeFraction,
                    mIsUrlFocusChangeInProgress);
        }
    }

    /* package */ void onUrlFocusedFromNtpScrollChanged() {
        updateButtonVisibility();
    }

    /*package */ void setUnfocusedWidth(int unfocusedWidth) {
        mLocationBarLayout.setUnfocusedWidth(unfocusedWidth);
    }

    /* package */ void setVoiceRecognitionHandlerForTesting(
            VoiceRecognitionHandler voiceRecognitionHandler) {
        mVoiceRecognitionHandler = voiceRecognitionHandler;
    }

    /* package */ void setLensControllerForTesting(LensController lensController) {
        mLensController = lensController;
    }

    void resetLastCachedIsLensOnOmniboxEnabledForTesting() {
        mIsLensOnOmniboxEnabled = null;
    }

    /* package */ void setIsUrlBarFocusedWithoutAnimationsForTesting(
            boolean isUrlBarFocusedWithoutAnimations) {
        mUrlFocusedWithoutAnimations = isUrlBarFocusedWithoutAnimations;
    }

    /*package */ void updateVisualsForState() {
        onPrimaryColorChanged();
    }

    /*package */ @BrandedColorScheme
    int getBrandedColorScheme() {
        return mBrandedColorScheme;
    }

    /*package */ void setShowTitle(boolean showTitle) {
        // This method is only used in CustomTabToolbar.
    }

    public void maybeShowOrClearCursorInLocationBar() {
        if (!DeviceFormFactor.isNonMultiDisplayContextOnTablet(mContext)) return;
        Tab tab = mLocationBarDataProvider.getTab();
        if (tab == null) return;
        boolean onNtp = UrlUtilities.isNtpUrl(tab.getUrl());

        if (ChromeAccessibilityUtil.get().isAccessibilityEnabled()
                && mLocationBarDataProvider.getNewTabPageDelegate().isCurrentlyVisible()) {
            mUrlCoordinator.requestAccessibilityFocus();
        }

        // While a hardware keyboard is connected, loading the NTP should cause the URL bar to gain
        // focus with a blinking cursor and without focus animations. Loading a non-NTP URL should
        // clear such focus if it exists.
        if (OmniboxCapabilities.hasDesktopExperience(mContext)) {
            if (onNtp) {
                showUrlBarCursorWithoutFocusAnimations();
            } else {
                clearUrlBarCursorWithoutFocusAnimations();
            }
        }
    }

    /*package */ void showUrlBarCursorWithoutFocusAnimations() {
        if (mUrlHasFocus || didFocusUrlFromFakebox()) {
            return;
        }

        // Verify if Hardware keyboard still requests Software keyboard (IME) to be used.
        // If that happens, suppress early focus to take Software keyboard out of the way.
        // This is specifically relevant in Incognito mode, where Soft keyboard clobbers relevant
        // messages.
        // The setting below is not explicitly itemized in Settings.Secure, but it corresponds
        // to whether Software keyboard would be called up when Physical keyboard is in use on
        // Pixel devices.
        if (KeyboardUtils.shouldShowImeWithHardwareKeyboard(mContext)) return;

        mUrlFocusedWithoutAnimations = true;
        // This method should only be called on devices with a hardware keyboard attached, as
        // described in the documentation for LocationBar#showUrlBarCursorWithoutFocusAnimations.
        beginInput(
                new AutocompleteInput()
                        .setAutocompleteState(AutocompleteState.STANDBY)
                        .setFocusReason(OmniboxFocusReason.DEFAULT_WITH_HARDWARE_KEYBOARD));
    }

    /**
     * If the URL bar was previously focused on the NTP due to a connected keyboard, an navigation
     * away from the NTP should clear this focus before filling the current tab's URL.
     */
    /*package */ void clearUrlBarCursorWithoutFocusAnimations() {
        if (mUrlCoordinator.hasFocus() && mUrlFocusedWithoutAnimations) {
            // If we did not run the focus animations, then the user has not typed any text.
            // So, clear the focus and accept whatever URL the page is currently attempting to
            // display, given that the current tab is not displaying the NTP.
            endInput();
        }
    }

    /*package */ void revertChanges() {
        if (mCurrentInput != null) {
            // Propagate the requested update to both Autocomplete and the UrlBar.
            // Programmatic reselection of user text on UrlBar does not propagate the change to
            // TextChange listeners.
            mCurrentInput
                    .setAutocompleteState(AutocompleteState.STANDBY)
                    .setUserText(mCurrentInput.getInitialUserText());
            mUrlCoordinator.setUrlBarData(
                    getUrlBarDataForCurrentInput(mCurrentInput),
                    UrlBar.ScrollType.NO_SCROLL,
                    UrlBarData.SELECT_ALL);
            mUrlCoordinator.setKeyboardVisibility(false, false);
        } else {
            setUrl(
                    mLocationBarDataProvider.getCurrentGurl(),
                    mLocationBarDataProvider.getUrlBarData());
        }
    }

    /** Triggers on EACH key press to drive fast site-search triggers. */
    /* package */ void onUrlTextRichChanged(UrlBarTextChangeInfo info) {
        if (mCurrentInput == null) return;

        if (shouldTriggerSiteSearch(info)) {
            if (mAutocompleteCoordinator != null
                    && mAutocompleteCoordinator.triggerSiteSearch(
                            SiteSearchActivationSource.SPACE)) {
                return;
            }
        }
    }

    /** Triggers only when IME input batch completes to drive autocomplete. */
    /* package */ void onUrlTextChanged(String text) {
        updateButtonVisibility();
        if (mCurrentInput == null) return;

        mCurrentInput
                .setUserText(text)
                .setAllowUserTextAutocompletion(mUrlCoordinator.shouldAutocomplete());
    }

    /**
     * Determines whether site-search should be triggered based on the current text change.
     * Expression triggers if a single space character was introduced.
     *
     * <p>Note that this is an initial check. The AutocompleteCoordinator will perform a more
     * thorough check to determine if site-search should be triggered based on whether the keyword
     * is valid.
     *
     * @param info Information about the text change.
     * @return True if site-search should be triggered, false otherwise.
     */
    @VisibleForTesting
    /* package */ boolean shouldTriggerSiteSearch(UrlBarTextChangeInfo info) {

        if (info.isDelete()) {
            // Deletions should never trigger site-search. This helps avoid accidentally activating
            // site-search when the user is trying to delete text.
            return false;
        }

        String text = info.getText();

        if (text.startsWith(" ")) {
            // Don't trigger site search if leading by spaces.
            return false;
        }

        // Check if a single space character was added (either by pure insertion or by replacing
        // selection with a single space).
        // This is the primary condition for site-search activation.
        boolean singleSpaceAdded =
                info.getAfter() == 1
                        && info.getStart() < text.length()
                        && text.charAt(info.getStart()) == ' ';
        if (!singleSpaceAdded) {
            return false;
        }

        String textBeforeSpace = text.substring(0, info.getStart());
        if (textBeforeSpace.trim().isEmpty()) {
            // No valid text before space (e.g. space as first character, or consecutive spaces).
            return false;
        }

        if (textBeforeSpace.contains(" ")) {
            // Multiple words before space. We only trigger site search if the user typed a space
            // after a single word (e.g. keywords + space). If there are multiple words, it is
            // likely a normal query.
            return false;
        }

        return true;
    }

    @Override
    public void onSuggestionDropdownScroll() {}

    @Override
    public void onSuggestionDropdownOverscrolledToTop() {}

    @Override
    public void onSuggestionDropdownScrollOffsetChanged(int scrollOffset) {
        mLocationBarLayout.onSuggestionsListScrollOffsetChanged(scrollOffset);
    }

    private @Nullable GURL getExactMatchUrl(@Nullable AutocompleteMatch defaultMatch) {
        if (mCurrentInput == null) return null;

        // Other modes cannot exact match.
        if (!mCurrentInput.isConventionalRequestType()) return null;

        // Zero suggest is always considered Search.
        if (TextUtils.isEmpty(mCurrentInput.getUserText())) return null;

        // Search suggestions again are search, not an exact matches.
        if (defaultMatch == null || defaultMatch.isSearchSuggestion()) return null;

        return defaultMatch.getUrl();
    }

    /* package */ void onSuggestionsChanged(
            @Nullable AutocompleteMatch defaultMatch, boolean hasSuggestions) {
        if (mAutocompleteCoordinator == null || mCurrentInput == null) return;

        String userText = mCurrentInput.getUserText();
        if (mScrimHandler != null) {
            // Respond to events of user reverting back to suppressed state by pressing <Esc> key.
            // This only matters in scenarios where a physical keyboard is connected.
            // See handleEscPress below.
            mScrimHandler.setVisibility(
                    mCurrentInput.getAutocompleteState() == AutocompleteState.ENABLED);
        }
        mExactMatchUrlSupplier.set(getExactMatchUrl(defaultMatch));

        if (mUrlCoordinator.shouldAutocomplete()) {
            String siteSearchLabel = null;
            if (defaultMatch != null && defaultMatch.getAssociatedKeyword() != null) {
                TemplateUrlService templateUrlService = mTemplateUrlServiceSupplier.get();
                if (templateUrlService != null) {
                    siteSearchLabel =
                            templateUrlService.getFullNameFromTemplateUrl(
                                    defaultMatch.getAssociatedKeyword());
                }
            }

            mUrlCoordinator.setAutocompleteText(
                    userText,
                    defaultMatch != null ? defaultMatch.getInlineAutocompletion() : null,
                    defaultMatch != null ? defaultMatch.getAdditionalText() : null,
                    siteSearchLabel);
        }

        // Handle the case where suggestions (in particular zero suggest) are received without the
        // URL focusing happening.
        if (mUrlFocusedWithoutAnimations && mUrlHasFocus) {
            handleUrlFocusAnimation(/* hasFocus= */ true);
        }

        Profile profile = mProfileSupplier.get();
        if (mNativeInitialized
                && profile != null
                && !CommandLine.getInstance().hasSwitch(ChromeSwitches.DISABLE_INSTANT)
                && DeviceClassManager.enablePrerendering()
                && PreloadPagesSettingsBridge.getState(profile) != PreloadPagesState.NO_PRELOADING
                && mLocationBarDataProvider.hasTab()) {
            assumeNonNull(mOmniboxPrerender);
            mOmniboxPrerender.prerenderMaybe(
                    userText,
                    mOriginalUrl.getSpec(),
                    mAutocompleteCoordinator.getCurrentNativeAutocompleteResult(),
                    profile,
                    mLocationBarDataProvider.getTab());
        }

        mUrlCoordinator.onUrlBarSuggestionsChanged(
                mAutocompleteCoordinator.getSuggestionCount() != 0);
        mLocationBarLayout.onSuggestionsChanged(hasSuggestions);
    }

    /* package */ void loadUrl(OmniboxLoadUrlParams omniboxLoadUrlParams) {
        try (TraceEvent e = TraceEvent.scoped("LocationBarMediator.loadUrl")) {
            assert mLocationBarDataProvider != null;
            Tab currentTab = mLocationBarDataProvider.getTab();

            // The code of the rest of this class ensures that this can't be called until the native
            // side is initialized
            assert mNativeInitialized : "Loading URL before native side initialized";

            // TODO(crbug.com/40693835): Should be taking a full loaded LoadUrlParams.
            if (mOverrideUrlLoadingDelegate.willHandleLoadUrlWithPostData(
                    omniboxLoadUrlParams, mLocationBarDataProvider.isIncognito())) {
                return;
            }

            String url = omniboxLoadUrlParams.url;
            if (url != null && url.startsWith(UrlConstants.CHROME_EXTENSION_SCHEME + "://")) {
                if (currentTab != null && currentTab.getWebContents() != null) {
                    ExtensionUi.onOmniboxExtensionInputEntered(
                            currentTab.getWebContents(),
                            url,
                            omniboxLoadUrlParams.openInNewTab,
                            omniboxLoadUrlParams.openInNewWindow);
                }
                return;
            }

            if (currentTab != null) {
                boolean isCurrentTabNtpUrl = UrlUtilities.isNtpUrl(currentTab.getUrl());
                if (currentTab.isNativePage() || isCurrentTabNtpUrl) {
                    mOmniboxUma.recordNavigationOnNtp(
                            omniboxLoadUrlParams.url,
                            omniboxLoadUrlParams.transitionType,
                            !currentTab.isIncognito() && isCurrentTabNtpUrl);
                    // Passing in an empty string should not do anything unless the user is at the
                    // NTP. Since the NTP has no url, pressing enter while clicking on the URL bar
                    // should refresh the page as it does when you click and press enter on any
                    // other site.
                    if (url.isEmpty()) url = currentTab.getUrl().getSpec();
                }

                if (omniboxLoadUrlParams.callback != null) {
                    currentTab.addObserver(
                            new EmptyTabObserver() {
                                @Override
                                public void onLoadUrl(
                                        Tab tab,
                                        LoadUrlParams params,
                                        LoadUrlResult loadUrlResult) {
                                    omniboxLoadUrlParams.callback.onLoadUrl(params, loadUrlResult);
                                    tab.removeObserver(this);
                                }
                            });
                }
            }

            // Loads the |url| in a new tab or the current ContentView and gives focus to the
            // ContentView.
            if (currentTab != null && !url.isEmpty()) {
                LoadUrlParams loadUrlParams = new LoadUrlParams(url);
                try (TimingMetric record =
                        TimingMetric.shortUptime("Android.Omnibox.SetGeolocationHeadersTime")) {
                    loadUrlParams.setVerbatimHeaders(
                            GeolocationHeader.getGeoHeader(
                                    url,
                                    assertNonNull(mProfileSupplier.get()),
                                    mTemplateUrlServiceSupplier.get()));
                }
                loadUrlParams.setTransitionType(
                        omniboxLoadUrlParams.transitionType | PageTransition.FROM_ADDRESS_BAR);
                if (omniboxLoadUrlParams.inputStartTimestamp != 0) {
                    loadUrlParams.setInputStartTimestamp(omniboxLoadUrlParams.inputStartTimestamp);
                }

                if (!omniboxLoadUrlParams.extraHeaders.isEmpty()) {
                    StringBuilder headers = new StringBuilder();
                    for (var entry : omniboxLoadUrlParams.extraHeaders.entrySet()) {
                        headers.append(entry.getKey());
                        headers.append(": ");
                        headers.append(entry.getValue());
                        headers.append("\r\n");
                    }
                    String previousHeaders = loadUrlParams.getVerbatimHeaders();
                    if (!TextUtils.isEmpty(previousHeaders)) {
                        headers.append(previousHeaders);
                    }

                    loadUrlParams.setVerbatimHeaders(headers.toString());
                }

                if (omniboxLoadUrlParams.postData != null
                        && omniboxLoadUrlParams.postData.length != 0) {
                    loadUrlParams.setPostData(
                            ResourceRequestBody.createFromBytes(omniboxLoadUrlParams.postData));
                }

                TabModelSelector tabModelSelector = mTabModelSelectorSupplier.get();
                boolean processed = false;
                if (omniboxLoadUrlParams.openInNewWindow) {
                    Context tabContext = currentTab.getContext();
                    if (tabContext instanceof Activity sourceActivity) {
                        processed =
                                MultiInstanceOrchestratorFactory.getInstance()
                                        .openUrlInOtherWindow(
                                                sourceActivity,
                                                loadUrlParams,
                                                currentTab.getParentId(),
                                                /* preferNew= */ true,
                                                currentTab.isIncognitoBranded());
                    }
                } else if (omniboxLoadUrlParams.openInNewTab && tabModelSelector != null) {
                    tabModelSelector.openNewTab(
                            loadUrlParams,
                            TabLaunchType.FROM_OMNIBOX,
                            currentTab,
                            currentTab.isIncognito());
                    processed = true;
                }
                if (!processed) {
                    currentTab.loadUrl(loadUrlParams);
                }
                RecordUserAction.record("MobileOmniboxUse");
            }
            mLocaleManager.recordLocaleBasedSearchMetrics(
                    false, url, omniboxLoadUrlParams.transitionType);

            // Without the following postDelayedTask, focusCurrentTab runs on the critical path of
            // navigation. The following code postpone running focusCurrentTab and prioritize
            // running navigation code.
            PostTask.postDelayedTask(
                    TaskTraits.UI_USER_VISIBLE,
                    this::endInputAndFocusCurrentTab,
                    OmniboxFeatures.sPostDelayedTaskFocusTabTimeMillis.getValue());
        }
    }

    /* package */ boolean didFocusUrlFromFakebox() {
        // Retrieve state for the tab in case we are in the process of activating the input session.
        var state = FuseboxSessionState.from(mLocationBarDataProvider);
        if (state == null) return false;

        return switch (state.getAutocompleteInput().getFocusReason()) {
            case OmniboxFocusReason.FAKE_BOX_TAP,
                    OmniboxFocusReason.FAKE_BOX_LONG_PRESS,
                    OmniboxFocusReason.TASKS_SURFACE_FAKE_BOX_LONG_PRESS,
                    OmniboxFocusReason.TASKS_SURFACE_FAKE_BOX_TAP,
                    OmniboxFocusReason.FAKE_BOX_PLUS_BUTTON_TAP ->
                    true;
            default -> false;
        };
    }

    /** Recalculates the visibility of the buttons inside the location bar. */
    /* package */ void updateButtonVisibility() {
        updateDeleteButtonVisibility();
        updateBackButtonVisibility();
        updateNavigateButtonVisibility();
        updateInstallButtonVisibility(/* notifyEmbedder= */ false);
        updateMicButtonVisibility(/* notifyEmbedder= */ false);
        updateLensButtonVisibility(/* notifyEmbedder= */ false);
        if (mIsTablet) {
            updateBookmarkButtonVisibility(/* notifyEmbedder= */ false);
            updateZoomButtonVisibility(/* notifyEmbedder= */ false);
        }

        mLocationBarEmbedder.onWidthConsumerVisibilityChanged();

        if (mOmniboxChipManager != null) {
            updateOmniboxChipVisibility();
        }
    }

    /**
     * Sets the displayed URL according to the provided url string and UrlBarData.
     *
     * <p>The URL is converted to the most user friendly format (removing HTTP:// for example).
     *
     * <p>If the current tab is null, the URL text will be cleared.
     */
    /* package */ void setUrl(GURL currentUrl, UrlBarData urlBarData) {
        // If the URL is currently focused, do not replace the text they have entered with the URL.
        // Once they stop editing the URL, the current tab's URL will automatically be filled in.
        if (mUrlCoordinator.hasFocus()) {
            return;
        }

        mOriginalUrl = currentUrl;
        setUrlBarText(urlBarData, UrlBar.ScrollType.SCROLL_TO_TLD, UrlBarData.SELECT_ALL);
    }

    /* package */ void deleteButtonClicked(View view) {
        if (!mNativeInitialized) return;
        RecordUserAction.record("MobileOmniboxDeleteUrl");
        if (mCurrentInput == null) return; // session not started yet.

        if (TextUtils.isEmpty(mCurrentInput.getUserText())) {
            mCurrentInput.setRequestType(mLocationBarDataProvider.getDefaultRequestType());
        }

        mCurrentInput.setUserText(null);
        mUrlCoordinator.setUrlBarData(
                UrlBarData.forNonUrlText(mCurrentInput.getUserText()),
                UrlBar.ScrollType.NO_SCROLL,
                mCurrentInput.getSelection());
        updateButtonVisibility();
        mUrlCoordinator.requestAccessibilityFocus();
    }

    /* package */ void navigateButtonClicked(View view) {
        if (!mNativeInitialized) return;
        mUrlCoordinator.dispatchGoEvent();
    }

    /* package */ void micButtonClicked(View view) {
        if (!mNativeInitialized) return;
        // Hide keyboard before launch voice search to avoid keyboard action announcement in
        // TalkBack to be picked up by voice search.
        mUrlCoordinator.setKeyboardVisibility(false, false);

        RecordUserAction.record("MobileOmniboxVoiceSearch");
        mVoiceRecognitionHandler.startVoiceRecognition(
                mLocationBarLayout.getVoiceRecognitionSource(), CallbackUtils.emptyRunnable());
    }

    /** package */
    void lensButtonClicked(View view) {
        if (!mNativeInitialized || mLocationBarDataProvider == null) return;
        int entryPoint = mLocationBarLayout.getLensEntryPoint();
        // Lens does not track Search Widget metrics.
        // Enable once LensMetrics#getClickedActionName includes QUICK_ACTION_SEARCH_WIDGET.
        if (entryPoint != LensEntryPoint.QUICK_ACTION_SEARCH_WIDGET) {
            LensMetrics.recordClicked(entryPoint);
        }
        startLens(entryPoint);
    }

    /* package */ void zoomButtonClicked(View view) {
        WebContents webContents = getWebContentsForCurrentTab();
        if (mPageZoomIndicatorCoordinator == null || webContents == null) return;
        mPageZoomIndicatorCoordinator.show(webContents);
    }

    /* package */ void setAddToHomescreenCoordinatorForTesting(
            AddToHomescreenCoordinator addToHomescreenCoordinator) {
        mAddToHomescreenCoordinatorForTesting = addToHomescreenCoordinator;
    }

    private @Nullable AddToHomescreenCoordinator getAddToHomescreenCoordinator() {
        if (mAddToHomescreenCoordinatorForTesting != null) {
            return mAddToHomescreenCoordinatorForTesting;
        }

        WebContents webContents = getWebContentsForCurrentTab();
        if (webContents == null) return null;

        return new AddToHomescreenCoordinator(
                webContents,
                mContext,
                mWindowAndroid,
                assertNonNull(mModalDialogManagerSupplier.get()));
    }

    /* package */ void installButtonClicked(View view) {
        AddToHomescreenCoordinator addToHomescreenCoordinator = getAddToHomescreenCoordinator();
        if (addToHomescreenCoordinator == null) return;

        addToHomescreenCoordinator.showForAppMenu(AppMenuVerbiage.APP_MENU_OPTION_INSTALL);
    }

    /* package */ void setUrlFocusChangeInProgress(boolean inProgress) {
        if (mAutocompleteCoordinator == null || mUrlCoordinator == null) return;
        mIsUrlFocusChangeInProgress = inProgress;
        if (!inProgress) {
            updateButtonVisibility();

            // The accessibility bounding box is not properly updated when focusing the Omnibox
            // from the NTP fakebox.  Clearing/re-requesting focus triggers the bounding box to
            // be recalculated.
            if (didFocusUrlFromFakebox()
                    && mUrlHasFocus
                    && ChromeAccessibilityUtil.get().isAccessibilityEnabled()) {
                // TODO(crbug.com/475620206): likely an old workaround, consider removing.
                mAccessibilityFocusWorkaroundInProgress = true;
                mUrlCoordinator.clearFocus();
                mUrlCoordinator.requestFocus();
                mAccessibilityFocusWorkaroundInProgress = false;
                // Existing text (e.g. if the user pasted via the fakebox) from the fake box
                // should be restored after toggling the focus.
                if (mCurrentInput != null && !mCurrentInput.getUserText().isEmpty()) {
                    beginOrResumeInput(/* activateNewSession= */ false);
                }
            }

            mAutocompleteCoordinator.onUrlAnimationFinished();
            for (UrlFocusChangeListener listener : mUrlFocusChangeListeners) {
                listener.onUrlAnimationFinished(mUrlHasFocus);
            }
        }
    }

    /**
     * Handles any actions to be performed after all other actions triggered by the URL focus
     * change. This will be called after any animations are performed to transition from one focus
     * state to the other.
     *
     * @param showExpandedState Whether the url bar is expanded.
     * @param shouldShowKeyboard Whether the keyboard should be shown. This value is determined by
     *     whether url bar has got focus. Most of the time this is the same as showExpandedState,
     *     but in some cases, e.g. url bar is scrolled to the top of the screen on homepage but not
     *     focused, we set it differently.
     */
    /* package */ void finishUrlFocusChange(boolean showExpandedState, boolean shouldShowKeyboard) {
        if (mUrlCoordinator == null) return;
        mUrlCoordinator.setKeyboardVisibility(shouldShowKeyboard, true);
        setUrlFocusChangeInProgress(false);
        updateShouldAnimateIconChanges();
        if (!mIsTablet && !showExpandedState) {
            setUrlActionContainerVisibility(false);
        }
        if (mIsTablet) {
            float urlFocusChangeFraction = showExpandedState ? 1.0f : 0.0f;
            mLocationBarLayout.setUrlFocusChangePercent(
                    urlFocusChangeFraction, urlFocusChangeFraction, false);
        }
    }

    /**
     * Begins a new Omnibox input session.
     *
     * @param activateNewSession Whether to begin a new input session if one is not already active.
     *     Active input sessions show Autocomplete and focused Omnibox.
     */
    @EnsuresNonNullIf("mCurrentInput")
    @VisibleForTesting
    void beginOrResumeInput(boolean activateNewSession) {
        // Do not instantiate a new ephemeral session unless we're activating it as well.
        var session = FuseboxSessionState.from(mLocationBarDataProvider);

        // Target session must be either active, or activated.
        if (session == null || !(session.isSessionActive() || activateNewSession)) {
            endInputInternal();
            return;
        }

        // If we're switching tab (active -> active), just reanchor observer.
        if (mCurrentInput != null) {
            mCurrentInput.getRequestTypeSupplier().removeObserver(mAutocompleteRequestTypeObserver);
            mCurrentInput.getSiteSearchDataSupplier().removeObserver(mSiteSearchDataObserver);
        }
        // To avoid the async gap between now and on activate, null out here as well.
        setAttachmentModelList(null);

        // Acquire AutocompleteInput now, in case the `activate()` call below runs synchronously.
        // This guarantees that any calls to onSuggestionsChanged() will see the correct
        // mCurrentInput instance.
        mCurrentInput = session.getAutocompleteInput();
        // If the session is already in a specialized mode (e.g. IMAGE_GENERATION), we preserve it.
        // Otherwise, we apply the default request type for the current page.
        if (mCurrentInput.getRequestType() == AutocompleteRequestType.SEARCH) {
            mCurrentInput.setRequestType(mLocationBarDataProvider.getDefaultRequestType());
        }

        session.activate(
                mContext,
                mLocationBarDataProvider.getWebContents(),
                mProfileSupplier,
                () -> {
                    if (mAutocompleteCoordinator == null || mCurrentInput == null) return;
                    if (mScrimHandler != null) {
                        mScrimHandler.updateScrimVisualState();
                        mScrimHandler.setVisibility(
                                mCurrentInput.getAutocompleteState() == AutocompleteState.ENABLED);
                    }
                    mAutocompleteCoordinator.beginInput(session);
                    mFuseboxCoordinator.beginInput(session);
                    mStatusCoordinator.beginInput(session);
                    // Trigger animation now that we have an up-to-date value for the fusebox state.
                    setupSuggestionsListShowAnimation();
                    setAttachmentModelList(session.getFuseboxAttachmentModelList());
                });

        mCurrentInput
                .getRequestTypeSupplier()
                .addSyncObserverAndCallIfNonNull(mAutocompleteRequestTypeObserver);
        mCurrentInput
                .getSiteSearchDataSupplier()
                .addSyncObserverAndCallIfNonNull(mSiteSearchDataObserver);

        UrlBarData data = getUrlBarDataForCurrentInput(mCurrentInput);
        mUrlCoordinator.setUrlBarData(
                data, UrlBar.ScrollType.NO_SCROLL, mCurrentInput.getSelection());
        updateButtonVisibility();

        // Serve the cached suggestions while we wait for Profile.
        if (mCurrentInput.isInCacheableContext() && mAutocompleteCoordinator != null) {
            mAutocompleteCoordinator.serveCachedZeroSuggest(mCurrentInput);
        }
    }

    @VisibleForTesting
    /* package */ UrlBarData getUrlBarDataForCurrentInput(
            @Nullable AutocompleteInput currentInput) {
        if (currentInput == null) return UrlBarData.EMPTY;

        String userText = currentInput.getUserText();
        if (!TextUtils.isEmpty(userText)
                && TextUtils.equals(userText, currentInput.getInitialUserText())) {
            if (ContextualTasksUtils.isContextualTasksUrl(
                    mLocationBarDataProvider.getCurrentGurl())) {
                WebContents webContents = mLocationBarDataProvider.getWebContents();
                if (webContents != null) {
                    GURL contextualTaskDisplayUrl =
                            ContextualTasksUtils.getContextualTasksDisplayUrl(webContents);
                    return UrlBarData.forUrlAndText(
                            mLocationBarDataProvider.getCurrentGurl(),
                            contextualTaskDisplayUrl.getSpec());
                }
            }
            return UrlBarData.forUrlAndText(currentInput.getPageUrl(), userText);
        }
        return UrlBarData.forNonUrlText(userText);
    }

    private void setupSuggestionsListShowAnimation() {
        if (mAutocompleteCoordinator == null) return;
        @Nullable Animator autocompleteAnimator =
                mAutocompleteCoordinator.setupSuggestionsListShowAnimation();
        if (autocompleteAnimator == null) return;
        mLocationBarLayout.setAlpha(0.0f);
        ObjectAnimator alphaAnimator =
                ObjectAnimator.ofFloat(mLocationBarLayout, View.ALPHA, 0.0f, 1.0f);
        alphaAnimator.addListener(
                new AnimatorListenerAdapter() {
                    @Override
                    public void onAnimationCancel(Animator animation) {
                        mLocationBarLayout.setAlpha(1.0f);
                    }
                });

        AnimatorSet animatorSet = new AnimatorSet();
        animatorSet.playTogether(alphaAnimator, autocompleteAnimator);
        animatorSet.setDuration(POPOVER_FADE_DURATION_MS);
        animatorSet.setInterpolator(Interpolators.LINEAR_INTERPOLATOR);
        animatorSet.start();
    }

    /** Ends the current Omnibox input session. */
    private void endInputInternal() {
        if (mAutocompleteCoordinator == null || mCurrentInput == null || mIsReparenting) return;
        if (mFuseboxCoordinator.getFuseboxLayoutModeSupplier().get()
                        == FuseboxLayoutMode.SUGGESTIONS_POPOVER
                && isParentedToSuggestionsContainer()) {
            reparentToToolbar();
        }
        mAutocompleteCoordinator.endInput();

        mStatusCoordinator.endInput();

        if (mScrimHandler != null) mScrimHandler.setVisibility(false);
        mCurrentInput.getRequestTypeSupplier().removeObserver(mAutocompleteRequestTypeObserver);
        mCurrentInput.getSiteSearchDataSupplier().removeObserver(mSiteSearchDataObserver);
        FuseboxSessionState state = FuseboxSessionState.from(mLocationBarDataProvider);
        if (state != null) {
            state.deactivate();
            // Only for Contextual Tasks, we skip ending the Fusebox input to allow it to stay warm
            // in compact mode.
            if (!state.isContextualTasksState()) {
                mFuseboxCoordinator.endInput();
            }
        }

        mCurrentInput = null;
        // The hint text depends on mCurrentInput, nulling it may change the outcome.
        onSearchBoxHintTextChanged();

        setAttachmentModelList(null);
    }

    @VisibleForTesting
    boolean isParentedToSuggestionsContainer() {
        return mToolbarParent != null;
    }

    private void reparentToSuggestionsContainer() {
        if (mAutocompleteCoordinator == null
                || mAutocompleteCoordinator.getSuggestionsContainer() == null
                || isPageClassIneligibleForPopover()) {
            return;
        }

        mIsReparenting = true;
        mUrlCoordinator.startReparenting();
        OmniboxSuggestionsContainer suggestionsContainer =
                mAutocompleteCoordinator.getSuggestionsContainer();
        MarginLayoutParams marginLayoutParams =
                (MarginLayoutParams) mLocationBarLayout.getLayoutParams();
        marginLayoutParams.width = MarginLayoutParams.MATCH_PARENT;
        marginLayoutParams.height = MarginLayoutParams.MATCH_PARENT;
        mToolbarParent = (ViewGroup) mLocationBarLayout.getParent();
        mIndexInToolbar = mToolbarParent.indexOfChild(mLocationBarLayout);
        mToolbarParent.removeView(mLocationBarLayout);
        suggestionsContainer.addView(mLocationBarLayout, 0, marginLayoutParams);
        mDropdown = suggestionsContainer.takeDropdownView();
        int dropdownIndex =
                mLocationBarLayout.indexOfChild(
                        mLocationBarLayout.findViewById(R.id.suggestions_container_placeholder));
        mLocationBarLayout.addView(mDropdown, dropdownIndex);
        ConstraintSet set = new ConstraintSet();
        set.clone(mLocationBarLayout);

        set.connect(mDropdown.getId(), ConstraintSet.TOP, R.id.url_bar, ConstraintSet.BOTTOM);
        set.connect(
                mDropdown.getId(),
                ConstraintSet.BOTTOM,
                R.id.location_bar_attachments_add,
                ConstraintSet.TOP);
        set.connect(
                mDropdown.getId(),
                ConstraintSet.START,
                ConstraintSet.PARENT_ID,
                ConstraintSet.START);
        set.connect(
                mDropdown.getId(), ConstraintSet.END, ConstraintSet.PARENT_ID, ConstraintSet.END);
        set.connect(
                R.id.navigate_button, ConstraintSet.TOP, mDropdown.getId(), ConstraintSet.BOTTOM);
        set.connect(
                R.id.location_bar_attachments_add,
                ConstraintSet.TOP,
                mDropdown.getId(),
                ConstraintSet.BOTTOM);
        set.connect(R.id.delete_button, ConstraintSet.TOP, R.id.url_bar, ConstraintSet.TOP);
        set.connect(R.id.delete_button, ConstraintSet.BOTTOM, R.id.url_bar, ConstraintSet.BOTTOM);
        set.constrainWidth(mDropdown.getId(), ConstraintSet.MATCH_CONSTRAINT);
        set.constrainHeight(mDropdown.getId(), ConstraintSet.WRAP_CONTENT);
        set.constrainedHeight(mDropdown.getId(), true);
        set.applyTo(mLocationBarLayout);
        mUrlCoordinator.finishReparenting(true);
        mIsReparenting = false;
    }

    private void reparentToToolbar() {
        if (mAutocompleteCoordinator == null
                || mAutocompleteCoordinator.getSuggestionsContainer() == null
                || isPageClassIneligibleForPopover()) {
            return;
        }

        mIsReparenting = true;
        mUrlCoordinator.startReparenting();
        MarginLayoutParams marginLayoutParams =
                (MarginLayoutParams) mLocationBarLayout.getLayoutParams();
        ViewGroup suggestionsContainer = mAutocompleteCoordinator.getSuggestionsContainer();
        suggestionsContainer.removeView(mLocationBarLayout);
        assertNonNull(mToolbarParent)
                .addView(mLocationBarLayout, mIndexInToolbar, marginLayoutParams);
        if (mLocationBarLayout.isInLayout()) {
            PostTask.postDelayedTask(
                    TaskTraits.UI_USER_VISIBLE, () -> mLocationBarLayout.removeView(mDropdown), 0);
        } else {
            mLocationBarLayout.removeView(mDropdown);
        }
        mToolbarParent = null;
        mUrlCoordinator.finishReparenting(false);
        mIsReparenting = false;
    }

    private boolean isPageClassIneligibleForPopover() {
        if (mLocationBarDataProvider == null) return false;

        int pageClass = mLocationBarDataProvider.getPageClassification(false);
        return pageClass == PageClassification.ANDROID_SHORTCUTS_WIDGET_VALUE
                || pageClass == PageClassification.ANDROID_SEARCH_WIDGET_VALUE
                || pageClass == PageClassification.ANDROID_HUB_VALUE
                || pageClass == PageClassification.JUMP_START_VALUE;
    }

    /**
     * Sets a listener to be notified when a specialized Fusebox mode is activated.
     *
     * @param onSpecializedFuseboxModeActivatedCallback The callback to be invoked with a boolean
     *     indicating whether a specialized mode is active.
     */
    /* package */ void setOnSpecializedFuseboxModeActivatedListener(
            @Nullable Callback<Boolean> onSpecializedFuseboxModeActivatedCallback) {
        mOnSpecializedFuseboxModeActivatedCallback = onSpecializedFuseboxModeActivatedCallback;
    }

    /**
     * Handle and run any necessary animations that are triggered off focusing the UrlBar.
     *
     * @param hasFocus Whether focus was gained.
     */
    @VisibleForTesting
    /* package */ void handleUrlFocusAnimation(boolean hasFocus) {
        @FuseboxLayoutMode
        int layoutMode = mFuseboxCoordinator.getFuseboxLayoutModeSupplier().get();
        if (layoutMode == FuseboxLayoutMode.SUGGESTIONS_POPOVER
                && hasFocus
                && !isParentedToSuggestionsContainer()) {
            reparentToSuggestionsContainer();
        }
        mLocationBarLayout.setFuseboxLayoutMode(layoutMode);

        if (hasFocus) {
            mUrlFocusedWithoutAnimations = false;
        }

        // Propagate signals to AutocompleteCoordinator ahead of everyone else.
        // Autocomplete requires certain signals, such as AutocompleteRequestType
        // and PageClassification to be correct throughout from the moment the focus
        // is gained to the moment the focus is lost.
        //
        // This call is permitted to happen before anyone else is activated, and
        // must be called before everyone else cleans up.
        if (hasFocus) {
            // Session may be pre-focused from the NTP.
            if (mCurrentInput == null) {
                beginOrResumeInput(/* activateNewSession= */ true);
            }
        } else {
            endInputInternal();
        }

        for (UrlFocusChangeListener listener : mUrlFocusChangeListeners) {
            listener.onUrlFocusChange(hasFocus);
        }

        // The focus animation for phones is driven by ToolbarPhone, so we don't currently have any
        // phone-specific animation logic in this class.
        if (mIsTablet) {
            if (mUrlFocusChangeAnimator != null && mUrlFocusChangeAnimator.isRunning()) {
                mUrlFocusChangeAnimator.cancel();
                mUrlFocusChangeAnimator = null;
            }

            if (mLocationBarDataProvider.getNewTabPageDelegate().isCurrentlyVisible()) {
                finishUrlFocusChange(hasFocus, /* shouldShowKeyboard= */ hasFocus);
                return;
            }

            mLocationBarLayout.getRootView().getLocalVisibleRect(mRootViewBounds);
            float screenSizeRatio =
                    (mRootViewBounds.height()
                            / (float) Math.max(mRootViewBounds.height(), mRootViewBounds.width()));
            mUrlFocusChangeAnimator =
                    ObjectAnimator.ofFloat(
                            this, mUrlFocusChangeFractionProperty, hasFocus ? 1f : 0f);
            mUrlFocusChangeAnimator.setDuration(
                    (long) (NTP_KEYBOARD_FOCUS_DURATION_MS * screenSizeRatio));
            mUrlFocusChangeAnimator.addListener(
                    new CancelAwareAnimatorListener() {
                        @Override
                        public void onEnd(Animator animator) {
                            finishUrlFocusChange(hasFocus, /* shouldShowKeyboard= */ hasFocus);
                        }

                        @Override
                        public void onCancel(Animator animator) {
                            setUrlFocusChangeInProgress(false);
                        }
                    });
            mUrlFocusChangeAnimator.start();
        }
    }

    /* package */ void setShouldShowMicButtonWhenUnfocusedForPhone(boolean shouldShow) {
        assert !mIsTablet;
        mShouldShowMicButtonWhenUnfocused = shouldShow;
    }

    /* package */ void setShouldShowLensButtonWhenUnfocusedForPhone(boolean shouldShow) {
        assert !mIsTablet;
        mShouldShowLensButtonWhenUnfocused = shouldShow;
    }

    /* package */ void setMiniOriginMode(boolean active) {
        mMiniOriginMode = active;
        updateBackButtonVisibility();
    }

    /* package */ void setShouldShowMicButtonWhenUnfocusedForTesting(boolean shouldShow) {
        assert mIsTablet;
        mShouldShowMicButtonWhenUnfocused = shouldShow;
    }

    /**
     * @param shouldShow Whether buttons should be displayed in the URL bar when it's not focused.
     */
    /* package */ void setShouldShowButtonsWhenUnfocusedForTablet(boolean shouldShow) {
        assert mIsTablet;
        mShouldShowButtonsWhenUnfocused = shouldShow;
        updateButtonVisibility();
    }

    /**
     * @param button The {@link View} of the button to show. Returns An animator to run for the
     *     given view when showing buttons in the unfocused location bar. This should also be used
     *     to create animators for showing toolbar buttons.
     */
    /* package */ ObjectAnimator createShowButtonAnimatorForTablet(View button) {
        assert mIsTablet;
        if (button.getVisibility() != View.VISIBLE) {
            button.setAlpha(0.f);
        }
        ObjectAnimator buttonAnimator = ObjectAnimator.ofFloat(button, View.ALPHA, 1.f);
        buttonAnimator.setInterpolator(Interpolators.LINEAR_OUT_SLOW_IN_INTERPOLATOR);
        buttonAnimator.setStartDelay(ICON_FADE_ANIMATION_DELAY_MS);
        buttonAnimator.setDuration(ICON_FADE_ANIMATION_DURATION_MS);
        return buttonAnimator;
    }

    /**
     * @param button The {@link View} of the button to hide. Returns An animator to run for the
     *     given view when hiding buttons in the unfocused location bar. This should also be used to
     *     create animators for hiding toolbar buttons.
     */
    /* package */ ObjectAnimator createHideButtonAnimatorForTablet(View button) {
        assert mIsTablet;
        ObjectAnimator buttonAnimator = ObjectAnimator.ofFloat(button, View.ALPHA, 0.f);
        buttonAnimator.setInterpolator(Interpolators.FAST_OUT_LINEAR_IN_INTERPOLATOR);
        buttonAnimator.setDuration(ICON_FADE_ANIMATION_DURATION_MS);
        return buttonAnimator;
    }

    /**
     * Creates animators for showing buttons in the unfocused tablet location bar. The buttons fade
     * in while the width of the location bar decreases. There are toolbar buttons that show at the
     * same time, causing the width of the location bar to change.
     *
     * @param toolbarStartPaddingDifference The difference in the toolbar's start padding between
     *     the beginning and end of the animation.
     * @return A List of animators to run.
     */
    /* package */ List<Animator> getShowButtonsWhenUnfocusedAnimatorsForTablet(
            int toolbarStartPaddingDifference) {
        assert mIsTablet;
        LocationBarTablet locationBarTablet = ((LocationBarTablet) mLocationBarLayout);

        ArrayList<Animator> animators = new ArrayList<>();

        Animator widthChangeAnimator =
                ObjectAnimator.ofFloat(this, mWidthChangeFractionPropertyTablet, 0f);
        widthChangeAnimator.setDuration(WIDTH_CHANGE_ANIMATION_DURATION_MS);
        widthChangeAnimator.setInterpolator(Interpolators.FAST_OUT_SLOW_IN_INTERPOLATOR);
        widthChangeAnimator.addListener(
                new AnimatorListenerAdapter() {
                    @Override
                    public void onAnimationStart(Animator animation) {
                        locationBarTablet.startAnimatingWidthChange(toolbarStartPaddingDifference);
                        setShouldShowButtonsWhenUnfocusedForTablet(true);
                    }

                    @Override
                    public void onAnimationEnd(Animator animation) {
                        // Only reset values if the animation is ending because it's completely
                        // finished and not because it was canceled.
                        if (locationBarTablet.getWidthChangeFraction() == 0.f) {
                            locationBarTablet.finishAnimatingWidthChange();
                            locationBarTablet.resetValuesAfterAnimation();
                        }
                    }
                });
        animators.add(widthChangeAnimator);

        // When buttons show in the unfocused location bar, either the delete button or bookmark
        // button will be showing. If the delete button is currently showing, the bookmark button
        // should not fade in.
        if (!locationBarTablet.isDeleteButtonVisible()) {
            animators.add(
                    createShowButtonAnimatorForTablet(
                            locationBarTablet.getBookmarkButtonForAnimation()));
        }

        if (!locationBarTablet.isMicButtonVisible()
                || locationBarTablet.getMicButtonAlpha() != 1.f) {
            // If the microphone button is already fully visible, don't animate its appearance.
            animators.add(
                    createShowButtonAnimatorForTablet(
                            locationBarTablet.getMicButtonForAnimation()));
        }
        if (shouldShowLensButton()
                && (!locationBarTablet.isLensButtonVisible()
                        || locationBarTablet.getLensButtonAlpha() != 1.f)) {
            // If the Lens button is already fully visible, don't animate its appearance.
            animators.add(
                    createShowButtonAnimatorForTablet(
                            locationBarTablet.getLensButtonForAnimation()));
        }

        return animators;
    }

    /**
     * Creates animators for hiding buttons in the unfocused tablet location bar. The buttons fade
     * out while the width of the location bar increases. There are toolbar buttons that hide at the
     * same time, causing the width of the location bar to change.
     *
     * @param toolbarStartPaddingDifference The difference in the toolbar's start padding between
     *     the beginning and end of the animation.
     * @return A List of animators to run.
     */
    /* package */ List<Animator> getHideButtonsWhenUnfocusedAnimatorsForTablet(
            int toolbarStartPaddingDifference) {
        LocationBarTablet locationBarTablet = ((LocationBarTablet) mLocationBarLayout);

        ArrayList<Animator> animators = new ArrayList<>();

        Animator widthChangeAnimator =
                ObjectAnimator.ofFloat(this, mWidthChangeFractionPropertyTablet, 1f);
        widthChangeAnimator.setStartDelay(WIDTH_CHANGE_ANIMATION_DELAY_MS);
        widthChangeAnimator.setDuration(WIDTH_CHANGE_ANIMATION_DURATION_MS);
        widthChangeAnimator.setInterpolator(Interpolators.FAST_OUT_SLOW_IN_INTERPOLATOR);
        widthChangeAnimator.addListener(
                new AnimatorListenerAdapter() {
                    @Override
                    public void onAnimationStart(Animator animation) {
                        locationBarTablet.startAnimatingWidthChange(toolbarStartPaddingDifference);
                    }

                    @Override
                    public void onAnimationEnd(Animator animation) {
                        // Only reset values if the animation is ending because it's completely
                        // finished and not because it was canceled.
                        if (locationBarTablet.getWidthChangeFraction() == 1.f) {
                            locationBarTablet.finishAnimatingWidthChange();
                            locationBarTablet.resetValuesAfterAnimation();
                            setShouldShowButtonsWhenUnfocusedForTablet(false);
                        }
                    }
                });
        animators.add(widthChangeAnimator);

        // When buttons show in the unfocused location bar, either the delete button or bookmark
        // button will be showing. If the delete button is currently showing, the bookmark button
        // should not fade out.
        if (!locationBarTablet.isDeleteButtonVisible()) {
            animators.add(
                    createHideButtonAnimatorForTablet(
                            locationBarTablet.getBookmarkButtonForAnimation()));
        }

        if (!(mUrlHasFocus && !locationBarTablet.isDeleteButtonVisible())) {
            // The microphone button always shows when buttons are shown in the unfocused location
            // bar. When buttons are hidden in the unfocused location bar, the microphone shows if
            // the location bar is focused and the delete button isn't showing. The microphone
            // button should not be hidden if the url bar is currently focused and the delete button
            // isn't showing.
            animators.add(
                    createHideButtonAnimatorForTablet(
                            locationBarTablet.getMicButtonForAnimation()));
            if (shouldShowLensButton()) {
                animators.add(
                        createHideButtonAnimatorForTablet(
                                locationBarTablet.getLensButtonForAnimation()));
            }
        }

        return animators;
    }

    /**
     * Changes the text on the url bar. The text update will be applied regardless of the current
     * focus state (comparing to {@link LocationBarMediator#setUrl} which only applies text updates
     * when not focused).
     *
     * @param urlBarData The contents of the URL bar, both for editing and displaying.
     * @param scrollType Specifies how the text should be scrolled in the unfocused state.
     * @param selection Specifies the range of text to be selected in the focused state.
     * @return Whether the URL was changed as a result of this call.
     */
    /* package */ boolean setUrlBarText(
            UrlBarData urlBarData, @UrlBar.ScrollType int scrollType, Range<Integer> selection) {
        return mUrlCoordinator.setUrlBarData(urlBarData, scrollType, selection);
    }

    /**
     * Requests the URL focus.
     *
     * <p>Notifies listeners that the URL focus is about to be requested.
     */
    /* package */ void requestUrlFocus() {
        for (UrlFocusChangeListener listener : mUrlFocusChangeListeners) {
            listener.onUrlFocusWillBeRequested(mLocationBarDataProvider.getTab());
        }
        mUrlCoordinator.requestFocus();
    }

    // Private methods

    @VisibleForTesting
    void setProfile(Profile profile) {
        if (!mNativeInitialized) return;

        assumeNonNull(mOmniboxPrerender);
        mOmniboxPrerender.initializeForProfile(profile);

        if (mSearchEngineUtils != null) {
            mSearchEngineUtils.removeSearchBoxHintTextObserver(this);
        }

        mSearchEngineUtils = SearchEngineUtils.getForProfile(profile);
        mSearchEngineUtils.addSearchBoxHintTextObserver(this);
        mLocationBarLayout.setSearchEngineUtils(mSearchEngineUtils);
    }

    /**
     * Used when suggestions are done and it's time to pass focus back to the tab. However caution
     * needs to be exercised when calling this from cases where input session is intentionally
     * active (e.g. NTP on Desktop) such that we don't accidentally clear Omnibox input when, say,
     * returning to the NTP.
     */
    private void endInputAndFocusCurrentTab() {
        try (TraceEvent traceEvent =
                TraceEvent.scoped("LocationBarMediator.endInputAndFocusCurrentTab")) {
            assert mLocationBarDataProvider != null;
            if (mLocationBarDataProvider.hasTab()) {
                View view = assumeNonNull(mLocationBarDataProvider.getTab()).getView();
                if (mCurrentInput != null) endInput();
                if (view != null) view.requestFocus();
            }
        }
    }

    /** Update visuals to use a correct color scheme depending on the primary color. */
    @VisibleForTesting
    /* package */ void updateBrandedColorScheme() {
        @BrandedColorScheme
        int newScheme =
                OmniboxResourceProvider.getBrandedColorScheme(
                        mContext,
                        mLocationBarDataProvider.isIncognitoBranded(),
                        getPrimaryBackgroundColor());
        if (newScheme == mBrandedColorScheme && mHasEverUpdatedBrandedColorScheme) return;
        mHasEverUpdatedBrandedColorScheme = true;
        mBrandedColorScheme = newScheme;

        // The delete button only appears when the url bar has focus, so its tint is rather static,
        // and need not be assigned in updateButtonTints().
        mLocationBarLayout.setDeleteButtonTint(
                ThemeUtils.getThemedToolbarIconTint(mContext, mBrandedColorScheme));
        mLocationBarLayout.updateVisualsForState(mBrandedColorScheme);
        mUrlCoordinator.setBrandedColorScheme(mBrandedColorScheme);
        // This sets spans inside the data object that override the color.
        updateUrl();
        mStatusCoordinator.setBrandedColorScheme(mBrandedColorScheme);
        if (mAutocompleteCoordinator != null) {
            mAutocompleteCoordinator.updateVisualsForState(mBrandedColorScheme);
        }
        mFuseboxCoordinator.updateVisualsForState(mBrandedColorScheme);
    }

    /** Returns the primary color based on the url focus, and incognito state. */
    private int getPrimaryBackgroundColor() {
        // If the url bar is focused, the toolbar background color is the default color regardless
        // of whether it is branded or not.
        if (isUrlBarFocused()) {
            return ChromeColors.getDefaultThemeColor(
                    mContext, mLocationBarDataProvider.isIncognitoBranded());
        } else {
            return mLocationBarDataProvider.getPrimaryColor();
        }
    }

    private void updateShouldAnimateIconChanges() {
        boolean isToolbarTopAnchored =
                mBrowserControlsStateProvider != null
                        && mBrowserControlsStateProvider.getControlsPosition()
                                == ControlsPosition.TOP;
        boolean shouldAnimate =
                mIsTablet
                        ? isUrlBarFocused()
                        : isToolbarTopAnchored
                                && (isUrlBarFocused() || mIsUrlFocusChangeInProgress);
        mStatusCoordinator.setShouldAnimateIconChanges(shouldAnimate);
    }

    private void recordOmniboxFocusReason(@OmniboxFocusReason int reason) {
        RecordHistogram.recordEnumeratedHistogram(
                "Android.OmniboxFocusReason", reason, OmniboxFocusReason.NUM_ENTRIES);
    }

    /**
     * Updates the display of the mic button.
     *
     * @param notifyEmbedder Whether the {@link LocationBarEmbedder} should be notified of the
     *     visibility change. If false, the caller has the responsibility of alerting the embedder,
     *     if appropriate (e.g. update the embedder once after several visibility changes at once).
     */
    private void updateMicButtonVisibility(boolean notifyEmbedder) {
        boolean shouldShowMicButton = shouldShowMicButton();
        setMicButtonVisibility(shouldShowMicButton);
        if (notifyEmbedder) {
            mLocationBarEmbedder.onWidthConsumerVisibilityChanged();
        }
    }

    private void setMicButtonVisibility(boolean shouldShowMicButton) {
        boolean showMicButton =
                shouldShowMicButton && mMicButtonToolbarWidthConsumer.hasSpaceToShow();

        if (mPreviousMicButtonVisible == null || showMicButton != mPreviousMicButtonVisible) {
            LocationBarMetrics.recordMicButtonShown(showMicButton);
        }
        mPreviousMicButtonVisible = showMicButton;

        mLocationBarLayout.setMicButtonVisibility(showMicButton);
    }

    /**
     * Updates the display of the lens button.
     *
     * @param notifyEmbedder Whether the {@link LocationBarEmbedder} should be notified of the
     *     visibility change. If false, the caller has the responsibility of alerting the embedder,
     *     if appropriate (e.g. update the embedder once after several visibility changes at once).
     */
    private void updateLensButtonVisibility(boolean notifyEmbedder) {
        boolean shouldShowLensButton = shouldShowLensButton();
        setLensButtonVisibility(shouldShowLensButton);
        if (notifyEmbedder) {
            mLocationBarEmbedder.onWidthConsumerVisibilityChanged();
        }
    }

    private void setLensButtonVisibility(boolean shouldShowLensButton) {
        boolean actualLensButtonVisible =
                shouldShowLensButton && mLensButtonToolbarWidthConsumer.hasSpaceToShow();

        if (mPreviousLensButtonVisible == null
                || actualLensButtonVisible != mPreviousLensButtonVisible) {
            LensMetrics.recordShown(LensEntryPoint.OMNIBOX, actualLensButtonVisible);
        }
        mPreviousLensButtonVisible = actualLensButtonVisible;
        mLocationBarLayout.setLensButtonVisibility(actualLensButtonVisible);
    }

    // Note - the delete button is never restricted by the toolbar width availability, so the
    // embedder does not need to be updated after its visibility changes.
    private void updateDeleteButtonVisibility() {
        boolean showDeleteButton =
                isUrlBarFocusedWithUserInput()
                        && !OmniboxCapabilities.hasDesktopExperience(mContext);
        if (isParentedToSuggestionsContainer()) {
            showDeleteButton |=
                    (mCurrentInput != null && !mCurrentInput.isConventionalRequestType());
        }

        if (mPreviousDeleteButtonVisible == null
                || showDeleteButton != mPreviousDeleteButtonVisible) {
            LocationBarMetrics.recordDeleteButtonShown(showDeleteButton);
        }
        mPreviousDeleteButtonVisible = showDeleteButton;

        mLocationBarLayout.setDeleteButtonVisibility(showDeleteButton);
    }

    /* package */ void updateBackButtonVisibility() {
        Tab tab = mLocationBarDataProvider.getTab();
        if (tab == null) {
            mLocationBarLayout.setBackButtonVisibility(false);
            return;
        }
        boolean isNtp = (tab.getUrl() != null) && UrlUtilities.isNtpUrl(tab.getUrl());

        boolean showBackButton =
                ToolbarVariationUtils.isToolbarUiRefactorEnabled(mContext)
                        && ToolbarVariationUtils.shouldBackButtonBeInOmnibox()
                        && !mMiniOriginMode
                        && !mUrlHasFocus
                        && !isNtp;
        mLocationBarLayout.setBackButtonVisibility(showBackButton);
        mLocationBarLayout.setBackButtonEnabled(tab.canGoBack());
    }

    /* package */ void onBackButtonClicked() {
        Tab tab = mLocationBarDataProvider.getTab();
        if (tab != null && tab.canGoBack()) {
            tab.goBack();
        }
    }

    /**
     * @see FuseboxAttachmentChangeListener#onAttachmentsListChanged()
     */
    @Override
    public void onAttachmentListChanged() {
        updateNavigateButtonVisibility();
    }

    private void onFuseboxStateChanged(@FuseboxState int state) {
        updateNavigateButtonVisibility();
        mLocationBarLayout.onFuseboxStateChanged(state);
    }

    private void onFuseboxInteractionCompleted(boolean actionTaken) {
        AutocompleteInput currentInput = mCurrentInput;
        if (currentInput == null
                || currentInput.getAutocompleteState() != AutocompleteState.STANDBY_NO_FOCUS) {
            return;
        }

        if (actionTaken) {
            currentInput.setAutocompleteState(AutocompleteState.ENABLED);
            currentInput.setFocusReason(OmniboxFocusReason.FAKE_BOX_TAP);
            beginOrResumeInput(/* activateNewSession= */ false);
            requestUrlFocus();
        } else {
            // When no action is taken, just end the input session since the omnibox won't be
            // focused.
            endInput();
        }
    }

    private void updateNavigateButtonVisibility() {
        // TODO(crbug.com/464003589): Update the hasTextOrAttachments to include
        // getAttachmentsPresentSupplier check.
        boolean hasTextOrAttachments =
                !TextUtils.isEmpty(mUrlCoordinator.getTextWithAutocomplete());
        // TODO(crbug.com/464003589): || mFuseboxCoordinator.getAttachmentsPresentSupplier().get();
        boolean isExpandedFusebox =
                mFuseboxCoordinator.getFuseboxStateSupplier().get() == FuseboxState.EXPANDED;
        boolean navigateButtonVisible = mUrlHasFocus && isExpandedFusebox && hasTextOrAttachments;
        mLocationBarLayout.setNavigateButtonVisibility(navigateButtonVisible);
    }

    /* package */ void onZoomLevelChanged() {
        updateZoomButtonVisibility(/* notifyEmbedder= */ true);
    }

    @VisibleForTesting
    boolean shouldShowZoomButton() {
        if (mUrlHasFocus || mIsUrlFocusChangeInProgress) return false;
        if (!mIsTablet
                || mPageZoomIndicatorCoordinator == null
                || getWebContentsForCurrentTab() == null
                || mPageZoomIndicatorCoordinator.isZoomLevelDefault()) {
            return false;
        }
        return !mPageZoomIndicatorCoordinator.isZoomLevelDefault()
                || mPageZoomIndicatorCoordinator.isPopupWindowShowing();
    }

    /**
     * Updates the display of the zoom button.
     *
     * @param notifyEmbedder Whether the {@link LocationBarEmbedder} should be notified of the
     *     visibility change. If false, the caller has the responsibility of alerting the embedder,
     *     if appropriate (e.g. update the embedder once after several visibility changes at once).
     */
    private void updateZoomButtonVisibility(boolean notifyEmbedder) {
        if (mPageZoomIndicatorCoordinator == null) return;

        setZoomButtonVisibility(shouldShowZoomButton());
        if (notifyEmbedder) {
            // Embedder will handle visibility changes.
            mLocationBarEmbedder.onWidthConsumerVisibilityChanged();
        }
    }

    private void setZoomButtonVisibility(boolean shouldShowZoomButton) {
        boolean showZoomButton =
                shouldShowZoomButton && mZoomButtonToolbarWidthConsumer.hasSpaceToShow();

        if (mPreviousZoomButtonVisible == null || showZoomButton != mPreviousZoomButtonVisible) {
            LocationBarMetrics.recordZoomButtonShown(showZoomButton);
        }
        mPreviousZoomButtonVisible = showZoomButton;

        mLocationBarLayout.setZoomButtonVisibility(showZoomButton);
    }

    public void updateZoomButtonVisibilityForTesting() {
        updateZoomButtonVisibility(/* notifyEmbedder= */ true);
    }

    public ToolbarWidthConsumer getBookmarkButtonToolbarWidthConsumerForTesting() {
        return mBookmarkButtonToolbarWidthConsumer;
    }

    private @Nullable WebContents getWebContentsForCurrentTab() {
        Tab currentTab = mLocationBarDataProvider.getTab();
        if (currentTab == null) return null;

        return currentTab.getWebContents();
    }

    /** Called every time the installability result on `manager` changes. */
    @Override
    public void onInstallabilityUpdated(AppBannerManager manager) {
        WebContents webContents = getWebContentsForCurrentTab();
        if (webContents == null || manager != AppBannerManager.forWebContents(webContents)) return;

        updateInstallButtonVisibility(/* notifyEmbedder= */ true);
    }

    /**
     * Returns true if the install button should be shown based on user focus and app promotability
     * status. The button is always hidden on phones since it is part of the URL action container
     * and is only shown when the omnibox is unfocused; the URL action container is hidden when the
     * omnibox is unfocused on phones.
     */
    @VisibleForTesting
    boolean shouldShowInstallButton() {
        if (mUrlHasFocus || mIsUrlFocusChangeInProgress) return false;

        WebContents webContents = getWebContentsForCurrentTab();
        if (webContents == null) return false;

        return AppBannerManager.isProbablyPromotable(webContents);
    }

    /**
     * Updates the visibility of the install button.
     *
     * @param notifyEmbedder Whether the {@link LocationBarEmbedder} should be notified of the
     *     visibility change. If false, the caller has the responsibility of alerting the embedder,
     *     if appropriate (e.g. update the embedder once after several visibility changes at once).
     */
    private void updateInstallButtonVisibility(boolean notifyEmbedder) {
        setInstallButtonVisibility(shouldShowInstallButton());
        if (notifyEmbedder) {
            mLocationBarEmbedder.onWidthConsumerVisibilityChanged();
        }
    }

    private void setInstallButtonVisibility(boolean shouldShowInstallButton) {
        boolean showInstallButton =
                shouldShowInstallButton && mInstallButtonToolbarWidthConsumer.hasSpaceToShow();

        if (mPreviousInstallButtonVisible == null
                || showInstallButton != mPreviousInstallButtonVisible) {
            LocationBarMetrics.recordInstallButtonShown(showInstallButton);
        }
        mPreviousInstallButtonVisible = showInstallButton;

        mLocationBarLayout.setInstallButtonVisibility(showInstallButton);
    }

    /**
     * Updates the visibility of the bookmark button.
     *
     * @param notifyEmbedder Whether the {@link LocationBarEmbedder} should be notified of the
     *     visibility change. If false, the caller has the responsibility of alerting the embedder,
     *     if appropriate (e.g. update the embedder once after several visibility changes at once).
     */
    private void updateBookmarkButtonVisibility(boolean notifyEmbedder) {
        setBookmarkButtonVisibility(shouldShowBookmarkButton());
        if (notifyEmbedder) {
            mLocationBarEmbedder.onWidthConsumerVisibilityChanged();
        }
    }

    private void setBookmarkButtonVisibility(boolean shouldShowBookmarkButton) {
        LocationBarTablet locationBarTablet = (LocationBarTablet) mLocationBarLayout;
        boolean showBookmarkButton =
                shouldShowBookmarkButton && mBookmarkButtonToolbarWidthConsumer.hasSpaceToShow();

        if (mPreviousBookmarkButtonVisible == null
                || showBookmarkButton != mPreviousBookmarkButtonVisible) {
            LocationBarMetrics.recordBookmarkButtonShown(showBookmarkButton);
        }
        mPreviousBookmarkButtonVisible = showBookmarkButton;

        locationBarTablet.setBookmarkButtonVisibility(showBookmarkButton);
    }

    @VisibleForTesting
    boolean shouldShowBookmarkButton() {
        return mShouldShowButtonsWhenUnfocused && shouldShowPageActionButtons();
    }

    /** Returns Whether the url bar is focused and has non-empty input. */
    @VisibleForTesting
    boolean isUrlBarFocusedWithUserInput() {
        boolean hasText =
                mUrlCoordinator != null
                        && !TextUtils.isEmpty(mUrlCoordinator.getTextWithAutocomplete());
        return hasText && (mUrlHasFocus || mIsUrlFocusChangeInProgress);
    }

    /**
     * @return Whether the Omnibox is focused on a desktop. Lens and mic buttons should be
     *     suppressed if the Omnibox is focused and the device is in desktop mode.
     */
    @EnsuresNonNullIf("mCurrentInput")
    private boolean isUrlBarFocusedOnDesktop() {
        return mCurrentInput != null && OmniboxCapabilities.hasDesktopExperience(mContext);
    }

    @VisibleForTesting
    boolean shouldShowMicButton() {
        if (isUrlBarFocusedWithUserInput()) return false;

        if (isUrlBarFocusedOnDesktop()) return false;

        if (!mNativeInitialized
                || mVoiceRecognitionHandler == null
                || !mVoiceRecognitionHandler.isVoiceSearchEnabled()
                || !mEmbedderUiOverrides.isVoiceEntrypointAllowed()) {
            return false;
        }
        boolean isToolbarMicEnabled = mIsToolbarMicEnabledSupplier.getAsBoolean();
        if (mIsTablet && mShouldShowButtonsWhenUnfocused) {
            return !isToolbarMicEnabled && (mUrlHasFocus || mIsUrlFocusChangeInProgress);
        } else {
            boolean canShowMicButton = !mIsTablet || !isToolbarMicEnabled;
            return canShowMicButton
                    && (mUrlHasFocus
                            || mIsUrlFocusChangeInProgress
                            || mIsLocationBarFocusedFromNtpScroll
                            || mShouldShowMicButtonWhenUnfocused);
        }
    }

    @VisibleForTesting
    boolean shouldShowLensButton() {
        if (mCurrentInput != null && !mCurrentInput.isConventionalRequestType()) return false;

        if (isUrlBarFocusedWithUserInput()) return false;

        if (isUrlBarFocusedOnDesktop()) return false;

        // When this method is called on UI inflation, return false as the native is not ready.
        if (!mNativeInitialized) {
            return false;
        }

        // Never show Lens in the old search widget page context.
        // This widget must guarantee consistent feature set regardless of search engine choice or
        // other aspects that may not be met by Lens.
        if (!mEmbedderUiOverrides.isLensEntrypointAllowed()) {
            return false;
        }

        // When this method is called after native initialized, check omnibox conditions and Lens
        // eligibility.
        if (mIsTablet && mShouldShowButtonsWhenUnfocused) {
            return (mUrlHasFocus || mIsUrlFocusChangeInProgress) && isLensOnOmniboxEnabled();
        }

        return (mUrlHasFocus
                        || mIsUrlFocusChangeInProgress
                        || mIsLocationBarFocusedFromNtpScroll
                        || mShouldShowLensButtonWhenUnfocused)
                && isLensOnOmniboxEnabled();
    }

    @RequiresNonNull("mOmniboxChipManager")
    private void updateOmniboxChipVisibility() {
        boolean focused = mUrlHasFocus || mIsUrlFocusChangeInProgress;
        mOmniboxChipManager.setOmniboxFocused(focused);
    }

    private void onAutocompleteRequestTypeChanged(@AutocompleteRequestType int type) {
        boolean isSpecializedRequestType =
                mCurrentInput != null && !mCurrentInput.isConventionalRequestType();
        if (mOnSpecializedFuseboxModeActivatedCallback != null) {
            mOnSpecializedFuseboxModeActivatedCallback.onResult(
                    type != AutocompleteRequestType.SEARCH);
        }
        updateButtonVisibility();
        onSearchBoxHintTextChanged();
        mLocationBarLayout.onSpecializedFuseboxModeActivated(isSpecializedRequestType);
    }

    private boolean isLensOnOmniboxEnabled() {
        if (mIsLensOnOmniboxEnabled == null) {
            mIsLensOnOmniboxEnabled = Boolean.valueOf(isLensEnabled(LensEntryPoint.OMNIBOX));
        }

        return mIsLensOnOmniboxEnabled.booleanValue();
    }

    private boolean shouldShowPageActionButtons() {
        assert mIsTablet;
        if (!mNativeInitialized) return true;

        // There is one action, bookmark, and it should be shown if the omnibox isn't focused.
        return !(mUrlHasFocus || mIsUrlFocusChangeInProgress);
    }

    private void updateUrl() {
        setUrl(mLocationBarDataProvider.getCurrentGurl(), mLocationBarDataProvider.getUrlBarData());
    }

    private void updateOmniboxPrerender() {
        if (mOmniboxPrerender == null) return;
        // Profile may be null if switching to a tab that has not yet been initialized.
        Profile profile = mProfileSupplier.get();
        if (profile == null) return;
        mOmniboxPrerender.clear(profile);
    }

    @SuppressLint("GestureBackNavigation")
    private boolean handleKeyEvent(View view, int keyCode, KeyEvent event) {
        try (TraceEvent e = TraceEvent.scoped("LocationBarMediator.handleKeyEvent")) {
            if (keyCode == KeyEvent.KEYCODE_ESCAPE) return false;
            boolean isRtl = view.getLayoutDirection() == View.LAYOUT_DIRECTION_RTL;
            if (mAutocompleteCoordinator != null
                    && mAutocompleteCoordinator.handleKeyEvent(keyCode, event)) {
                return true;
            } else if (KeyNavigationUtil.isEnter(event) && !hasAutocompleteController()) {
                // This path is specific to Contextual Tasks where suggestions are disabled.
                // The overriding URL loading delegate in ContextualTasksFusebox will handle this
                // and send it to the ComposeboxQueryControllerBridge where the query text will be
                // extracted and sent to the AIM page.
                loadUrl(
                        new OmniboxLoadUrlParams.Builder(
                                        mUrlCoordinator.getTextWithAutocomplete(),
                                        PageTransition.TYPED)
                                .setInputStartTimestamp(event.getEventTime())
                                .build());
                return true;
            } else if ((!isRtl && KeyNavigationUtil.isGoRight(event))
                    || (isRtl && KeyNavigationUtil.isGoLeft(event))) {
                // Ensures URL bar doesn't lose focus, when RIGHT or LEFT (RTL) key is pressed while
                // the cursor is positioned at the end of the text.
                TextView tv = (TextView) view;
                return tv.getSelectionStart() == tv.getSelectionEnd()
                        && tv.getSelectionEnd() == tv.getText().length();
            } else if (keyCode == KeyEvent.KEYCODE_DEL) {
                if (mCurrentInput != null
                        && mCurrentInput.getSiteSearchData() != null
                        && TextUtils.isEmpty(mUrlCoordinator.getTextWithoutAutocomplete())) {
                    // When in zero prefix keyword mode, pressing backspace should remove current
                    // site search data and restore the keyword as user input.
                    var siteSearchData = mCurrentInput.getSiteSearchData();
                    String searchText =
                            siteSearchData.enteredViaSpace
                                    ? siteSearchData.keyword + " "
                                    : siteSearchData.keyword;
                    mCurrentInput.setUserText(searchText);
                    mCurrentInput.setSiteSearchData(null);
                    mUrlCoordinator.setUrlBarData(
                            UrlBarData.forNonUrlText(searchText),
                            UrlBar.ScrollType.NO_SCROLL,
                            UrlBarData.SELECT_END);
                    return true;
                }
            }
            return false;
        }
    }

    @Override
    public Boolean handleEscPress() {
        if (mCurrentInput == null) return false;
        if (mAutocompleteCoordinator == null) return false;

        if (mAutocompleteCoordinator.isServingSuggestions()) {
            // First ESC keypress should close the suggestions list.
            mAutocompleteCoordinator.stopAutocomplete();
        } else if (!TextUtils.equals(
                mCurrentInput.getUserText(), mCurrentInput.getInitialUserText())) {
            // Second ESC keypress should reset the input to its initial state, if it's different.
            revertChanges();
            updateButtonVisibility();
        } else {
            // Third ESC keypress should terminate input.
            endInput();
        }
        return true;
    }

    @Override
    public boolean invokeBackActionOnEscape() {
        return false;
    }

    private void updateSearchEngineStatusIconShownState() {
        // The search engine icon will be the first visible focused view when it's showing.
        boolean shouldShowSearchEngineLogo =
                mSearchEngineUtils == null || mSearchEngineUtils.shouldShowSearchEngineLogo();

        // This branch will be hit if the search engine logo should be shown.
        if (shouldShowSearchEngineLogo && mLocationBarLayout instanceof LocationBarPhone) {
            // When the search engine icon is enabled, icons are translations into the parent view's
            // padding area. Set clip padding to false to prevent them from getting clipped.
            mLocationBarLayout.setClipToPadding(false);
        }
    }

    // LocationBarData.Observer implementation.
    // Using the default empty onSecurityStateChanged.
    // Using the default empty onTitleChanged.

    @Override
    public void onIncognitoStateChanged() {
        mIsLensOnOmniboxEnabled = Boolean.valueOf(isLensEnabled(LensEntryPoint.OMNIBOX));
        updateButtonVisibility();
        updateSearchEngineStatusIconShownState();
        // Update the visuals to use correct incognito colors.
        mUrlCoordinator.setIncognitoColorsEnabled(mLocationBarDataProvider.isIncognitoBranded());
    }

    @Override
    public void onNtpStartedLoading() {
        mLocationBarLayout.onNtpStartedLoading();
    }

    @Override
    public void onPrimaryColorChanged() {
        // Compute |mBrandedColorScheme| first.
        updateBrandedColorScheme();
        updateButtonTints();
    }

    @Override
    public void onTabChanged(@Nullable Tab previousTab) {
        // Save the previous tab state.
        if (mCurrentInput != null) {
            mCurrentInput.setSelection(
                    mUrlCoordinator.getSelectionStart(), mUrlCoordinator.getSelectionEnd());
        }

        // Restore the saved tab state.
        var state = FuseboxSessionState.from(mLocationBarDataProvider);
        if (state != null && state.isSessionActive()) {
            state.getAutocompleteInput()
                    .setFocusReason(OmniboxFocusReason.LOCATION_BAR_STATE_RESTORATION);
            beginInput(state.getAutocompleteInput());
        }

        // Set zoom indicator tooltip
        if (mPageZoomIndicatorCoordinator != null) {
            mPageZoomIndicatorCoordinator.setTooltip();
        }
    }

    @Override
    public void onUrlChanged(boolean isTabChanging) {
        if (isTabChanging) {
            var currentSession = FuseboxSessionState.from(mLocationBarDataProvider);
            if (currentSession == null || !currentSession.isSessionActive()) {
                updateUrl();

                // Ensure the URL bar loses focus if the tab it was interacting with is changed from
                // underneath it.
                endInput();

                // Place the cursor in the Omnibox if applicable.  We always clear the focus above
                // to ensure the shield placed over the content is dismissed when switching tabs.
                // But if needed, we will refocus the omnibox and make the cursor visible here.
                maybeShowOrClearCursorInLocationBar();
            }
        } else {
            updateUrl();
        }

        // Set zoom indicator tooltip
        if (mPageZoomIndicatorCoordinator != null) {
            mPageZoomIndicatorCoordinator.setTooltip();
        }

        updateOmniboxPrerender();
        updateButtonVisibility();
    }

    @Override
    public void onPageLoadStopped() {
        // Update back button visibility and enabled state when page stops loading.
        // When navigating from NTP, onUrlChanged might be called before tab.canGoBack()
        // returns true. Updating here ensures the button state is corrected when the page
        // finishes loading.
        updateBackButtonVisibility();
    }

    @Override
    public void hintZeroSuggestRefresh() {
        if (mAutocompleteCoordinator == null) return;
        mAutocompleteCoordinator.prefetchZeroSuggestResults(mLocationBarDataProvider.getTab());
    }

    // TemplateUrlService.TemplateUrlServiceObserver implementation
    @Override
    public void onTemplateURLServiceChanged() {
        mIsLensOnOmniboxEnabled = Boolean.valueOf(isLensEnabled(LensEntryPoint.OMNIBOX));
    }

    // OmniboxStub implementation.
    /**
     * Begins an Omnibox input session with the given input. This will typically focus the Omnibox
     * and initialize autocomplete.
     *
     * @param input The AutocompleteInput object with details for the focus operation.
     */
    @Override
    public void beginInput(AutocompleteInput input) {
        input.setPageClassification(mLocationBarDataProvider.getPageClassification(false));
        input.setPageUrl(mLocationBarDataProvider.getCurrentGurl());
        input.setPageTitle(mLocationBarDataProvider.getTitle());

        var state = FuseboxSessionState.from(mLocationBarDataProvider);
        // Conditions to show omnibox and suggestions are not met - avoid showing detached
        // suggest and bail.
        if (state == null) return;

        state.applyAutocompleteInput(input);

        if (!mUrlHasFocus) {
            recordOmniboxFocusReason(input.getFocusReason());
            // Record Lens button shown when Omnibox is focused.
            if (shouldShowLensButton()) LensMetrics.recordOmniboxFocusedWhenLensShown();
        }

        if (mUrlHasFocus && mUrlFocusedWithoutAnimations && !mIsReparenting) {
            handleUrlFocusAnimation(true);
        } else if (input.getAutocompleteState() != AutocompleteState.STANDBY_NO_FOCUS) {
            requestUrlFocus();
        }

        // Wait for the Url focus change before refreshing autocomplete.
        beginOrResumeInput(/* activateNewSession= */ true);
    }

    /**
     * Ends the current Omnibox input session. This will typically clear the focus from the Omnibox.
     */
    @Override
    public void endInput() {
        endInputInternal();
        if (mUrlHasFocus) {
            mUrlCoordinator.clearFocus();
        }
    }

    @Override
    public @Nullable VoiceRecognitionHandler getVoiceRecognitionHandler() {
        return mVoiceRecognitionHandler;
    }

    @Override
    public void addUrlFocusChangeListener(UrlFocusChangeListener listener) {
        mUrlFocusChangeListeners.addObserver(listener);
    }

    @Override
    public void removeUrlFocusChangeListener(UrlFocusChangeListener listener) {
        mUrlFocusChangeListeners.removeObserver(listener);
    }

    @Override
    public boolean isUrlBarFocused() {
        return mUrlHasFocus;
    }

    private boolean hasAutocompleteController() {
        return mAutocompleteCoordinator != null
                && mAutocompleteCoordinator.hasAutocompleteController();
    }

    /** {@see OmniboxStub#loadUrlFromVoice(GURL)} */
    @Override
    public void loadUrlFromVoice(GURL url) {
        loadUrl(
                new OmniboxLoadUrlParams.Builder(url.getSpec(), PageTransition.TYPED)
                        .setOpenInNewTab(false)
                        .build());
    }

    @Override
    public void onVoiceAvailabilityImpacted() {
        updateButtonVisibility();
    }

    boolean isUrlBarFocusedWithoutAnimation() {
        return mUrlFocusedWithoutAnimations;
    }

    /** Getter for LocationBarDataProvider. */
    public LocationBarDataProvider getLocationBarDataProvider() {
        return mLocationBarDataProvider;
    }

    // UrlBarDelegate implementation.

    @Override
    public @Nullable View getViewForUrlBackFocus() {
        assert mLocationBarDataProvider != null;
        Tab tab = mLocationBarDataProvider.getTab();
        return tab == null ? null : tab.getView();
    }

    @Override
    public boolean allowKeyboardLearning() {
        return !mLocationBarDataProvider.isOffTheRecord();
    }

    @Override
    public @Nullable String getReplacementCutCopyText(
            String currentText, int selectionStart, int selectionEnd) {
        GURL currentGurl = mLocationBarDataProvider.getCurrentGurl();
        if (!ContextualTasksUtils.isContextualTasksUrl(currentGurl)) return null;

        WebContents webContents = mLocationBarDataProvider.getWebContents();
        if (webContents == null) return null;

        GURL functionalGurl = ContextualTasksUtils.getContextualTasksFunctionalURL(webContents);
        if (GURL.isEmptyOrInvalid(functionalGurl)) {
            return null;
        }

        return ContextualTasksUtils.getReplacementUrl(
                currentText, selectionStart, selectionEnd, functionalGurl);
    }

    @Override
    public boolean isKeyboardSuppressed() {
        // Suppress the keyboard while the fusebox popup is showing as a bottom sheet.
        return mFuseboxCoordinator.getPopupStateSupplier().get() == PopupState.BOTTOM;
    }

    // Traditional way to intercept keycode_back, which is deprecated from T.
    @Override
    public void backKeyPressed() {
        if (mBackKeyBehavior.handleBackKeyPressed()) {
            return;
        }

        // Revert the URL to match the current page.
        setUrl(mLocationBarDataProvider.getCurrentGurl(), mLocationBarDataProvider.getUrlBarData());
        endInputAndFocusCurrentTab();
    }

    @Override
    public void shareText(String text) {
        ShareParams params = new ShareParams.Builder(mWindowAndroid, /* title= */ "", text).build();
        ShareHelper.shareWithSystemShareSheetUi(params);
    }

    @Override
    public void onFocusByTouch() {
        recordOmniboxFocusReason(OmniboxFocusReason.OMNIBOX_TAP);
    }

    @Override
    public void onEditorAction(int actionCode) {
        // For contextual tasks, autocomplete is disabled and unavailable to handle keyboard
        // actions, so we have to handle them here.
        if (hasAutocompleteController()) return;

        if (actionCode == EditorInfo.IME_ACTION_GO
                || actionCode == EditorInfo.IME_ACTION_SEARCH
                || actionCode == EditorInfo.IME_ACTION_SEND
                || actionCode == EditorInfo.IME_ACTION_DONE) {
            loadUrl(
                    new OmniboxLoadUrlParams.Builder(
                                    mUrlCoordinator.getTextWithAutocomplete(), PageTransition.TYPED)
                            .build());
        }
    }

    @Override
    public void onTouchAfterFocus() {
        // The goal of this special logic is to support the following use case:
        // On opening the NTP, the URL bar gains focus with a blinking cursor and without showing
        // the zero-prefix dropdown when a hardware keyboard is connected. Subsequently, if the user
        // taps on the omnibox without typing any text into it, the zero-prefix dropdown will be
        // shown.
        //
        // A touch event will be handled after the omnibox is already focused only when the
        // following criteria are satisfied:
        // 1. |mUrlFocusedWithoutAnimations| is true, which means that the omnibox is focused on the
        // NTP without any focus animations while a hardware keyboard is connected.
        // 2. The omnibox does not contain any text. It is possible that the user has typed text
        // into the omnibox after it gains focus due to hardware keyboard availability and a
        // subsequent tap will hide the suggestions dropdown shown for the typed text, while keeping
        // the scrim on the web contents, which is not desirable.
        if (mCurrentInput == null
                || mCurrentInput.getAutocompleteState() != AutocompleteState.STANDBY) {
            return;
        }

        recordOmniboxFocusReason(OmniboxFocusReason.TAP_AFTER_FOCUS_FROM_KEYBOARD);
        mCurrentInput
                .setFocusReason(OmniboxFocusReason.TAP_AFTER_FOCUS_FROM_KEYBOARD)
                .setAutocompleteState(AutocompleteState.ENABLED);
        beginOrResumeInput(/* activateNewSession= */ false);
    }

    // BackPressHandler implementation.
    // Modern way to intercept back press starting from T.
    @Override
    public @BackPressResult int handleBackPress() {
        int res = mUrlHasFocus ? BackPressResult.SUCCESS : BackPressResult.FAILURE;
        backKeyPressed();
        return res;
    }

    @Override
    public NonNullObservableSupplier<Boolean> getHandleBackPressChangedSupplier() {
        return mBackPressStateSupplier;
    }

    // OnKeyListener implementation.
    @Override
    public boolean onKey(View view, int keyCode, KeyEvent event) {
        return handleKeyEvent(view, keyCode, event);
    }

    // ComponentCallbacks implementation.

    @Override
    public void onConfigurationChanged(Configuration newConfig) {
        // If the user previously entered the STANDBY autocomplete state (no autocompletion), e.g.
        // by opening a NTP or by pressing physical ESC key (~desktop mode behavior) but upon
        // configuration change we detect we're no longer in desktop mode (e.g. keyboard has been
        // disconnected) then we can confidently state the user has typed no text and we can clear
        // the focus.
        if (mCurrentInput != null
                && mCurrentInput.getAutocompleteState() == AutocompleteState.STANDBY
                && !isUrlBarFocusedOnDesktop()) {
            endInput();
        }
    }

    @Override
    public void onLowMemory() {}

    @Override
    public boolean isLensEnabled(@LensEntryPoint int lensEntryPoint) {
        return mLensController.isLensEnabled(
                new LensQueryParams.Builder(
                                lensEntryPoint, mLocationBarDataProvider.isIncognito(), mIsTablet)
                        .build());
    }

    @Override
    public void startLens(@LensEntryPoint int lensEntryPoint) {
        // TODO(b/181067692): Report user action for this click.
        mLensController.startLens(
                mWindowAndroid,
                new LensIntentParams.Builder(lensEntryPoint, mLocationBarDataProvider.isIncognito())
                        .build());
    }

    // PauseResumeWithNativeObserver impl.
    @Override
    public void onResumeWithNative() {
        Profile profile = mProfileSupplier.get();
        TemplateUrlService templateUrlService = mTemplateUrlServiceSupplier.get();
        if (OmniboxFeatures.sUseFusedLocationProvider.isEnabled()
                && profile != null
                && templateUrlService != null) {
            GeolocationHeader.primeLocationForGeoHeaderIfEnabled(profile, templateUrlService);
        }
    }

    @Override
    public void onPauseWithNative() {
        OmniboxFeatures.updateLastExitTimestamp();
        if (OmniboxFeatures.sUseFusedLocationProvider.isEnabled()) {
            GeolocationHeader.stopListeningForLocationUpdates();
        }
    }

    /* package */ void setLocationBarButtonTranslationForNtpAnimation(float translationX) {
        mLocationBarLayout.setLocationBarButtonTranslationForNtpAnimation(translationX);
    }

    /** Updates the tints of UI buttons. */
    private void updateButtonTints() {
        ColorStateList tint = ThemeUtils.getThemedToolbarIconTint(mContext, mBrandedColorScheme);
        mLocationBarLayout.setMicButtonTint(tint);
        mLocationBarLayout.setLensButtonTint(tint);
        mLocationBarLayout.setInstallButtonTint(tint);
        mLocationBarLayout.setZoomButtonTint(tint);
        mLocationBarLayout.setBackButtonTint(tint);
    }

    /**
     * Updates the color of the hint text in the search box.
     *
     * @param useDefaultUrlBarHintTextColor Whether to use the default color for the search text in
     *     the search box. If not we will use specific color for NTP's un-focus state.
     */
    public void updateUrlBarHintTextColor(boolean useDefaultUrlBarHintTextColor) {
        if (useDefaultUrlBarHintTextColor) {
            mUrlCoordinator.setUrlBarHintTextColorForDefault(mBrandedColorScheme);
        } else {
            mUrlCoordinator.setUrlBarHintTextColorForNtp();
        }
    }

    /**
     * Updates the value for the end margin of the url action container in the search box.
     *
     * @param useDefaultUrlActionContainerEndMargin Whether to use the default end margin for the
     *     url action container in the search box. If not we will use the specific end margin value
     *     for NTP's un-focus state.
     */
    public void updateUrlActionContainerEndMargin(boolean useDefaultUrlActionContainerEndMargin) {
        mLocationBarLayout.updateUrlActionContainerEndMargin(useDefaultUrlActionContainerEndMargin);
    }

    /**
     * Updates the location bar button background.
     *
     * @param backgroundResId The button background resource.
     */
    public void updateButtonBackground(@DrawableRes int backgroundResId) {
        mLocationBarLayout.setDeleteButtonBackground(backgroundResId);
    }

    public void maybeShowDefaultBrowserPromo() {
        DefaultBrowserPromoUtils.getInstance()
                .maybeShowDefaultBrowserPromoMessages(
                        mContext, mWindowAndroid, assertNonNull(mProfileSupplier.get()));
    }

    @Override
    public void onSearchBoxHintTextChanged() {
        // Edge case / SearchActivity could be triggering focus before Profile (and by proxy -
        // SearchEngineUtils) is available.
        if (mSearchEngineUtils == null) return;
        if (mEmbedderUiOverrides.isEmbedderControlledHint()) return;

        if (mCurrentInput != null && mCurrentInput.getSiteSearchData() != null) {
            mUrlCoordinator.setUrlBarHintText("");
            return;
        }

        @AutocompleteRequestType
        int requestType =
                mCurrentInput == null
                        ? mLocationBarDataProvider.getDefaultRequestType()
                        : mCurrentInput.getRequestType();

        FuseboxSessionState fuseboxSessionState = null;
        if (OmniboxFeatures.sShowModelPicker.getValue()) {
            fuseboxSessionState = FuseboxSessionState.from(mLocationBarDataProvider);
        }

        mUrlCoordinator.setUrlBarHintText(
                mSearchEngineUtils.getOmniboxHintText(requestType, fuseboxSessionState));
    }

    /* package */ ToolbarWidthConsumer getBookmarkButtonToolbarWidthConsumer() {
        return mBookmarkButtonToolbarWidthConsumer;
    }

    /* package */ ToolbarWidthConsumer getInstallButtonToolbarWidthConsumer() {
        return mInstallButtonToolbarWidthConsumer;
    }

    /* package */ ToolbarWidthConsumer getMicButtonToolbarWidthConsumer() {
        return mMicButtonToolbarWidthConsumer;
    }

    /* package */ ToolbarWidthConsumer getLensButtonToolbarWidthConsumer() {
        return mLensButtonToolbarWidthConsumer;
    }

    /* package */ ToolbarWidthConsumer getZoomButtonToolbarWidthConsumer() {
        return mZoomButtonToolbarWidthConsumer;
    }

    /* package */ @Nullable ToolbarWidthConsumer getOmniboxChipCollapsedToolbarWidthConsumer() {
        return mOmniboxChipManager != null
                ? mOmniboxChipManager.getCollapsedToolbarWidthConsumer()
                : null;
    }

    /* package */ @Nullable ToolbarWidthConsumer getOmniboxChipExpandedToolbarWidthConsumer() {
        return mOmniboxChipManager != null
                ? mOmniboxChipManager.getExpandedToolbarWidthConsumer()
                : null;
    }

    private static class ButtonToolbarWidthConsumer implements ToolbarWidthConsumer {
        private final int mButtonWidth;
        private final Supplier<Boolean> mShouldShowButton;
        private final Callback<Boolean> mUpdateButtonVisibility;
        private final boolean mIsTablet;
        private boolean mHasSpaceToShow;

        ButtonToolbarWidthConsumer(
                Context context,
                boolean isTablet,
                Supplier<Boolean> shouldShowButton,
                Callback<Boolean> updateButtonVisibility) {
            mShouldShowButton = shouldShowButton;
            mIsTablet = isTablet;
            mUpdateButtonVisibility = updateButtonVisibility;
            mButtonWidth =
                    context.getResources()
                            .getDimensionPixelSize(R.dimen.location_bar_action_icon_width);
        }

        boolean hasSpaceToShow() {
            if (!mIsTablet || !ChromeFeatureList.sToolbarTabletResizeRefactor.isEnabled()) {
                return true;
            }
            return mHasSpaceToShow;
        }

        @Override
        public boolean isVisible() {
            return mHasSpaceToShow && mShouldShowButton.get();
        }

        @Override
        public int updateVisibility(int availableWidth) {
            assert ChromeFeatureList.sToolbarTabletResizeRefactor.isEnabled();

            if (mShouldShowButton.get() && availableWidth >= mButtonWidth) {
                mHasSpaceToShow = true;
                mUpdateButtonVisibility.onResult(true);
                return mButtonWidth;
            }
            mHasSpaceToShow = false;
            mUpdateButtonVisibility.onResult(false);
            return 0;
        }

        @Override
        public int updateVisibilityWithAnimation(
                int availableWidth, Collection<Animator> animators) {
            return updateVisibility(availableWidth);
        }
    }

    /**
     * Set the visibility of the URL action buttons as a whole.
     *
     * <p>Visibility of each button is guarded by two states: visibility of a specific button and
     * visibility of the entire group, ensuring that only requested buttons are shown/hidden when
     * the value passed to this method is toggled.
     */
    void setUrlActionContainerVisibility(boolean shouldShow) {
        mLocationBarLayout.setUrlActionContainerVisibility(shouldShow);
    }

    private void setAttachmentModelList(
            @Nullable FuseboxAttachmentModelList fuseboxAttachmentModelList) {
        if (mFuseboxAttachmentModelList != null) {
            mFuseboxAttachmentModelList.removeAttachmentChangeListener(this);
        }
        mFuseboxAttachmentModelList = fuseboxAttachmentModelList;
        if (mFuseboxAttachmentModelList != null) {
            mFuseboxAttachmentModelList.addAttachmentChangeListener(this);
        }
    }
}
