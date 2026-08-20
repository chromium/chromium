// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.ui.bottombar;

import android.content.Context;
import android.content.SharedPreferences;
import android.content.res.ColorStateList;
import android.os.SystemClock;

import org.chromium.base.Callback;
import org.chromium.base.ContextUtils;
import org.chromium.base.TriState;
import org.chromium.base.TriStateUtils;
import org.chromium.base.lifetime.Destroyable;
import org.chromium.base.supplier.NonNullObservableSupplier;
import org.chromium.base.supplier.NullableObservableSupplier;
import org.chromium.base.supplier.OneshotSupplier;
import org.chromium.build.annotations.NullMarked;
import org.chromium.build.annotations.Nullable;
import org.chromium.chrome.browser.feature_engagement.TrackerFactory;
import org.chromium.chrome.browser.glic.GlicEnabling;
import org.chromium.chrome.browser.glic.GlicKeyedService;
import org.chromium.chrome.browser.glic.GlicKeyedServiceFactory;
import org.chromium.chrome.browser.layouts.LayoutStateProvider;
import org.chromium.chrome.browser.layouts.LayoutStateProvider.LayoutStateObserver;
import org.chromium.chrome.browser.layouts.LayoutType;
import org.chromium.chrome.browser.preferences.ChromePreferenceKeys;
import org.chromium.chrome.browser.profiles.Profile;
import org.chromium.chrome.browser.search_engines.TemplateUrlServiceFactory;
import org.chromium.chrome.browser.tab.EmptyTabObserver;
import org.chromium.chrome.browser.tab.Tab;
import org.chromium.chrome.browser.tab.TabObserver;
import org.chromium.chrome.browser.theme.ThemeColorProvider;
import org.chromium.chrome.browser.ui.actions.ActionId;
import org.chromium.chrome.browser.ui.actions.ActionProperties;
import org.chromium.chrome.browser.ui.actions.ActionRegistry;
import org.chromium.chrome.browser.ui.android.bars_common.IphIntent;
import org.chromium.chrome.browser.ui.bottombar.BottomBarHostManager.Host;
import org.chromium.chrome.browser.ui.bottombar.BottomBarMetrics.AimIneligibilityReason;
import org.chromium.chrome.browser.ui.bottombar.BottomBarMetrics.CandidateAction;
import org.chromium.chrome.browser.ui.theme.BrandedColorScheme;
import org.chromium.components.browser_ui.widget.highlight.ViewHighlighter.HighlightParams;
import org.chromium.components.browser_ui.widget.highlight.ViewHighlighter.HighlightShape;
import org.chromium.components.embedder_support.util.UrlUtilities;
import org.chromium.components.feature_engagement.FeatureConstants;
import org.chromium.components.feature_engagement.Tracker;
import org.chromium.components.search_engines.TemplateUrlService;
import org.chromium.components.search_engines.TemplateUrlService.TemplateUrlServiceObserver;
import org.chromium.ui.modelutil.PropertyModel;

/** Mediator for the bottom bar */
@NullMarked
public class BottomBarMediator
        implements ThemeColorProvider.TintObserver,
                BottomBarButtonManager.Listener,
                BottomBarPromoDialogCoordinator.BottomBarPromoDialogListener,
                LayoutStateObserver,
                SharedPreferences.OnSharedPreferenceChangeListener,
                Destroyable {
    /** Delegate for compositor-level visibility changes. */
    public interface VisibilityDelegate {
        /**
         * Called when the visibility of the bottom bar changes.
         *
         * @param isVisible True if the bottom bar is visible, false otherwise.
         */
        void onVisibilityChanged(boolean isVisible);

        /** Called when the model state changes and a new screenshot is needed. */
        void onModelTokenChange();

        /** Called when the background color of the bottom bar changes. */
        void onBackgroundColorChanged();
    }

    // Dependencies
    private final Context mContext;
    private final PropertyModel mModel;
    private final BottomBarButtonManager mButtonManager;
    private final ThemeColorProvider mThemeColorProvider;
    private final VisibilityDelegate mVisibilityDelegate;
    private final BottomBarPromoDialogCoordinator mPromoDialogCoordinator;
    private final NullableObservableSupplier<Tab> mTabSupplier;
    private final NonNullObservableSupplier<Boolean> mHomepageEnabledSupplier;
    private final NonNullObservableSupplier<Boolean> mOmniboxFocusStateSupplier;
    private final NullableObservableSupplier<Profile> mProfileSupplier;
    private final OneshotSupplier<String> mCountrySupplier;
    private final NullableObservableSupplier<PropertyModel> mGlicActionSupplier;
    private final NullableObservableSupplier<PropertyModel> mAiModeActionSupplier;
    private final NullableObservableSupplier<PropertyModel> mNewTabActionSupplier;
    private final boolean mShouldIncludeHomeButton;

    // Observers and Callbacks
    private final TabObserver mTabObserver;
    private final Callback<@Nullable Tab> mTabSupplierObserver = this::onTabChanged;
    private final Callback<Boolean> mHomepageEnabledObserver = this::onHomepageEnabledChanged;
    private final Callback<Boolean> mOmniboxFocusObserver = this::onOmniboxFocusChanged;
    private final Callback<@Nullable Profile> mProfileObserver = this::onProfileChanged;
    private final GlicKeyedService.AllowedChangedObserver mAllowedChangedObserver =
            this::onGlicAllowedChanged;

    // Mutable State (Nullable)
    private @Nullable GlicKeyedService mGlicKeyedService;
    private @Nullable Profile mOriginalProfile;
    private @Nullable Tab mCurrentTab;
    private @TriState int mIsVisible;
    private @Nullable IphIntent mNewTabIphIntent;
    private @Nullable TemplateUrlService mTemplateUrlService;
    private @Nullable TemplateUrlServiceObserver mTemplateUrlServiceObserver;
    private @Nullable LayoutStateProvider mLayoutStateProvider;
    private @Nullable @ActionId Integer mResolvedCandidateExtraAction;

    // Mutable State (Primitive / Non-null)
    private boolean mGlicWasVisible;
    private boolean mGlicTimeToAppearRecorded;
    private boolean mShouldHideForHub;
    private boolean mDestroyed;
    private long mBottomBarShownTimeMs = -1;
    private long mGlicAppearedTimeMs = -1;
    private boolean mStartupPromoFlowFinished;
    private boolean mObservingSharedPrefs;
    private @Host int mHost = Host.TABBED;

    /**
     * @param context The context to use for the bottom bar.
     * @param model The property model to update.
     * @param buttonManager The {@link BottomBarButtonManager} for the bottom bar buttons.
     * @param themeColorProvider The provider to observe theme changes from.
     * @param tabSupplier Supplier of the current tab.
     * @param homepageEnabledSupplier Supplier of whether the homepage is enabled.
     * @param visibilityDelegate Delegate to handle compositor-level visibility changes.
     * @param shouldIncludeHomeButton Whether the home button should be included in the bottom bar.
     * @param profileSupplier Supplier of the current profile.
     * @param countrySupplier Supplier of the latest variations country code.
     * @param omniboxFocusStateSupplier Supplier of the omnibox focus state.
     * @param promoDialogCoordinator The {@link BottomBarPromoDialogCoordinator} for the promo
     *     dialog.
     * @param actionRegistry The {@link ActionRegistry}.
     */
    public BottomBarMediator(
            Context context,
            PropertyModel model,
            BottomBarButtonManager buttonManager,
            ThemeColorProvider themeColorProvider,
            NullableObservableSupplier<Tab> tabSupplier,
            NonNullObservableSupplier<Boolean> homepageEnabledSupplier,
            VisibilityDelegate visibilityDelegate,
            boolean shouldIncludeHomeButton,
            NullableObservableSupplier<Profile> profileSupplier,
            OneshotSupplier<String> countrySupplier,
            NonNullObservableSupplier<Boolean> omniboxFocusStateSupplier,
            BottomBarPromoDialogCoordinator promoDialogCoordinator,
            ActionRegistry actionRegistry,
            LayoutStateProvider layoutStateProvider) {
        mContext = context;
        mModel = model;
        mButtonManager = buttonManager;
        mThemeColorProvider = themeColorProvider;
        mTabSupplier = tabSupplier;
        mHomepageEnabledSupplier = homepageEnabledSupplier;
        mVisibilityDelegate = visibilityDelegate;
        mShouldIncludeHomeButton = shouldIncludeHomeButton;
        mProfileSupplier = profileSupplier;
        mCountrySupplier = countrySupplier;
        mOmniboxFocusStateSupplier = omniboxFocusStateSupplier;
        mPromoDialogCoordinator = promoDialogCoordinator;
        mGlicActionSupplier = actionRegistry.get(ActionId.GLIC);
        mAiModeActionSupplier = actionRegistry.get(ActionId.AI_MODE);
        mNewTabActionSupplier = actionRegistry.get(ActionId.NEW_TAB);
        mGlicTimeToAppearRecorded = false;

        if (!BottomBarConfigUtils.shouldShowOnGts()) {
            layoutStateProvider.addObserver(this);
            mShouldHideForHub = layoutStateProvider.isLayoutVisible(LayoutType.HUB);
            mLayoutStateProvider = layoutStateProvider;
        }

        mTabObserver =
                new EmptyTabObserver() {
                    @Override
                    public void onUrlUpdated(Tab tab) {
                        updateVisibility();
                    }
                };

        mThemeColorProvider.addTintObserver(this);
        mModel.set(BottomBarProperties.COLOR_SCHEME, mThemeColorProvider.getBrandedColorScheme());
        mProfileSupplier.addSyncObserverAndCallIfNonNull(mProfileObserver);
        mCountrySupplier.onAvailable((country) -> updateExtraActionVisibility());
        mOmniboxFocusStateSupplier.addSyncObserver(mOmniboxFocusObserver);
        onTabChanged(mTabSupplier.addSyncObserver(mTabSupplierObserver));
        if (mShouldIncludeHomeButton) {
            mHomepageEnabledSupplier.addSyncObserverAndCallIfNonNull(mHomepageEnabledObserver);
        }

        // Safe to set the listener after all observers are initialized to trigger the immediate
        // callback with the correct state.
        mButtonManager.setListener(this);
    }

    private void onTabChanged(@Nullable Tab tab) {
        if (mCurrentTab != null) {
            mCurrentTab.removeObserver(mTabObserver);
        }
        mCurrentTab = tab;
        if (mCurrentTab != null) {
            mCurrentTab.addObserver(mTabObserver);
        }
        updateVisibility();
    }

    private void onOmniboxFocusChanged(boolean focused) {
        updateVisibility();
    }

    @Override
    public void onFinishedShowing(@LayoutType int layoutType) {
        if (layoutType == LayoutType.HUB) {
            mShouldHideForHub = true;
            updateVisibility();
        }
    }

    @Override
    public void onStartedHiding(@LayoutType int layoutType) {
        if (layoutType == LayoutType.HUB) {
            mShouldHideForHub = false;
            updateVisibility();
        }
    }

    private void updateVisibility() {
        boolean currentTabIsRegularNtp =
                mCurrentTab != null
                        && UrlUtilities.isNtpUrl(mCurrentTab.getUrl())
                        && !mCurrentTab.isOffTheRecord();
        boolean isOmniboxFocused = mOmniboxFocusStateSupplier.get();
        boolean shouldDisableOnNtp =
                BottomBarConfigUtils.shouldDisableOnNtp() && currentTabIsRegularNtp;
        boolean isVisible = !shouldDisableOnNtp && !isOmniboxFocused && !mShouldHideForHub;

        if (mIsVisible == TriStateUtils.from(isVisible)) return;

        boolean didBecomeVisible = isVisible && mIsVisible != TriState.TRUE;
        mIsVisible = TriStateUtils.from(isVisible);

        mModel.set(BottomBarProperties.IS_VISIBLE, isVisible);
        mVisibilityDelegate.onVisibilityChanged(isVisible);

        if (didBecomeVisible) {
            mBottomBarShownTimeMs = SystemClock.uptimeMillis();
            if (mGlicAppearedTimeMs != -1 && !mGlicTimeToAppearRecorded) {
                BottomBarMetrics.recordGlicTimeToAppear(0);
                mGlicTimeToAppearRecorded = true;
            }
            maybeShowIphs();
        }
    }

    /**
     * Notifies the mediator that the startup promo flow has finished.
     *
     * <p>This marks the startup promo flow as finished, unlocking the ability to show BottomBar
     * IPHs. If no startup promo was shown, it attempts to show the IPHs immediately. If a promo was
     * shown, it defers showing IPHs until a subsequent visibility change to avoid
     * double-promotions.
     *
     * @param promoShown True if any startup promo was shown during the startup flow.
     */
    public void onStartupPromoFlowFinished(boolean promoShown) {
        mStartupPromoFlowFinished = true;
        if (!promoShown) {
            maybeShowIphs();
        }
    }

    private void maybeShowIphs() {
        if (!mStartupPromoFlowFinished) return;
        boolean isBottomBarVisible = mIsVisible == TriState.TRUE;
        boolean isExtraVisible = mModel.get(BottomBarProperties.IS_EXTRA_BUTTON_VISIBLE);
        if (isBottomBarVisible && isExtraVisible) {
            Profile profile = mProfileSupplier.get();
            Tracker tracker =
                    profile == null
                            ? null
                            : TrackerFactory.getTrackerForProfile(profile.getOriginalProfile());
            boolean hasSeenPromo =
                    tracker != null
                            && tracker.hasEverTriggered(
                                    FeatureConstants.ANDROID_BOTTOM_BAR_PROMO_DIALOG, false);

            if (!mPromoDialogCoordinator.isShowing() && hasSeenPromo) {
                triggerNewTabIph();
            }
        } else if (isBottomBarVisible) {
            // Trigger the new tab IPH if the bottom bar is visible but the GLIC button is not
            // visible.
            triggerNewTabIph();
        }
    }

    private void onProfileChanged(@Nullable Profile profile) {
        updateExtraActionVisibility();
    }

    private void updateExtraActionVisibility() {
        if (mDestroyed) return;
        Profile profile = mProfileSupplier.get();
        if (profile == null) {
            setButtonVisibility(ActionId.GLIC, false);
            setButtonVisibility(ActionId.AI_MODE, false);
            return;
        }

        Profile originalProfile = profile.getOriginalProfile();
        String country = mCountrySupplier.get();

        if (mResolvedCandidateExtraAction == null) {
            // Check if prerequisites for resolution are satisfied.
            if (!BottomBarActionEligibility.isCandidateResolutionReady(originalProfile, country)) {
                // Country code not yet populated and geofencing not bypassed: defer decision.
                setButtonVisibility(ActionId.GLIC, /* visible= */ false);
                setButtonVisibility(ActionId.AI_MODE, /* visible= */ false);
                return;
            }

            long startTime = SystemClock.uptimeMillis();
            BottomBarActionEligibility.getCandidateExtraAction(originalProfile, country);
            Integer candidateExtraAction =
                    BottomBarActionEligibility.getCachedCandidateExtraAction();
            mResolvedCandidateExtraAction = candidateExtraAction;
            long decisionDuration = SystemClock.uptimeMillis() - startTime;
            BottomBarMetrics.recordCandidateDecisionTime(decisionDuration);

            @CandidateAction int candidateMetric;
            if (candidateExtraAction != null && candidateExtraAction == ActionId.GLIC) {
                candidateMetric = CandidateAction.GLIC;
            } else if (candidateExtraAction != null && candidateExtraAction == ActionId.AI_MODE) {
                candidateMetric = CandidateAction.AIM;
            } else {
                candidateMetric = CandidateAction.NONE;
            }
            BottomBarMetrics.recordCandidateExtraAction(candidateMetric);
        }

        updateObservers(originalProfile);

        Integer candidateExtraAction = mResolvedCandidateExtraAction;
        if (candidateExtraAction != null && candidateExtraAction == ActionId.GLIC) {
            updateGlicVisibility(originalProfile);
        } else if (candidateExtraAction != null && candidateExtraAction == ActionId.AI_MODE) {
            updateAiModeVisibility();
        } else {
            setButtonVisibility(ActionId.GLIC, /* visible= */ false);
            setButtonVisibility(ActionId.AI_MODE, /* visible= */ false);
        }
    }

    private void updateGlicVisibility(@Nullable Profile originalProfile) {
        Integer candidateExtraAction = mResolvedCandidateExtraAction;
        if (originalProfile == null
                || candidateExtraAction == null
                || candidateExtraAction != ActionId.GLIC) {
            setButtonVisibility(ActionId.GLIC, /* visible= */ false);
            return;
        }

        String country = mCountrySupplier.get();
        boolean visible =
                (GlicEnabling.isPolicyEnforced(originalProfile)
                                || BottomBarConfigUtils.isGlicButtonEnabled())
                        && BottomBarActionEligibility.isGlicAllowedInCountry(country);

        if (visible && !mGlicWasVisible) {
            mGlicAppearedTimeMs = SystemClock.uptimeMillis();
            if (mBottomBarShownTimeMs != -1 && !mGlicTimeToAppearRecorded) {
                long timeSinceShown = mGlicAppearedTimeMs - mBottomBarShownTimeMs;
                BottomBarMetrics.recordGlicTimeToAppear(timeSinceShown);
                mGlicTimeToAppearRecorded = true;
            }
        }
        mGlicWasVisible = visible;

        setButtonVisibility(ActionId.AI_MODE, /* visible= */ false);
        setButtonVisibility(ActionId.GLIC, visible);
    }

    private void updateAiModeVisibility() {
        Integer candidateExtraAction = mResolvedCandidateExtraAction;
        if (candidateExtraAction == null || candidateExtraAction != ActionId.AI_MODE) {
            setButtonVisibility(ActionId.AI_MODE, /* visible= */ false);
            return;
        }

        boolean visible =
                mTemplateUrlService != null && mTemplateUrlService.isDefaultSearchEngineGoogle();

        if (!visible) {
            BottomBarMetrics.recordAimIneligibilityReason(
                    AimIneligibilityReason.DEFAULT_SEARCH_ENGINE_NOT_GOOGLE);
        }

        setButtonVisibility(ActionId.GLIC, /* visible= */ false);
        setButtonVisibility(ActionId.AI_MODE, visible);
    }

    private void updateObservers(@Nullable Profile originalProfile) {
        if (mOriginalProfile == originalProfile) {
            return;
        }
        mOriginalProfile = originalProfile;

        if (mGlicKeyedService != null) {
            mGlicKeyedService.removeAllowedChangedObserver(mAllowedChangedObserver);
            mGlicKeyedService = null;
        }
        if (mObservingSharedPrefs) {
            ContextUtils.getAppSharedPreferences().unregisterOnSharedPreferenceChangeListener(this);
            mObservingSharedPrefs = false;
        }
        if (mTemplateUrlService != null && mTemplateUrlServiceObserver != null) {
            mTemplateUrlService.removeObserver(mTemplateUrlServiceObserver);
            mTemplateUrlService = null;
            mTemplateUrlServiceObserver = null;
        }

        Integer candidateExtraAction = mResolvedCandidateExtraAction;
        if (originalProfile == null || candidateExtraAction == null) {
            return;
        }

        if (candidateExtraAction == ActionId.GLIC) {
            GlicKeyedService glicKeyedService =
                    GlicKeyedServiceFactory.getForProfile(originalProfile);
            mGlicKeyedService = glicKeyedService;
            if (mGlicKeyedService != null) {
                mGlicKeyedService.addAllowedChangedObserver(mAllowedChangedObserver);
            }
            ContextUtils.getAppSharedPreferences().registerOnSharedPreferenceChangeListener(this);
            mObservingSharedPrefs = true;
        } else if (candidateExtraAction == ActionId.AI_MODE) {
            mTemplateUrlService = TemplateUrlServiceFactory.getForProfile(originalProfile);
            if (mTemplateUrlService != null) {
                mTemplateUrlServiceObserver = this::onTemplateURLServiceChanged;
                mTemplateUrlService.addObserver(mTemplateUrlServiceObserver);
            }
        }
    }

    private void onGlicAllowedChanged() {
        updateGlicVisibility(mOriginalProfile);
    }

    private void onTemplateURLServiceChanged() {
        updateAiModeVisibility();
    }

    @Override
    public void onSharedPreferenceChanged(
            SharedPreferences sharedPreferences, @Nullable String key) {
        if (ChromePreferenceKeys.BOTTOM_BAR_GLIC_BUTTON_ENABLED.equals(key)) {
            updateGlicVisibility(mOriginalProfile);
        }
    }

    private void onHomepageEnabledChanged(boolean isEnabled) {
        setButtonVisibility(ActionId.HOME_BUTTON, isEnabled);
    }

    private void setButtonVisibility(int actionId, boolean visible) {
        mButtonManager.setButtonVisibility(actionId, visible);
    }

    @Override
    public void onButtonVisibilityChanged(int actionId, boolean visible) {
        if (actionId == ActionId.GLIC && visible) {
            maybeShowIphs();
        }
    }

    @Override
    public void onBottomBarStateChanged(boolean visibilityChanged) {
        mVisibilityDelegate.onModelTokenChange();
        if (visibilityChanged) {
            updateNewTabButtonBackground();
        }
    }

    private void updateNewTabButtonBackground() {
        boolean isCentered = mButtonManager.hasCenteredButton();
        boolean current = mModel.get(BottomBarProperties.IS_NEW_TAB_BACKGROUND_VISIBLE);
        if (current != isCentered) {
            mModel.set(BottomBarProperties.IS_NEW_TAB_BACKGROUND_VISIBLE, isCentered);
        }
    }

    /**
     * Updates the current host of the bottom bar.
     *
     * @param host The {@link Host} where the bottom bar is currently hosted.
     */
    public void setParent(@Host int host) {
        if (mHost == host) return;
        mHost = host;
        if (host == Host.TABBED) {
            updateColorSchemeFromThemeColorProvider();
        }
    }

    @Override
    public void onTintChanged(
            @Nullable ColorStateList tint,
            @Nullable ColorStateList activityFocusTint,
            @BrandedColorScheme int brandedColorScheme) {
        if (mHost != Host.TABBED) return;
        mModel.set(BottomBarProperties.COLOR_SCHEME, brandedColorScheme);
        mVisibilityDelegate.onBackgroundColorChanged();
    }

    @Override
    public void onPromoDialogAccepted() {
        @ActionId int eligibleAction = mModel.get(BottomBarProperties.EXTRA_BUTTON_ACTION_ID);

        PropertyModel actionModel;
        int stringResId;
        String featureTracker;

        if (eligibleAction == ActionId.AI_MODE) {
            actionModel = mAiModeActionSupplier.get();
            stringResId = R.string.iph_android_bottom_bar_aim;
            featureTracker = FeatureConstants.ANDROID_BOTTOM_BAR_AIM;
        } else {
            actionModel = mGlicActionSupplier.get();
            stringResId = R.string.iph_android_bottom_bar_glic;
            featureTracker = FeatureConstants.ANDROID_BOTTOM_BAR_GLIC;
        }

        if (actionModel == null) return;

        HighlightParams highlightParams = new HighlightParams(HighlightShape.RECTANGLE);
        highlightParams.setBoundsRespectPadding(true);
        int circleRadius =
                mContext.getResources()
                        .getDimensionPixelSize(R.dimen.bottom_bar_button_highlight_radius);
        highlightParams.setCornerRadius(circleRadius);

        @BottomBarMetrics.IphFeature
        String iphFeatureType =
                eligibleAction == ActionId.AI_MODE
                        ? BottomBarMetrics.IphFeature.AIM
                        : BottomBarMetrics.IphFeature.GLIC;

        IphIntent iphIntent =
                new IphIntent.Builder(featureTracker)
                        .setStringResId(stringResId)
                        .setAccessibilityResId(stringResId)
                        .setHighlightParams(highlightParams)
                        .setOnShowCallback(
                                () ->
                                        BottomBarMetrics.recordIphEvent(
                                                BottomBarMetrics.IphEvent.SHOWN, iphFeatureType))
                        .setOnDismissCallback(
                                () -> {
                                    BottomBarMetrics.recordIphEvent(
                                            BottomBarMetrics.IphEvent.DISMISSED, iphFeatureType);
                                    triggerNewTabIph();
                                })
                        .build();

        actionModel.set(ActionProperties.IPH_INTENT, iphIntent);
    }

    private void triggerNewTabIph() {
        PropertyModel newTabModel = mNewTabActionSupplier.get();
        if (newTabModel == null) return;

        // Only trigger the New Tab IPH once.
        if (mNewTabIphIntent != null) return;

        IphIntent.Builder newTabIphBuilder =
                new IphIntent.Builder(FeatureConstants.ANDROID_BOTTOM_BAR_NEW_TAB)
                        .setStringResId(R.string.iph_android_bottom_bar_new_tab)
                        .setAccessibilityResId(R.string.iph_android_bottom_bar_new_tab);

        HighlightParams newTabHighlightParams = new HighlightParams(HighlightShape.RECTANGLE);
        newTabHighlightParams.setBoundsRespectPadding(true);
        if (mButtonManager.hasCenteredButton()) {
            newTabHighlightParams.setCornerRadius(
                    mContext.getResources()
                            .getDimensionPixelSize(R.dimen.bottom_bar_new_tab_background_radius));
        } else {
            int circleRadius =
                    mContext.getResources()
                            .getDimensionPixelSize(R.dimen.bottom_bar_button_highlight_radius);
            newTabHighlightParams.setCornerRadius(circleRadius);
        }
        newTabIphBuilder.setHighlightParams(newTabHighlightParams);
        IphIntent newTabIph =
                newTabIphBuilder
                        .setOnShowCallback(
                                () ->
                                        BottomBarMetrics.recordIphEvent(
                                                BottomBarMetrics.IphEvent.SHOWN,
                                                BottomBarMetrics.IphFeature.NEW_TAB))
                        .setOnDismissCallback(
                                () ->
                                        BottomBarMetrics.recordIphEvent(
                                                BottomBarMetrics.IphEvent.DISMISSED,
                                                BottomBarMetrics.IphFeature.NEW_TAB))
                        .build();
        newTabModel.set(ActionProperties.IPH_INTENT, newTabIph);
        mNewTabIphIntent = newTabIph;
    }

    private void updateColorSchemeFromThemeColorProvider() {
        @BrandedColorScheme int brandedColorScheme = mThemeColorProvider.getBrandedColorScheme();
        mModel.set(BottomBarProperties.COLOR_SCHEME, brandedColorScheme);
        mVisibilityDelegate.onBackgroundColorChanged();
    }

    /*package*/ @Host
    int getHostForTesting() {
        return mHost;
    }

    @Override
    public void destroy() {
        mDestroyed = true;
        mThemeColorProvider.removeTintObserver(this);
        if (mCurrentTab != null) {
            mCurrentTab.removeObserver(mTabObserver);
            mCurrentTab = null;
        }
        mTabSupplier.removeObserver(mTabSupplierObserver);
        if (mShouldIncludeHomeButton) {
            mHomepageEnabledSupplier.removeObserver(mHomepageEnabledObserver);
        }
        if (mObservingSharedPrefs) {
            ContextUtils.getAppSharedPreferences().unregisterOnSharedPreferenceChangeListener(this);
            mObservingSharedPrefs = false;
        }
        mProfileSupplier.removeObserver(mProfileObserver);
        if (mGlicKeyedService != null) {
            mGlicKeyedService.removeAllowedChangedObserver(mAllowedChangedObserver);
            mGlicKeyedService = null;
        }
        if (mTemplateUrlService != null && mTemplateUrlServiceObserver != null) {
            mTemplateUrlService.removeObserver(mTemplateUrlServiceObserver);
            mTemplateUrlService = null;
            mTemplateUrlServiceObserver = null;
        }

        mOmniboxFocusStateSupplier.removeObserver(mOmniboxFocusObserver);

        if (mLayoutStateProvider != null) {
            mLayoutStateProvider.removeObserver(this);
            mLayoutStateProvider = null;
        }

        PropertyModel glicModel = mGlicActionSupplier.get();
        if (glicModel != null) {
            glicModel.set(ActionProperties.IPH_INTENT, null);
        }
        PropertyModel aiModeModel = mAiModeActionSupplier.get();
        if (aiModeModel != null) {
            aiModeModel.set(ActionProperties.IPH_INTENT, null);
        }
        PropertyModel newTabModel = mNewTabActionSupplier.get();
        if (newTabModel != null) {
            newTabModel.set(ActionProperties.IPH_INTENT, null);
        }
        mNewTabIphIntent = null;
    }
}
