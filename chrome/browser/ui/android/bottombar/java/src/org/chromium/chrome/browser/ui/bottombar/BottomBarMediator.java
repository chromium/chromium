// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.ui.bottombar;

import android.content.Context;
import android.content.res.ColorStateList;
import android.os.SystemClock;

import org.chromium.base.Callback;
import org.chromium.base.lifetime.Destroyable;
import org.chromium.base.supplier.NonNullObservableSupplier;
import org.chromium.base.supplier.NullableObservableSupplier;
import org.chromium.build.annotations.NullMarked;
import org.chromium.build.annotations.Nullable;
import org.chromium.chrome.browser.feature_engagement.TrackerFactory;
import org.chromium.chrome.browser.glic.GlicEnabling;
import org.chromium.chrome.browser.glic.GlicKeyedService;
import org.chromium.chrome.browser.glic.GlicKeyedServiceFactory;
import org.chromium.chrome.browser.profiles.Profile;
import org.chromium.chrome.browser.tab.EmptyTabObserver;
import org.chromium.chrome.browser.tab.Tab;
import org.chromium.chrome.browser.tab.TabObserver;
import org.chromium.chrome.browser.theme.ThemeColorProvider;
import org.chromium.chrome.browser.ui.actions.ActionId;
import org.chromium.chrome.browser.ui.actions.ActionProperties;
import org.chromium.chrome.browser.ui.actions.ActionRegistry;
import org.chromium.chrome.browser.ui.android.bars_common.IphIntent;
import org.chromium.chrome.browser.ui.theme.BrandedColorScheme;
import org.chromium.components.browser_ui.widget.highlight.ViewHighlighter.HighlightParams;
import org.chromium.components.browser_ui.widget.highlight.ViewHighlighter.HighlightShape;
import org.chromium.components.embedder_support.util.UrlUtilities;
import org.chromium.components.feature_engagement.FeatureConstants;
import org.chromium.components.feature_engagement.Tracker;
import org.chromium.ui.modelutil.PropertyModel;

/** Mediator for the bottom bar */
@NullMarked
public class BottomBarMediator
        implements ThemeColorProvider.TintObserver,
                BottomBarButtonManager.Listener,
                BottomBarPromoDialogCoordinator.BottomBarPromoDialogListener,
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
    private final NullableObservableSupplier<PropertyModel> mGlicActionSupplier;
    private final NullableObservableSupplier<PropertyModel> mNewTabActionSupplier;
    private final boolean mShouldIncludeHomeButton;

    // Observers and Callbacks
    private final TabObserver mTabObserver;
    private final Callback<@Nullable Tab> mTabSupplierObserver = this::onTabChanged;
    private final Callback<Boolean> mHomepageEnabledObserver = this::onHomepageEnabledChanged;
    private final Callback<Boolean> mOmniboxFocusObserver = this::onOmniboxFocusChanged;
    private final Callback<@Nullable Profile> mProfileObserver = this::updateGlicVisibility;
    private final GlicKeyedService.AllowedChangedObserver mAllowedChangedObserver =
            this::onGlicAllowedChanged;

    // Mutable State (Nullable)
    private @Nullable GlicKeyedService mGlicKeyedService;
    private @Nullable Profile mOriginalProfile;
    private @Nullable Tab mCurrentTab;
    private @Nullable Boolean mIsVisible;
    private @Nullable IphIntent mNewTabIphIntent;

    // Mutable State (Primitive / Non-null)
    private boolean mGlicWasVisible;
    private boolean mGlicTimeToAppearRecorded;
    private long mBottomBarShownTimeMs = -1;
    private long mGlicAppearedTimeMs = -1;
    private boolean mStartupPromoFlowFinished;

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
            NonNullObservableSupplier<Boolean> omniboxFocusStateSupplier,
            BottomBarPromoDialogCoordinator promoDialogCoordinator,
            ActionRegistry actionRegistry) {
        mContext = context;
        mModel = model;
        mButtonManager = buttonManager;
        mThemeColorProvider = themeColorProvider;
        mTabSupplier = tabSupplier;
        mHomepageEnabledSupplier = homepageEnabledSupplier;
        mVisibilityDelegate = visibilityDelegate;
        mShouldIncludeHomeButton = shouldIncludeHomeButton;
        mProfileSupplier = profileSupplier;
        mOmniboxFocusStateSupplier = omniboxFocusStateSupplier;
        mPromoDialogCoordinator = promoDialogCoordinator;
        mGlicActionSupplier = actionRegistry.get(ActionId.GLIC);
        mNewTabActionSupplier = actionRegistry.get(ActionId.NEW_TAB);
        mGlicTimeToAppearRecorded = false;

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

    private void onOmniboxFocusChanged(Boolean focused) {
        updateVisibility();
    }

    private void updateVisibility() {
        boolean currentTabIsRegularNtp =
                mCurrentTab != null
                        && UrlUtilities.isNtpUrl(mCurrentTab.getUrl())
                        && !mCurrentTab.isOffTheRecord();
        boolean isOmniboxFocused = mOmniboxFocusStateSupplier.get();
        boolean shouldDisableOnNtp =
                BottomBarConfigUtils.shouldDisableOnNtp() && currentTabIsRegularNtp;
        boolean isVisible = !shouldDisableOnNtp && !isOmniboxFocused;

        if (mIsVisible != null && mIsVisible == isVisible) return;

        boolean didBecomeVisible = isVisible && (mIsVisible == null || !mIsVisible);
        mIsVisible = isVisible;

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
        boolean isBottomBarVisible = Boolean.TRUE.equals(mIsVisible);
        boolean isGlicVisible =
                Boolean.TRUE.equals(mModel.get(BottomBarProperties.IS_GLIC_BUTTON_VISIBLE));
        if (isBottomBarVisible && isGlicVisible) {
            Profile profile = mProfileSupplier.get();
            Tracker tracker = profile == null ? null : TrackerFactory.getTrackerForProfile(profile);
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

    private void updateGlicVisibility(@Nullable Profile profile) {
        Profile originalProfile = profile != null ? profile.getOriginalProfile() : null;

        // Manage observers for dynamic updates.
        updateObservers(originalProfile);

        if (profile == null) {
            setButtonVisibility(ActionId.GLIC, false);
            return;
        }

        // Calculate and set visibility.
        long startTime = SystemClock.uptimeMillis();
        boolean shouldBeVisible = GlicEnabling.isEnabledForProfile(originalProfile);
        long decisionDuration = SystemClock.uptimeMillis() - startTime;

        BottomBarMetrics.recordGlicVisibilityDecisionTime(decisionDuration);

        if (shouldBeVisible && !mGlicWasVisible) {
            mGlicAppearedTimeMs = SystemClock.uptimeMillis();
            if (mBottomBarShownTimeMs != -1 && !mGlicTimeToAppearRecorded) {
                long timeSinceShown = mGlicAppearedTimeMs - mBottomBarShownTimeMs;
                BottomBarMetrics.recordGlicTimeToAppear(timeSinceShown);
                mGlicTimeToAppearRecorded = true;
            }
        }
        mGlicWasVisible = shouldBeVisible;

        setButtonVisibility(ActionId.GLIC, shouldBeVisible);
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

        if (originalProfile == null) return;

        GlicKeyedService glicKeyedService = GlicKeyedServiceFactory.getForProfile(originalProfile);
        mGlicKeyedService = glicKeyedService;
        if (mGlicKeyedService != null) {
            mGlicKeyedService.addAllowedChangedObserver(mAllowedChangedObserver);
        }
    }

    private void onGlicAllowedChanged() {
        updateGlicVisibility(mProfileSupplier.get());
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
        Boolean current = mModel.get(BottomBarProperties.IS_NEW_TAB_BACKGROUND_VISIBLE);
        if (current == null || current != isCentered) {
            mModel.set(BottomBarProperties.IS_NEW_TAB_BACKGROUND_VISIBLE, isCentered);
        }
    }

    @Override
    public void onTintChanged(
            @Nullable ColorStateList tint,
            @Nullable ColorStateList activityFocusTint,
            @BrandedColorScheme int brandedColorScheme) {
        mModel.set(BottomBarProperties.COLOR_SCHEME, brandedColorScheme);
        mVisibilityDelegate.onBackgroundColorChanged();
    }

    @Override
    public void onPromoDialogAccepted() {
        PropertyModel glicModel = mGlicActionSupplier.get();
        if (glicModel == null) return;

        HighlightParams glicHighlightParams = new HighlightParams(HighlightShape.RECTANGLE);
        glicHighlightParams.setBoundsRespectPadding(true);
        int circleRadius =
                mContext.getResources()
                        .getDimensionPixelSize(R.dimen.bottom_bar_button_highlight_radius);
        glicHighlightParams.setCornerRadius(circleRadius);

        IphIntent glicIph =
                new IphIntent.Builder(FeatureConstants.ANDROID_BOTTOM_BAR_GLIC)
                        .setStringResId(R.string.iph_android_bottom_bar_glic)
                        .setAccessibilityResId(R.string.iph_android_bottom_bar_glic)
                        .setHighlightParams(glicHighlightParams)
                        .setOnShowCallback(
                                () ->
                                        BottomBarMetrics.recordIphEvent(
                                                BottomBarMetrics.IphEvent.SHOWN,
                                                /* isNewTabIph= */ false))
                        .setOnDismissCallback(
                                () -> {
                                    BottomBarMetrics.recordIphEvent(
                                            BottomBarMetrics.IphEvent.DISMISSED,
                                            /* isNewTabIph= */ false);
                                    triggerNewTabIph();
                                })
                        .build();

        glicModel.set(ActionProperties.IPH_INTENT, glicIph);
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
                                                /* isNewTabIph= */ true))
                        .setOnDismissCallback(
                                () ->
                                        BottomBarMetrics.recordIphEvent(
                                                BottomBarMetrics.IphEvent.DISMISSED,
                                                /* isNewTabIph= */ true))
                        .build();
        newTabModel.set(ActionProperties.IPH_INTENT, newTabIph);
        mNewTabIphIntent = newTabIph;
    }

    @Override
    public void destroy() {
        mThemeColorProvider.removeTintObserver(this);
        if (mCurrentTab != null) {
            mCurrentTab.removeObserver(mTabObserver);
            mCurrentTab = null;
        }
        mTabSupplier.removeObserver(mTabSupplierObserver);
        if (mShouldIncludeHomeButton) {
            mHomepageEnabledSupplier.removeObserver(mHomepageEnabledObserver);
        }
        mProfileSupplier.removeObserver(mProfileObserver);
        if (mGlicKeyedService != null) {
            mGlicKeyedService.removeAllowedChangedObserver(mAllowedChangedObserver);
            mGlicKeyedService = null;
        }

        mOmniboxFocusStateSupplier.removeObserver(mOmniboxFocusObserver);

        PropertyModel glicModel = mGlicActionSupplier.get();
        if (glicModel != null) {
            glicModel.set(ActionProperties.IPH_INTENT, null);
        }
        PropertyModel newTabModel = mNewTabActionSupplier.get();
        if (newTabModel != null) {
            newTabModel.set(ActionProperties.IPH_INTENT, null);
        }
        mNewTabIphIntent = null;
    }
}
