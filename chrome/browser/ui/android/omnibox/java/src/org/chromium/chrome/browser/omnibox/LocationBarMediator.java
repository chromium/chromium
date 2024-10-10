// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.omnibox;

import android.animation.Animator;
import android.animation.AnimatorListenerAdapter;
import android.animation.ObjectAnimator;
import android.annotation.SuppressLint;
import android.content.ComponentCallbacks;
import android.content.Context;
import android.content.res.ColorStateList;
import android.content.res.Configuration;
import android.graphics.Rect;
import android.text.TextUtils;
import android.util.FloatProperty;
import android.view.KeyEvent;
import android.view.View;
import android.view.View.MeasureSpec;
import android.view.View.OnKeyListener;
import android.widget.TextView;

import androidx.annotation.NonNull;
import androidx.annotation.Nullable;
import androidx.annotation.VisibleForTesting;
import androidx.appcompat.content.res.AppCompatResources;

import org.chromium.base.CallbackController;
import org.chromium.base.CommandLine;
import org.chromium.base.ObserverList;
import org.chromium.base.metrics.RecordHistogram;
import org.chromium.base.metrics.RecordUserAction;
import org.chromium.base.metrics.TimingMetric;
import org.chromium.base.supplier.ObservableSupplier;
import org.chromium.base.supplier.ObservableSupplierImpl;
import org.chromium.base.supplier.OneshotSupplier;
import org.chromium.base.task.PostTask;
import org.chromium.base.task.TaskTraits;
import org.chromium.chrome.browser.device.DeviceClassManager;
import org.chromium.chrome.browser.flags.ChromeSwitches;
import org.chromium.chrome.browser.lens.LensController;
import org.chromium.chrome.browser.lens.LensEntryPoint;
import org.chromium.chrome.browser.lens.LensIntentParams;
import org.chromium.chrome.browser.lens.LensMetrics;
import org.chromium.chrome.browser.lens.LensQueryParams;
import org.chromium.chrome.browser.lifecycle.PauseResumeWithNativeObserver;
import org.chromium.chrome.browser.locale.LocaleManager;
import org.chromium.chrome.browser.omnibox.UrlBar.UrlBarDelegate;
import org.chromium.chrome.browser.omnibox.UrlBarCoordinator.SelectionState;
import org.chromium.chrome.browser.omnibox.geo.GeolocationHeader;
import org.chromium.chrome.browser.omnibox.status.StatusCoordinator;
import org.chromium.chrome.browser.omnibox.styles.OmniboxResourceProvider;
import org.chromium.chrome.browser.omnibox.suggestions.AutocompleteCoordinator;
import org.chromium.chrome.browser.omnibox.suggestions.OmniboxLoadUrlParams;
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
import org.chromium.chrome.browser.ui.default_browser_promo.DefaultBrowserPromoUtils;
import org.chromium.chrome.browser.ui.native_page.NativePage;
import org.chromium.chrome.browser.ui.theme.BrandedColorScheme;
import org.chromium.chrome.browser.util.ChromeAccessibilityUtil;
import org.chromium.chrome.browser.util.KeyNavigationUtil;
import org.chromium.components.browser_ui.styles.ChromeColors;
import org.chromium.components.browser_ui.widget.animation.CancelAwareAnimatorListener;
import org.chromium.components.browser_ui.widget.gesture.BackPressHandler;
import org.chromium.components.embedder_support.util.UrlUtilities;
import org.chromium.components.omnibox.AutocompleteMatch;
import org.chromium.components.omnibox.OmniboxFeatures;
import org.chromium.components.search_engines.TemplateUrlService;
import org.chromium.content_public.browser.LoadUrlParams;
import org.chromium.content_public.common.ResourceRequestBody;
import org.chromium.ui.KeyboardUtils;
import org.chromium.ui.base.PageTransition;
import org.chromium.ui.base.WindowAndroid;
import org.chromium.ui.interpolators.Interpolators;
import org.chromium.url.GURL;

import java.util.ArrayList;
import java.util.List;
import java.util.function.BooleanSupplier;

/**
 * Mediator for the LocationBar component. Intended location for LocationBar business logic;
 * currently, migration of this logic out of LocationBarLayout is in progress.
 */
class LocationBarMediator
        implements LocationBarDataProvider.Observer,
                OmniboxStub,
                VoiceRecognitionHandler.Delegate,
                VoiceRecognitionHandler.Observer,
                UrlBarDelegate,
                OnKeyListener,
                ComponentCallbacks,
                TemplateUrlService.TemplateUrlServiceObserver,
                BackPressHandler,
                PauseResumeWithNativeObserver {
    private static final int ICON_FADE_ANIMATION_DURATION_MS = 150;
    private static final int ICON_FADE_ANIMATION_DELAY_MS = 75;
    private static final long NTP_KEYBOARD_FOCUS_DURATION_MS = 200;
    private static final int WIDTH_CHANGE_ANIMATION_DURATION_MS = 225;
    private static final int WIDTH_CHANGE_ANIMATION_DELAY_MS = 75;
    private static Boolean sLastCachedIsLensOnOmniboxEnabled;

    /** Enabled/disabled state of 'save offline' button. */
    public interface SaveOfflineButtonState {
        /**
         * @param tab Tab displaying the page that will be saved.
         * @return {@code true} if the UI button is enabled.
         */
        boolean isEnabled(Tab tab);
    }

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
            new FloatProperty<LocationBarMediator>("") {
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
            new FloatProperty<LocationBarMediator>("") {
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
    private final LocationBarEmbedderUiOverrides mEmbedderUiOverrides;
    private StatusCoordinator mStatusCoordinator;
    private AutocompleteCoordinator mAutocompleteCoordinator;
    private OmniboxPrerender mOmniboxPrerender;
    private UrlBarCoordinator mUrlCoordinator;
    private ObservableSupplier<Profile> mProfileSupplier;
    private CallbackController mCallbackController = new CallbackController();
    private final OverrideUrlLoadingDelegate mOverrideUrlLoadingDelegate;
    private final LocaleManager mLocaleManager;
    private final List<Runnable> mDeferredNativeRunnables = new ArrayList<>();
    private final OneshotSupplier<TemplateUrlService> mTemplateUrlServiceSupplier;
    private final Context mContext;
    private final BackKeyBehaviorDelegate mBackKeyBehavior;
    private final WindowAndroid mWindowAndroid;
    private GURL mOriginalUrl = GURL.emptyGURL();
    private Animator mUrlFocusChangeAnimator;
    private final ObserverList<UrlFocusChangeListener> mUrlFocusChangeListeners =
            new ObserverList<>();
    private final Rect mRootViewBounds = new Rect();
    private final SaveOfflineButtonState mSaveOfflineButtonState;
    private final OmniboxUma mOmniboxUma;
    private final OmniboxSuggestionsDropdownEmbedderImpl mEmbedderImpl;

    private boolean mNativeInitialized;
    private boolean mUrlFocusedFromFakebox;
    private boolean mUrlFocusedWithoutAnimations;
    private boolean mUrlFocusedWithPastedText;
    private boolean mIsUrlFocusChangeInProgress;
    private final boolean mIsTablet;
    private boolean mShouldShowLensButtonWhenUnfocused;
    private boolean mShouldShowMicButtonWhenUnfocused;
    // Whether the microphone and bookmark buttons should be shown in the tablet location bar. These
    // buttons are hidden if the window size is < 600dp.
    private boolean mShouldShowButtonsWhenUnfocused;
    private float mUrlFocusChangeFraction;
    private boolean mUrlHasFocus;
    private LensController mLensController;
    private final BooleanSupplier mIsToolbarMicEnabledSupplier;
    // Tracks if the location bar is laid out in a focused state due to an ntp scroll.
    private boolean mIsLocationBarFocusedFromNtpScroll;
    private @BrandedColorScheme int mBrandedColorScheme = BrandedColorScheme.APP_DEFAULT;
    private ObservableSupplierImpl<Boolean> mBackPressStateSupplier =
            new ObservableSupplierImpl<>();
    private ObservableSupplier<TabModelSelector> mTabModelSelectorSupplier;
    private SearchEngineUtils mSearchEngineUtils;

    /*package */ LocationBarMediator(
            @NonNull Context context,
            @NonNull LocationBarLayout locationBarLayout,
            @NonNull LocationBarDataProvider locationBarDataProvider,
            @NonNull LocationBarEmbedderUiOverrides embedderUiOverrides,
            @NonNull ObservableSupplier<Profile> profileSupplier,
            @NonNull OverrideUrlLoadingDelegate overrideUrlLoadingDelegate,
            @NonNull LocaleManager localeManager,
            @NonNull OneshotSupplier<TemplateUrlService> templateUrlServiceSupplier,
            @NonNull BackKeyBehaviorDelegate backKeyBehavior,
            @NonNull WindowAndroid windowAndroid,
            boolean isTablet,
            @NonNull LensController lensController,
            @NonNull SaveOfflineButtonState saveOfflineButtonState,
            @NonNull OmniboxUma omniboxUma,
            @NonNull BooleanSupplier isToolbarMicEnabledSupplier,
            @NonNull OmniboxSuggestionsDropdownEmbedderImpl dropdownEmbedder,
            @Nullable ObservableSupplier<TabModelSelector> tabModelSelectorSupplier) {
        mContext = context;
        mLocationBarLayout = locationBarLayout;
        mLocationBarDataProvider = locationBarDataProvider;
        mLocationBarDataProvider.addObserver(this);
        mEmbedderUiOverrides = embedderUiOverrides;
        mOverrideUrlLoadingDelegate = overrideUrlLoadingDelegate;
        mLocaleManager = localeManager;
        mVoiceRecognitionHandler = new VoiceRecognitionHandler(this, profileSupplier);
        mVoiceRecognitionHandler.addObserver(this);
        mProfileSupplier = profileSupplier;
        mProfileSupplier.addObserver(mCallbackController.makeCancelable(this::setProfile));
        mTemplateUrlServiceSupplier = templateUrlServiceSupplier;
        mBackKeyBehavior = backKeyBehavior;
        mWindowAndroid = windowAndroid;
        mIsTablet = isTablet;
        mShouldShowButtonsWhenUnfocused = isTablet;
        mLensController = lensController;
        mSaveOfflineButtonState = saveOfflineButtonState;
        mOmniboxUma = omniboxUma;
        mIsToolbarMicEnabledSupplier = isToolbarMicEnabledSupplier;
        mEmbedderImpl = dropdownEmbedder;
        mTabModelSelectorSupplier = tabModelSelectorSupplier;
    }

    /**
     * Sets coordinators post-construction; they can't be set at construction time since
     * LocationBarMediator is a delegate for them, so is constructed beforehand.
     *
     * @param urlCoordinator Coordinator for the url bar.
     * @param autocompleteCoordinator Coordinator for the autocomplete component.
     * @param statusCoordinator Coordinator for the status icon.
     */
    /*package */ void setCoordinators(
            UrlBarCoordinator urlCoordinator,
            AutocompleteCoordinator autocompleteCoordinator,
            StatusCoordinator statusCoordinator) {
        mUrlCoordinator = urlCoordinator;
        mAutocompleteCoordinator = autocompleteCoordinator;
        mStatusCoordinator = statusCoordinator;
        updateShouldAnimateIconChanges();
        updateButtonVisibility();
        updateSearchEngineStatusIconShownState();
    }

    /* package */ void destroy() {
        if (mTemplateUrlServiceSupplier.hasValue()) {
            mTemplateUrlServiceSupplier.get().removeObserver(this);
        }
        mStatusCoordinator = null;
        mAutocompleteCoordinator = null;
        mUrlCoordinator = null;
        mVoiceRecognitionHandler.removeObserver(this);
        mVoiceRecognitionHandler.destroy();
        mVoiceRecognitionHandler = null;
        mLocationBarDataProvider.removeObserver(this);
        mDeferredNativeRunnables.clear();
        mUrlFocusChangeListeners.clear();
    }

    /*package */ void onUrlFocusChange(boolean hasFocus) {
        setUrlFocusChangeInProgress(true);
        mUrlHasFocus = hasFocus;
        // Intercept back press if it has focus.
        mBackPressStateSupplier.set(mUrlHasFocus);
        updateButtonVisibility();
        updateShouldAnimateIconChanges();
        onPrimaryColorChanged();

        if (hasFocus) {
            if (mNativeInitialized) RecordUserAction.record("FocusLocation");
            boolean shouldRetainOmniboxOnFocus = OmniboxFeatures.shouldRetainOmniboxOnFocus();
            if (!mUrlFocusedWithPastedText && !shouldRetainOmniboxOnFocus) {
                setUrlBarText(
                        UrlBarData.EMPTY, UrlBar.ScrollType.NO_SCROLL, SelectionState.SELECT_END);
            } else if (shouldRetainOmniboxOnFocus) {
                mUrlCoordinator.setSelectAllOnFocus(true);
            }
        } else {
            mUrlFocusedFromFakebox = false;
            mUrlFocusedWithoutAnimations = false;
        }

        mStatusCoordinator.onUrlFocusChange(hasFocus);

        if (!mUrlFocusedWithoutAnimations) handleUrlFocusAnimation(hasFocus);

        if (hasFocus
                && mLocationBarDataProvider.hasTab()
                && !mLocationBarDataProvider.isIncognito()) {
            if (mTemplateUrlServiceSupplier.hasValue()) {
                if (mTemplateUrlServiceSupplier.get().isDefaultSearchEngineGoogle()) {
                    GeolocationHeader.primeLocationForGeoHeaderIfEnabled(
                            mProfileSupplier.get(), mTemplateUrlServiceSupplier.get());
                }
            } else {
                mTemplateUrlServiceSupplier.onAvailable(
                        (templateUrlService) -> {
                            if (templateUrlService.isDefaultSearchEngineGoogle()) {
                                GeolocationHeader.primeLocationForGeoHeaderIfEnabled(
                                        mProfileSupplier.get(), templateUrlService);
                            }
                        });
            }
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
                (templateUrlService) -> {
                    templateUrlService.addObserver(this);
                    if (OmniboxFeatures.sUseFusedLocationProvider.isEnabled()
                            && templateUrlService.isDefaultSearchEngineGoogle()) {
                        GeolocationHeader.primeLocationForGeoHeaderIfEnabled(
                                mProfileSupplier.get(), mTemplateUrlServiceSupplier.get());
                    }
                });

        mLocationBarLayout.onFinishNativeInitialization();
        if (mProfileSupplier.hasValue()) setProfile(mProfileSupplier.get());

        mLocationBarLayout.setMicButtonDrawable(
                AppCompatResources.getDrawable(mContext, R.drawable.ic_mic_white_24dp));
        onPrimaryColorChanged();

        for (Runnable deferredRunnable : mDeferredNativeRunnables) {
            mLocationBarLayout.post(deferredRunnable);
        }
        mDeferredNativeRunnables.clear();
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

            if (fraction > 0f) {
                mLocationBarLayout.setUrlActionContainerVisibility(View.VISIBLE);
            } else if (fraction == 0f && !mIsUrlFocusChangeInProgress) {
                // If a URL focus change is in progress, then it will handle setting the visibility
                // correctly after it completes.  If done here, it would cause the URL to jump due
                // to a badly timed layout call.
                mLocationBarLayout.setUrlActionContainerVisibility(View.GONE);
            }

            // Add expansion animation for the space besides status view in location bar.
            mLocationBarLayout.setUrlFocusChangePercent(
                    ntpSearchBoxScrollFraction,
                    urlFocusChangeFraction,
                    mIsUrlFocusChangeInProgress);
            mStatusCoordinator.setUrlFocusChangePercent(fraction);
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
        sLastCachedIsLensOnOmniboxEnabled = null;
    }

    /* package */ void setIsUrlBarFocusedWithoutAnimationsForTesting(
            boolean isUrlBarFocusedWithoutAnimations) {
        mUrlFocusedWithoutAnimations = isUrlBarFocusedWithoutAnimations;
    }

    /*package */ void updateVisualsForState() {
        onPrimaryColorChanged();
    }

    /*package */ void setShowTitle(boolean showTitle) {
        // This method is only used in CustomTabToolbar.
    }

    /*package */ void showUrlBarCursorWithoutFocusAnimations() {
        if (mUrlHasFocus || mUrlFocusedFromFakebox) {
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
        setUrlBarFocus(
                /* shouldBeFocused= */ true,
                /* pastedText= */ null,
                OmniboxFocusReason.DEFAULT_WITH_HARDWARE_KEYBOARD);
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
            setUrlBarFocus(false, null, OmniboxFocusReason.UNFOCUS);
        }
    }

    /*package */ void revertChanges() {
        if (mUrlHasFocus) {
            GURL currentUrl = mLocationBarDataProvider.getCurrentGurl();
            if (NativePage.isChromePageUrl(currentUrl, mLocationBarDataProvider.isOffTheRecord())) {
                setUrlBarTextEmpty();
            } else {
                setUrlBarText(
                        mLocationBarDataProvider.getUrlBarData(),
                        UrlBar.ScrollType.NO_SCROLL,
                        SelectionState.SELECT_ALL);
            }
            mUrlCoordinator.setKeyboardVisibility(false, false);
        } else {
            setUrl(
                    mLocationBarDataProvider.getCurrentGurl(),
                    mLocationBarDataProvider.getUrlBarData());
        }
    }

    /* package */ void onUrlTextChanged() {
        updateButtonVisibility();
    }

    /* package */ void onSuggestionsChanged(@Nullable AutocompleteMatch defaultMatch) {
        // TODO (https://crbug.com/1152501): Refactor the LBM/LBC relationship such that LBM doesn't
        // need to communicate with other coordinators like this.
        String userText = mUrlCoordinator.getTextWithoutAutocomplete();
        mStatusCoordinator.onDefaultMatchClassified(
                // Zero suggest is always considered Search.
                TextUtils.isEmpty(userText)
                        ||
                        // Otherwise, use the default match type (if possible), or assume Search (if
                        // not).
                        (defaultMatch != null ? defaultMatch.isSearchSuggestion() : true));
        if (mUrlCoordinator.shouldAutocomplete()) {
            mUrlCoordinator.setAutocompleteText(
                    userText,
                    defaultMatch != null ? defaultMatch.getInlineAutocompletion() : null,
                    defaultMatch != null ? defaultMatch.getAdditionalText() : null);
        }

        // Handle the case where suggestions (in particular zero suggest) are received without the
        // URL focusing happening.
        if (mUrlFocusedWithoutAnimations && mUrlHasFocus) {
            handleUrlFocusAnimation(/* hasFocus= */ true);
        }

        if (mNativeInitialized
                && mProfileSupplier.hasValue()
                && !CommandLine.getInstance().hasSwitch(ChromeSwitches.DISABLE_INSTANT)
                && DeviceClassManager.enablePrerendering()
                && PreloadPagesSettingsBridge.getState(mProfileSupplier.get())
                        != PreloadPagesState.NO_PRELOADING
                && mLocationBarDataProvider.hasTab()) {
            mOmniboxPrerender.prerenderMaybe(
                    userText,
                    mOriginalUrl.getSpec(),
                    mAutocompleteCoordinator.getCurrentNativeAutocompleteResult(),
                    mProfileSupplier.get(),
                    mLocationBarDataProvider.getTab());
        }

        mUrlCoordinator.onUrlBarSuggestionsChanged(
                mAutocompleteCoordinator.getSuggestionCount() != 0);
    }

    /* package */ void loadUrl(OmniboxLoadUrlParams omniboxLoadUrlParams) {
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
        if (currentTab != null) {
            boolean isCurrentTabNtpUrl = UrlUtilities.isNtpUrl(currentTab.getUrl());
            if (currentTab.isNativePage() || isCurrentTabNtpUrl) {
                mOmniboxUma.recordNavigationOnNtp(
                        omniboxLoadUrlParams.url,
                        omniboxLoadUrlParams.transitionType,
                        !currentTab.isIncognito() && isCurrentTabNtpUrl);
                // Passing in an empty string should not do anything unless the user is at the NTP.
                // Since the NTP has no url, pressing enter while clicking on the URL bar should
                // refresh the page as it does when you click and press enter on any other site.
                if (url.isEmpty()) url = currentTab.getUrl().getSpec();
            }

            if (omniboxLoadUrlParams.callback != null) {
                currentTab.addObserver(
                        new EmptyTabObserver() {
                            @Override
                            public void onLoadUrl(
                                    Tab tab, LoadUrlParams params, LoadUrlResult loadUrlResult) {
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
                loadUrlParams.setVerbatimHeaders(GeolocationHeader.getGeoHeader(url, currentTab));
            }
            loadUrlParams.setTransitionType(
                    omniboxLoadUrlParams.transitionType | PageTransition.FROM_ADDRESS_BAR);
            if (omniboxLoadUrlParams.inputStartTimestamp != 0) {
                loadUrlParams.setInputStartTimestamp(omniboxLoadUrlParams.inputStartTimestamp);
            }

            if (!TextUtils.isEmpty(omniboxLoadUrlParams.postDataType)) {
                StringBuilder headers = new StringBuilder();
                String prevHeader = loadUrlParams.getVerbatimHeaders();
                if (prevHeader != null && !prevHeader.isEmpty()) {
                    headers.append(prevHeader);
                    headers.append("\r\n");
                }

                headers.append("Content-Type: ");
                headers.append(omniboxLoadUrlParams.postDataType);

                loadUrlParams.setVerbatimHeaders(headers.toString());
            }

            if (omniboxLoadUrlParams.postData != null
                    && omniboxLoadUrlParams.postData.length != 0) {
                loadUrlParams.setPostData(
                        ResourceRequestBody.createFromBytes(omniboxLoadUrlParams.postData));
            }

            if (omniboxLoadUrlParams.openInNewTab
                    && mTabModelSelectorSupplier != null
                    && mTabModelSelectorSupplier.get() != null) {
                mTabModelSelectorSupplier
                        .get()
                        .openNewTab(
                                loadUrlParams,
                                TabLaunchType.FROM_OMNIBOX,
                                currentTab,
                                currentTab.isIncognito());
            } else {
                currentTab.loadUrl(loadUrlParams);
            }
            RecordUserAction.record("MobileOmniboxUse");
        }
        mLocaleManager.recordLocaleBasedSearchMetrics(
                false, url, omniboxLoadUrlParams.transitionType);

        PostTask.postTask(TaskTraits.UI_USER_VISIBLE, () -> focusCurrentTab());
    }

    /* package */ boolean didFocusUrlFromFakebox() {
        return mUrlFocusedFromFakebox;
    }

    /** Recalculates the visibility of the buttons inside the location bar. */
    /* package */ void updateButtonVisibility() {
        updateDeleteButtonVisibility();
        updateMicButtonVisibility();
        updateLensButtonVisibility();
        if (mIsTablet) {
            updateTabletButtonsVisibility();
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
        setUrlBarText(urlBarData, UrlBar.ScrollType.SCROLL_TO_TLD, SelectionState.SELECT_ALL);
    }

    /* package */ void deleteButtonClicked(View view) {
        if (!mNativeInitialized) return;
        RecordUserAction.record("MobileOmniboxDeleteUrl");
        setUrlBarTextEmpty();
        updateButtonVisibility();
    }

    /* package */ void micButtonClicked(View view) {
        if (!mNativeInitialized) return;
        // Hide keyboard before launch voice search to avoid keyboard action announcement in
        // TalkBack to be picked up by voice search.
        mUrlCoordinator.setKeyboardVisibility(false, false);

        RecordUserAction.record("MobileOmniboxVoiceSearch");
        mVoiceRecognitionHandler.startVoiceRecognition(
                mLocationBarLayout.getVoiceRecogintionSource());
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

    /* package */ void setUrlFocusChangeInProgress(boolean inProgress) {
        if (mUrlCoordinator == null) return;
        mIsUrlFocusChangeInProgress = inProgress;
        if (!inProgress) {
            updateButtonVisibility();

            // The accessibility bounding box is not properly updated when focusing the Omnibox
            // from the NTP fakebox.  Clearing/re-requesting focus triggers the bounding box to
            // be recalculated.
            if (didFocusUrlFromFakebox()
                    && mUrlHasFocus
                    && ChromeAccessibilityUtil.get().isAccessibilityEnabled()) {
                String existingText = mUrlCoordinator.getTextWithoutAutocomplete();
                mUrlCoordinator.clearFocus();
                mUrlCoordinator.requestFocus();
                // Existing text (e.g. if the user pasted via the fakebox) from the fake box
                // should be restored after toggling the focus.
                if (!TextUtils.isEmpty(existingText)) {
                    mUrlCoordinator.setUrlBarData(
                            UrlBarData.forNonUrlText(existingText),
                            UrlBar.ScrollType.NO_SCROLL,
                            UrlBarCoordinator.SelectionState.SELECT_END);
                    forceOnTextChanged();
                }
            }

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
            mLocationBarLayout.setUrlActionContainerVisibility(View.GONE);
        }
        if (mIsTablet) {
            float urlFocusChangeFraction = showExpandedState ? 1.0f : 0.0f;
            mLocationBarLayout.setUrlFocusChangePercent(
                    urlFocusChangeFraction, urlFocusChangeFraction, false);
            mLocationBarLayout.updateLayoutParams(
                    MeasureSpec.makeMeasureSpec(
                            mLocationBarLayout.getMeasuredWidth(), MeasureSpec.EXACTLY));
        }
        // Reset to the default values.
        mUrlCoordinator.setSelectAllOnFocus(false);
        mUrlFocusedWithPastedText = false;
    }

    /**
     * Handle and run any necessary animations that are triggered off focusing the UrlBar.
     *
     * @param hasFocus Whether focus was gained.
     */
    @VisibleForTesting
    /* package */ void handleUrlFocusAnimation(boolean hasFocus) {
        if (hasFocus) {
            mUrlFocusedWithoutAnimations = false;
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

        if (shouldShowSaveOfflineButton()) {
            animators.add(
                    createShowButtonAnimatorForTablet(
                            locationBarTablet.getSaveOfflineButtonForAnimation()));
        } else {
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

        if (shouldShowSaveOfflineButton() && locationBarTablet.isSaveOfflineButtonVisible()) {
            animators.add(
                    createHideButtonAnimatorForTablet(
                            locationBarTablet.getSaveOfflineButtonForAnimation()));
        } else if (!(mUrlHasFocus && !locationBarTablet.isDeleteButtonVisible())) {
            // If the save offline button isn't enabled, the microphone button always shows when
            // buttons are shown in the unfocused location bar. When buttons are hidden in the
            // unfocused location bar, the microphone shows if the location bar is focused and the
            // delete button isn't showing. The microphone button should not be hidden if the
            // url bar is currently focused and the delete button isn't showing.
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
     * @param selectionState Specifies how the text should be selected in the focused state.
     * @return Whether the URL was changed as a result of this call.
     */
    /* package */ boolean setUrlBarText(
            UrlBarData urlBarData,
            @UrlBar.ScrollType int scrollType,
            @SelectionState int selectionState) {
        return mUrlCoordinator.setUrlBarData(urlBarData, scrollType, selectionState);
    }

    /**
     * Clear any text in the URL bar.
     *
     * @return Whether this changed the existing text.
     */
    /* package */ boolean setUrlBarTextEmpty() {
        boolean textChanged =
                mUrlCoordinator.setUrlBarData(
                        UrlBarData.EMPTY,
                        UrlBar.ScrollType.SCROLL_TO_BEGINNING,
                        SelectionState.SELECT_ALL);
        forceOnTextChanged();
        return textChanged;
    }

    /* package */ void forceOnTextChanged() {
        String textWithoutAutocomplete = mUrlCoordinator.getTextWithoutAutocomplete();
        mAutocompleteCoordinator.onTextChanged(textWithoutAutocomplete);
    }

    // Private methods

    private void setProfile(Profile profile) {
        if (profile == null || !mNativeInitialized) return;
        mOmniboxPrerender.initializeForProfile(profile);
        mSearchEngineUtils = SearchEngineUtils.getForProfile(profile);
        mLocationBarLayout.setSearchEngineUtils(mSearchEngineUtils);
    }

    private void focusCurrentTab() {
        assert mLocationBarDataProvider != null;
        if (mLocationBarDataProvider.hasTab()) {
            View view = mLocationBarDataProvider.getTab().getView();
            if (view != null) view.requestFocus();
        }
    }

    /** Update visuals to use a correct color scheme depending on the primary color. */
    @VisibleForTesting
    /* package */ void updateBrandedColorScheme() {
        mBrandedColorScheme =
                OmniboxResourceProvider.getBrandedColorScheme(
                        mContext,
                        mLocationBarDataProvider.isIncognitoBranded(),
                        getPrimaryBackgroundColor());

        // The delete button only appears when the url bar has focus, so its tint is rather static,
        // and need not be assigned in updateButtonTints().
        mLocationBarLayout.setDeleteButtonTint(
                ThemeUtils.getThemedToolbarIconTint(mContext, mBrandedColorScheme));
        // If the URL changed colors and is not focused, update the URL to account for the new
        // color scheme.
        if (mUrlCoordinator.setBrandedColorScheme(mBrandedColorScheme) && !isUrlBarFocused()) {
            updateUrl();
        }
        mStatusCoordinator.setBrandedColorScheme(mBrandedColorScheme);
        if (mAutocompleteCoordinator != null) {
            mAutocompleteCoordinator.updateVisualsForState(mBrandedColorScheme);
        }
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
        boolean shouldAnimate =
                mIsTablet ? isUrlBarFocused() : isUrlBarFocused() || mIsUrlFocusChangeInProgress;
        mStatusCoordinator.setShouldAnimateIconChanges(shouldAnimate);
    }

    private void recordOmniboxFocusReason(@OmniboxFocusReason int reason) {
        RecordHistogram.recordEnumeratedHistogram(
                "Android.OmniboxFocusReason", reason, OmniboxFocusReason.NUM_ENTRIES);
    }

    /** Updates the display of the mic button. */
    private void updateMicButtonVisibility() {
        mLocationBarLayout.setMicButtonVisibility(shouldShowMicButton());
    }

    private void updateLensButtonVisibility() {
        boolean shouldShowLensButton = shouldShowLensButton();
        LensMetrics.recordShown(LensEntryPoint.OMNIBOX, shouldShowLensButton);
        mLocationBarLayout.setLensButtonVisibility(shouldShowLensButton);
    }

    private void updateDeleteButtonVisibility() {
        mLocationBarLayout.setDeleteButtonVisibility(shouldShowDeleteButton());
    }

    private void updateTabletButtonsVisibility() {
        assert mIsTablet;
        LocationBarTablet locationBarTablet = (LocationBarTablet) mLocationBarLayout;
        boolean showBookmarkButton =
                mShouldShowButtonsWhenUnfocused && shouldShowPageActionButtons();
        locationBarTablet.setBookmarkButtonVisibility(showBookmarkButton);

        boolean showSaveOfflineButton =
                mShouldShowButtonsWhenUnfocused && shouldShowSaveOfflineButton();
        locationBarTablet.setSaveOfflineButtonVisibility(
                showSaveOfflineButton, isSaveOfflineButtonEnabled());
    }

    /**
     * @return Whether the delete button should be shown.
     */
    private boolean shouldShowDeleteButton() {
        // Show the delete button at the end when the bar has focus and has some text.
        boolean hasText =
                mUrlCoordinator != null
                        && !TextUtils.isEmpty(mUrlCoordinator.getTextWithAutocomplete());
        return hasText && (mUrlHasFocus || mIsUrlFocusChangeInProgress);
    }

    private boolean shouldShowMicButton() {
        if (shouldShowDeleteButton()) return false;
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

    private boolean shouldShowLensButton() {
        if (shouldShowDeleteButton()) return false;

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

    private boolean isLensOnOmniboxEnabled() {
        if (sLastCachedIsLensOnOmniboxEnabled == null) {
            sLastCachedIsLensOnOmniboxEnabled =
                    Boolean.valueOf(isLensEnabled(LensEntryPoint.OMNIBOX));
        }

        return sLastCachedIsLensOnOmniboxEnabled.booleanValue();
    }

    private boolean shouldShowSaveOfflineButton() {
        assert mIsTablet;
        if (!mNativeInitialized || mLocationBarDataProvider == null) return false;
        Tab tab = mLocationBarDataProvider.getTab();
        if (tab == null) return false;
        // The save offline button should not be shown on native pages. Currently, trying to
        // save an offline page in incognito crashes, so don't show it on incognito either.
        return shouldShowPageActionButtons() && !tab.isOffTheRecord();
    }

    private boolean isSaveOfflineButtonEnabled() {
        if (mLocationBarDataProvider == null) return false;
        return mSaveOfflineButtonState.isEnabled(mLocationBarDataProvider.getTab());
    }

    private boolean shouldShowPageActionButtons() {
        assert mIsTablet;
        if (!mNativeInitialized) return true;

        // There are two actions, bookmark and save offline, and they should be shown if the
        // omnibox isn't focused.
        return !(mUrlHasFocus || mIsUrlFocusChangeInProgress);
    }

    private void updateUrl() {
        setUrl(mLocationBarDataProvider.getCurrentGurl(), mLocationBarDataProvider.getUrlBarData());
    }

    private void updateOmniboxPrerender() {
        if (mOmniboxPrerender == null) return;
        // Profile may be null if switching to a tab that has not yet been initialized.
        if (!mProfileSupplier.hasValue()) return;
        mOmniboxPrerender.clear(mProfileSupplier.get());
    }

    @SuppressLint("GestureBackNavigation")
    private boolean handleKeyEvent(View view, int keyCode, KeyEvent event) {
        boolean isRtl = view.getLayoutDirection() == View.LAYOUT_DIRECTION_RTL;
        if (mAutocompleteCoordinator.handleKeyEvent(keyCode, event)) {
            return true;
        } else if (keyCode == KeyEvent.KEYCODE_ESCAPE) {
            if (KeyNavigationUtil.isActionDown(event) && event.getRepeatCount() == 0) {
                revertChanges();
                return true;
            }
        } else if ((!isRtl && KeyNavigationUtil.isGoRight(event))
                || (isRtl && KeyNavigationUtil.isGoLeft(event))) {
            // Ensures URL bar doesn't lose focus, when RIGHT or LEFT (RTL) key is pressed while
            // the cursor is positioned at the end of the text.
            TextView tv = (TextView) view;
            return tv.getSelectionStart() == tv.getSelectionEnd()
                    && tv.getSelectionEnd() == tv.getText().length();
        }
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
        sLastCachedIsLensOnOmniboxEnabled = Boolean.valueOf(isLensEnabled(LensEntryPoint.OMNIBOX));
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
    public void onUrlChanged() {
        updateUrl();
        updateOmniboxPrerender();
        updateButtonVisibility();
    }

    @Override
    public void hintZeroSuggestRefresh() {
        mAutocompleteCoordinator.prefetchZeroSuggestResults();
    }

    // TemplateUrlService.TemplateUrlServiceObserver implementation
    @Override
    public void onTemplateURLServiceChanged() {
        sLastCachedIsLensOnOmniboxEnabled = Boolean.valueOf(isLensEnabled(LensEntryPoint.OMNIBOX));
    }

    // OmniboxStub implementation.

    @Override
    public void setUrlBarFocus(boolean shouldBeFocused, @Nullable String pastedText, int reason) {
        boolean urlHasFocus = mUrlHasFocus;
        if (shouldBeFocused) {
            if (!urlHasFocus) {
                recordOmniboxFocusReason(reason);
                // Record Lens button shown when Omnibox is focused.
                if (shouldShowLensButton()) LensMetrics.recordOmniboxFocusedWhenLensShown();
            }

            if (reason == OmniboxFocusReason.FAKE_BOX_TAP
                    || reason == OmniboxFocusReason.FAKE_BOX_LONG_PRESS
                    || reason == OmniboxFocusReason.TASKS_SURFACE_FAKE_BOX_LONG_PRESS
                    || reason == OmniboxFocusReason.TASKS_SURFACE_FAKE_BOX_TAP) {
                mUrlFocusedFromFakebox = true;
            }

            mUrlFocusedWithPastedText = pastedText != null;

            if (urlHasFocus && mUrlFocusedWithoutAnimations) {
                handleUrlFocusAnimation(true);
            } else {
                mUrlCoordinator.requestFocus();
            }
        } else {
            assert pastedText == null;
            mUrlCoordinator.clearFocus();
        }

        if (pastedText != null) {
            // This must be happen after requestUrlFocus(), which changes the selection.
            mUrlCoordinator.setUrlBarData(
                    UrlBarData.forNonUrlText(pastedText),
                    UrlBar.ScrollType.NO_SCROLL,
                    UrlBarCoordinator.SelectionState.SELECT_END);
            /*
             When the URL bar text is programmatically set on omnibox state restoration during a
             device fold transition, {@code AutocompleteEditText#getTextWithoutAutocomplete()}
             invoked by {@code #forceOnTextChanged()} returns an empty string because {@code
             AutocompleteEditText#mModel} is not initialized. To trigger the autocomplete system in
             this case, {@code AutocompleteCoordinator#onTextChanged()} will be directly called on
             the restored omnibox text input.
            */
            if (reason == OmniboxFocusReason.FOLD_TRANSITION_RESTORATION) {
                mAutocompleteCoordinator.onTextChanged(pastedText);
            } else {
                forceOnTextChanged();
            }
        }
    }

    @Override
    public void performSearchQuery(String query, List<String> searchParams) {
        if (TextUtils.isEmpty(query)) return;

        String queryUrl =
                mTemplateUrlServiceSupplier.get().getUrlForSearchQuery(query, searchParams);

        if (!TextUtils.isEmpty(queryUrl)) {
            loadUrl(
                    new OmniboxLoadUrlParams.Builder(queryUrl, PageTransition.GENERATED)
                            .setOpenInNewTab(false)
                            .build());
        } else {
            setSearchQuery(query);
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

    @Override
    public void clearOmniboxFocus() {
        setUrlBarFocus(
                /* shouldBeFocused= */ false, /* pastedText= */ null, OmniboxFocusReason.UNFOCUS);
    }

    @Override
    public void notifyVoiceRecognitionCanceled() {}

    // VoiceRecognitionHandler.Delegate implementation.

    @Override
    public void loadUrlFromVoice(String url) {
        loadUrl(
                new OmniboxLoadUrlParams.Builder(url, PageTransition.TYPED)
                        .setOpenInNewTab(false)
                        .build());
    }

    @Override
    public void onVoiceAvailabilityImpacted() {
        updateButtonVisibility();
    }

    @Override
    public void setSearchQuery(String query) {
        if (TextUtils.isEmpty(query)) return;

        if (!mNativeInitialized) {
            mDeferredNativeRunnables.add(() -> setSearchQuery(query));
            return;
        }

        // Ensure the UrlBar has focus before entering text. If the UrlBar is not focused,
        // autocomplete text will be updated but the visible text will not.
        setUrlBarFocus(
                /* shouldBeFocused= */ true,
                /* pastedText= */ null,
                OmniboxFocusReason.SEARCH_QUERY);
        setUrlBarText(
                UrlBarData.forNonUrlText(query),
                UrlBar.ScrollType.NO_SCROLL,
                SelectionState.SELECT_ALL);
        mAutocompleteCoordinator.startAutocompleteForQuery(query);
        mUrlCoordinator.setKeyboardVisibility(true, false);
    }

    @Override
    public LocationBarDataProvider getLocationBarDataProvider() {
        return mLocationBarDataProvider;
    }

    @Override
    public AutocompleteCoordinator getAutocompleteCoordinator() {
        return mAutocompleteCoordinator;
    }

    @Override
    public WindowAndroid getWindowAndroid() {
        return mWindowAndroid;
    }

    // UrlBarDelegate implementation.

    @Nullable
    @Override
    public View getViewForUrlBackFocus() {
        assert mLocationBarDataProvider != null;
        Tab tab = mLocationBarDataProvider.getTab();
        return tab == null ? null : tab.getView();
    }

    @Override
    public boolean allowKeyboardLearning() {
        return !mLocationBarDataProvider.isOffTheRecord();
    }

    // Traditional way to intercept keycode_back, which is deprecated from T.
    @Override
    public void backKeyPressed() {
        if (mBackKeyBehavior.handleBackKeyPressed()) {
            return;
        }

        setUrlBarFocus(false, null, OmniboxFocusReason.UNFOCUS);
        // Revert the URL to match the current page.
        setUrl(mLocationBarDataProvider.getCurrentGurl(), mLocationBarDataProvider.getUrlBarData());
        focusCurrentTab();
    }

    @Override
    public void onFocusByTouch() {
        recordOmniboxFocusReason(OmniboxFocusReason.OMNIBOX_TAP);
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
        if (!TextUtils.isEmpty(mUrlCoordinator.getTextWithoutAutocomplete())) return;
        recordOmniboxFocusReason(OmniboxFocusReason.TAP_AFTER_FOCUS_FROM_KEYBOARD);
        completeUrlFocusAnimationAndEnableSuggestions();
    }

    /**
     * Trigger focus animations to adequately enable Autocomplete and Suggestions. This is required
     * only when the intention is to trigger the suggestions dropdown after the omnibox has gained
     * focus without animations.
     *
     * <p>This call trusts the caller has performed all necessary verifications, and will display
     * suggestions unconditionally.
     */
    /* package */ void completeUrlFocusAnimationAndEnableSuggestions() {
        if (!mUrlFocusedWithoutAnimations || mUrlCoordinator == null) return;
        handleUrlFocusAnimation(true);
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
    public ObservableSupplier<Boolean> getHandleBackPressChangedSupplier() {
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
        if (mUrlHasFocus
                && mUrlFocusedWithoutAnimations
                && newConfig.keyboard != Configuration.KEYBOARD_QWERTY) {
            // If we lose the hardware keyboard and the focus animations were not run, then the
            // user has not typed any text, so we will just clear the focus instead.
            setUrlBarFocus(
                    /* shouldBeFocused= */ false,
                    /* pastedText= */ null,
                    OmniboxFocusReason.UNFOCUS);
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
        if (OmniboxFeatures.sUseFusedLocationProvider.isEnabled()
                && mProfileSupplier.hasValue()
                && mTemplateUrlServiceSupplier.hasValue()) {
            GeolocationHeader.primeLocationForGeoHeaderIfEnabled(
                    mProfileSupplier.get(), mTemplateUrlServiceSupplier.get());
        }
    }

    @Override
    public void onPauseWithNative() {
        if (OmniboxFeatures.sUseFusedLocationProvider.isEnabled()) {
            GeolocationHeader.stopListeningForLocationUpdates();
        }
    }

    /** Updates the tints of UI buttons. */
    private void updateButtonTints() {
        ColorStateList tint = ThemeUtils.getThemedToolbarIconTint(mContext, mBrandedColorScheme);
        mLocationBarLayout.setMicButtonTint(tint);
        mLocationBarLayout.setLensButtonTint(tint);
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

    public void maybeShowDefaultBrowserPromo() {
        DefaultBrowserPromoUtils.getInstance()
                .maybeShowDefaultBrowserPromoMessages(
                        mContext, mWindowAndroid, mProfileSupplier.get());
    }
}
