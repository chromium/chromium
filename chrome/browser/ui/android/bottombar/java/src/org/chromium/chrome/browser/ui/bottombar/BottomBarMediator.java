// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.ui.bottombar;

import android.content.Context;
import android.content.SharedPreferences;
import android.content.res.ColorStateList;
import android.os.SystemClock;

import androidx.annotation.Px;

import org.chromium.base.Callback;
import org.chromium.base.ContextUtils;
import org.chromium.base.TriState;
import org.chromium.base.TriStateUtils;
import org.chromium.base.lifetime.Destroyable;
import org.chromium.base.supplier.NonNullObservableSupplier;
import org.chromium.base.supplier.NullableObservableSupplier;
import org.chromium.build.annotations.NullMarked;
import org.chromium.build.annotations.Nullable;
import org.chromium.chrome.browser.browser_controls.BrowserControlsStateProvider.ControlsPosition;
import org.chromium.chrome.browser.glic.GlicEnabling;
import org.chromium.chrome.browser.glic.GlicKeyedService;
import org.chromium.chrome.browser.glic.GlicKeyedServiceFactory;
import org.chromium.chrome.browser.layouts.LayoutStateProvider;
import org.chromium.chrome.browser.layouts.LayoutStateProvider.LayoutStateObserver;
import org.chromium.chrome.browser.layouts.LayoutType;
import org.chromium.chrome.browser.preferences.ChromePreferenceKeys;
import org.chromium.chrome.browser.profiles.Profile;
import org.chromium.chrome.browser.tab.Tab;
import org.chromium.chrome.browser.theme.ThemeColorProvider;
import org.chromium.chrome.browser.ui.actions.ActionId;
import org.chromium.chrome.browser.ui.bottombar.BottomBarHostManager.Host;
import org.chromium.chrome.browser.ui.bottombar.BottomBarMetrics.CandidateAction;
import org.chromium.chrome.browser.ui.theme.BrandedColorScheme;
import org.chromium.ui.modelutil.PropertyModel;

/** Mediator for the bottom bar */
@NullMarked
public class BottomBarMediator
        implements ThemeColorProvider.TintObserver,
                BottomBarButtonManager.Listener,
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

        /**
         * Called when the height of the bottom bar changes.
         *
         * @param height The new height of the bottom bar in pixels.
         */
        void onHeightChanged(@Px int height);
    }

    // Dependencies
    private final Context mContext;
    private final PropertyModel mModel;
    private final BottomBarButtonManager mButtonManager;
    private final ThemeColorProvider mThemeColorProvider;
    private final VisibilityDelegate mVisibilityDelegate;
    private final NonNullObservableSupplier<Boolean> mHomepageEnabledSupplier;
    private final NonNullObservableSupplier<Boolean> mOmniboxFocusStateSupplier;
    private final NonNullObservableSupplier<@ControlsPosition Integer> mToolbarPositionSupplier;
    private final NullableObservableSupplier<Profile> mProfileSupplier;
    private final @Nullable BottomBarEducationCoordinator mEducationCoordinator;
    private final NullableObservableSupplier<Tab> mTabSupplier;

    // Observers and Callbacks
    private final Callback<Boolean> mHomepageEnabledObserver = this::onHomepageEnabledChanged;
    private final Callback<Boolean> mOmniboxFocusObserver = this::onOmniboxFocusChanged;
    private final Callback<@ControlsPosition Integer> mToolbarPositionObserver =
            this::onToolbarPositionChanged;
    private final Callback<@Nullable Profile> mProfileObserver = this::onProfileChanged;
    private final GlicKeyedService.AllowedChangedObserver mAllowedChangedObserver =
            this::onGlicAllowedChanged;

    // Mutable State (Nullable)
    private @Nullable GlicKeyedService mGlicKeyedService;
    private @Nullable Profile mOriginalProfile;
    private @TriState int mIsVisible;
    private @Nullable LayoutStateProvider mLayoutStateProvider;
    private @Nullable @ActionId Integer mResolvedCandidateExtraAction;

    // Mutable State (Primitive / Non-null)
    private boolean mEducationClaimedPromoSlot;
    private boolean mCandidateMetricsRecorded;
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
     * @param homepageEnabledSupplier Supplier of whether the homepage is enabled.
     * @param visibilityDelegate Delegate to handle compositor-level visibility changes.
     * @param profileSupplier Supplier of the current profile.
     * @param omniboxFocusStateSupplier Supplier of the omnibox focus state.
     * @param layoutStateProvider The {@link LayoutStateProvider} to observe layout state changes.
     * @param toolbarPositionSupplier Supplier of the current toolbar position.
     * @param educationCoordinator The {@link BottomBarEducationCoordinator} for the education
     *     bubble.
     * @param tabSupplier Supplier of the current tab.
     */
    public BottomBarMediator(
            Context context,
            PropertyModel model,
            BottomBarButtonManager buttonManager,
            ThemeColorProvider themeColorProvider,
            NonNullObservableSupplier<Boolean> homepageEnabledSupplier,
            VisibilityDelegate visibilityDelegate,
            NullableObservableSupplier<Profile> profileSupplier,
            NonNullObservableSupplier<Boolean> omniboxFocusStateSupplier,
            LayoutStateProvider layoutStateProvider,
            NonNullObservableSupplier<@ControlsPosition Integer> toolbarPositionSupplier,
            @Nullable BottomBarEducationCoordinator educationCoordinator,
            NullableObservableSupplier<Tab> tabSupplier) {
        mContext = context;
        mModel = model;
        mButtonManager = buttonManager;
        mThemeColorProvider = themeColorProvider;
        mHomepageEnabledSupplier = homepageEnabledSupplier;
        mVisibilityDelegate = visibilityDelegate;
        mProfileSupplier = profileSupplier;
        mOmniboxFocusStateSupplier = omniboxFocusStateSupplier;
        mToolbarPositionSupplier = toolbarPositionSupplier;
        mEducationCoordinator = educationCoordinator;
        mTabSupplier = tabSupplier;
        mGlicTimeToAppearRecorded = false;

        if (!BottomBarConfigUtils.shouldShowOnGts()) {
            layoutStateProvider.addObserver(this);
            mShouldHideForHub = layoutStateProvider.isLayoutVisible(LayoutType.HUB);
            mLayoutStateProvider = layoutStateProvider;
        }

        mThemeColorProvider.addTintObserver(this);
        mModel.set(BottomBarProperties.COLOR_SCHEME, mThemeColorProvider.getBrandedColorScheme());
        mProfileSupplier.addSyncObserver(mProfileObserver);
        updateExtraActionVisibility();
        mOmniboxFocusStateSupplier.addSyncObserver(mOmniboxFocusObserver);
        updateVisibility();
        mToolbarPositionSupplier.addSyncObserverAndCallIfNonNull(mToolbarPositionObserver);
        mHomepageEnabledSupplier.addSyncObserverAndCallIfNonNull(mHomepageEnabledObserver);

        // Safe to set the listener after all observers are initialized to trigger the immediate
        // callback with the correct state.
        mButtonManager.setListener(this);
    }

    private void onOmniboxFocusChanged(boolean focused) {
        updateVisibility();
    }

    private void onToolbarPositionChanged(@ControlsPosition int position) {
        updateTopPadding();
    }

    private void updateTopPadding() {
        boolean hasBottomOmnibox = mToolbarPositionSupplier.get() == ControlsPosition.BOTTOM;
        boolean removeTopPadding =
                mHost == Host.TABBED
                        && hasBottomOmnibox
                        && BottomBarConfigUtils.shouldRemoveTopPaddingForBottomOmnibox();
        mModel.set(BottomBarProperties.IS_TOP_PADDING_REMOVED, removeTopPadding);
        // The delegate only sizes the tabbed host; the Hub sizes its own container. Keying the
        // height off the omnibox position rather than the host avoids resizing the bottom controls
        // when entering or leaving the Hub.
        mVisibilityDelegate.onHeightChanged(
                BottomBarUtils.getBottomBarHeight(mContext, hasBottomOmnibox));
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
        boolean isOmniboxFocused = mOmniboxFocusStateSupplier.get();
        boolean isVisible = !isOmniboxFocused && !mShouldHideForHub;

        if (mIsVisible == TriStateUtils.from(isVisible)) return;

        boolean didBecomeVisible = isVisible && mIsVisible != TriState.TRUE;
        mIsVisible = TriStateUtils.from(isVisible);

        mModel.set(BottomBarProperties.IS_VISIBLE, isVisible);
        mVisibilityDelegate.onVisibilityChanged(isVisible);

        if (!isVisible && mEducationCoordinator != null) {
            mEducationCoordinator.onBottomBarHidden();
        }

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
     * <p>This marks the startup promo flow as finished, unlocking the ability to show the education
     * onboarding. If no startup promo was shown (or if education claimed the slot), it attempts to
     * show it immediately. If another promo was shown, it defers showing education until a
     * subsequent visibility change to avoid double-promotions.
     *
     * @param promoShown True if any startup promo was shown during the startup flow.
     */
    public void onStartupPromoFlowFinished(boolean promoShown) {
        mStartupPromoFlowFinished = true;
        if (!promoShown || mEducationClaimedPromoSlot) {
            maybeShowIphs();
        }
    }

    /**
     * Checks if the education flow should be shown and claims the startup promo slot if so.
     *
     * @param profile The current Profile.
     * @return Whether the education promo slot was successfully claimed.
     */
    public boolean maybeClaimPromoSlotForEducation(Profile profile) {
        if (mEducationCoordinator != null && mEducationCoordinator.wouldShow(profile)) {
            mEducationClaimedPromoSlot = true;
            return true;
        }
        return false;
    }

    private void maybeShowIphs() {
        if (!mStartupPromoFlowFinished) return;
        boolean isBottomBarVisible = mIsVisible == TriState.TRUE;

        if (mEducationCoordinator != null
                && (mEducationClaimedPromoSlot
                        || mEducationCoordinator.wouldShowTooltips(mProfileSupplier.get()))) {
            Tab currentTab = mTabSupplier.get();
            if (isBottomBarVisible && currentTab != null && !currentTab.isOffTheRecord()) {
                mEducationCoordinator.maybeShow();
            }
        }
    }

    private void onProfileChanged(@Nullable Profile profile) {
        updateExtraActionVisibility();
    }

    private void updateExtraActionVisibility() {
        if (mDestroyed) return;
        Profile profile = mProfileSupplier.get();
        if (profile == null) {
            mResolvedCandidateExtraAction = null;
            updateObservers(/* originalProfile= */ null);
            setButtonVisibility(ActionId.GLIC, /* visible= */ false);
            return;
        }

        Profile originalProfile = profile.getOriginalProfile();

        if (mOriginalProfile != originalProfile) {
            mResolvedCandidateExtraAction = null;
            mCandidateMetricsRecorded = false;
            updateObservers(originalProfile);
        }

        if (mResolvedCandidateExtraAction == null) {
            long startTime = SystemClock.uptimeMillis();
            BottomBarActionEligibility.getCandidateExtraAction(originalProfile);
            Integer candidateExtraAction =
                    BottomBarActionEligibility.getCachedCandidateExtraAction();
            mResolvedCandidateExtraAction = candidateExtraAction;
            long decisionDuration = SystemClock.uptimeMillis() - startTime;
            if (!mCandidateMetricsRecorded) {
                BottomBarMetrics.recordCandidateDecisionTime(decisionDuration);
                BottomBarActionEligibility.recordGlicIneligibilityReasonIfNeeded(originalProfile);

                @CandidateAction
                int candidateMetric =
                        candidateExtraAction != null && candidateExtraAction == ActionId.GLIC
                                ? CandidateAction.GLIC
                                : CandidateAction.NONE;
                BottomBarMetrics.recordCandidateExtraAction(candidateMetric);
                mCandidateMetricsRecorded = true;
            }
        }

        updateObservers(originalProfile);

        Integer candidateExtraAction = mResolvedCandidateExtraAction;
        if (candidateExtraAction != null && candidateExtraAction == ActionId.GLIC) {
            updateGlicVisibility(originalProfile);
        } else {
            setButtonVisibility(ActionId.GLIC, /* visible= */ false);
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

        boolean visible =
                GlicEnabling.isEnabledForProfile(originalProfile)
                        && (GlicEnabling.isPolicyEnforced(originalProfile)
                                || BottomBarConfigUtils.isGlicButtonEnabled());

        if (visible && !mGlicWasVisible) {
            mGlicAppearedTimeMs = SystemClock.uptimeMillis();
            if (mBottomBarShownTimeMs != -1 && !mGlicTimeToAppearRecorded) {
                long timeSinceShown = mGlicAppearedTimeMs - mBottomBarShownTimeMs;
                BottomBarMetrics.recordGlicTimeToAppear(timeSinceShown);
                mGlicTimeToAppearRecorded = true;
            }
        }
        mGlicWasVisible = visible;

        setButtonVisibility(ActionId.GLIC, visible);
    }

    private void updateObservers(@Nullable Profile originalProfile) {
        if (mOriginalProfile != originalProfile) {
            if (mGlicKeyedService != null) {
                mGlicKeyedService.removeAllowedChangedObserver(mAllowedChangedObserver);
                mGlicKeyedService = null;
            }
            if (mObservingSharedPrefs) {
                ContextUtils.getAppSharedPreferences()
                        .unregisterOnSharedPreferenceChangeListener(this);
                mObservingSharedPrefs = false;
            }
            mOriginalProfile = originalProfile;
        }

        if (mOriginalProfile == null) {
            return;
        }

        Integer candidateExtraAction = mResolvedCandidateExtraAction;
        boolean isGlicCandidate =
                candidateExtraAction != null && candidateExtraAction == ActionId.GLIC;
        boolean shouldObserveGlicAllowed = isGlicCandidate;
        if (!shouldObserveGlicAllowed && candidateExtraAction != null) {
            shouldObserveGlicAllowed = !GlicEnabling.isEnabledForProfile(mOriginalProfile);
        }
        if (shouldObserveGlicAllowed) {
            if (mGlicKeyedService == null) {
                mGlicKeyedService = GlicKeyedServiceFactory.getForProfile(mOriginalProfile);
                if (mGlicKeyedService != null) {
                    mGlicKeyedService.addAllowedChangedObserver(mAllowedChangedObserver);
                }
            }
        } else if (mGlicKeyedService != null) {
            mGlicKeyedService.removeAllowedChangedObserver(mAllowedChangedObserver);
            mGlicKeyedService = null;
        }

        if (isGlicCandidate) {
            if (!mObservingSharedPrefs) {
                ContextUtils.getAppSharedPreferences()
                        .registerOnSharedPreferenceChangeListener(this);
                mObservingSharedPrefs = true;
            }
        } else if (mObservingSharedPrefs) {
            ContextUtils.getAppSharedPreferences().unregisterOnSharedPreferenceChangeListener(this);
            mObservingSharedPrefs = false;
        }
    }

    private void onGlicAllowedChanged() {
        if (mOriginalProfile == null) {
            return;
        }
        if (mResolvedCandidateExtraAction == null
                || mResolvedCandidateExtraAction != ActionId.GLIC) {
            if (GlicEnabling.isEnabledForProfile(mOriginalProfile)) {
                // Reset the candidate to null to force a re-evaluation to see if GLIC should now
                // be promoted as the extra action, now that it is enabled for the profile.
                mResolvedCandidateExtraAction = null;
                updateExtraActionVisibility();
            }
            return;
        }
        updateGlicVisibility(mOriginalProfile);
    }

    @Override
    public void onSharedPreferenceChanged(
            SharedPreferences sharedPreferences, @Nullable String key) {
        if (ChromePreferenceKeys.BOTTOM_BAR_GLIC_BUTTON_ENABLED.equals(key)
                && mOriginalProfile != null) {
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
        if (mDestroyed) return;
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
        updateTopPadding();
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
        mHomepageEnabledSupplier.removeObserver(mHomepageEnabledObserver);
        if (mObservingSharedPrefs) {
            ContextUtils.getAppSharedPreferences().unregisterOnSharedPreferenceChangeListener(this);
            mObservingSharedPrefs = false;
        }
        mProfileSupplier.removeObserver(mProfileObserver);
        if (mGlicKeyedService != null) {
            mGlicKeyedService.removeAllowedChangedObserver(mAllowedChangedObserver);
            mGlicKeyedService = null;
        }
        mOriginalProfile = null;

        mOmniboxFocusStateSupplier.removeObserver(mOmniboxFocusObserver);
        mToolbarPositionSupplier.removeObserver(mToolbarPositionObserver);

        if (mLayoutStateProvider != null) {
            mLayoutStateProvider.removeObserver(this);
            mLayoutStateProvider = null;
        }
    }
}
