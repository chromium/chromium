// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.ui.bottombar;

import android.content.res.ColorStateList;
import android.os.SystemClock;

import org.chromium.base.Callback;
import org.chromium.base.lifetime.Destroyable;
import org.chromium.base.metrics.RecordHistogram;
import org.chromium.base.supplier.NonNullObservableSupplier;
import org.chromium.base.supplier.NullableObservableSupplier;
import org.chromium.build.annotations.NullMarked;
import org.chromium.build.annotations.Nullable;
import org.chromium.chrome.browser.glic.GlicEnabling;
import org.chromium.chrome.browser.profiles.Profile;
import org.chromium.chrome.browser.signin.services.IdentityServicesProvider;
import org.chromium.chrome.browser.sync.SyncServiceFactory;
import org.chromium.chrome.browser.tab.EmptyTabObserver;
import org.chromium.chrome.browser.tab.Tab;
import org.chromium.chrome.browser.tab.TabObserver;
import org.chromium.chrome.browser.theme.ThemeColorProvider;
import org.chromium.chrome.browser.ui.actions.ActionId;
import org.chromium.chrome.browser.ui.theme.BrandedColorScheme;
import org.chromium.components.embedder_support.util.UrlUtilities;
import org.chromium.components.signin.identitymanager.IdentityManager;
import org.chromium.components.signin.identitymanager.PrimaryAccountChangeEvent;
import org.chromium.components.sync.SyncService;
import org.chromium.google_apis.gaia.GoogleServiceAuthErrorState;
import org.chromium.ui.modelutil.PropertyModel;

/** Mediator for the bottom bar */
@NullMarked
public class BottomBarMediator implements ThemeColorProvider.TintObserver, Destroyable {
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

    private static final String GLIC_VISIBILITY_DECISION_TIME_HISTOGRAM =
            "Android.BottomBar.GlicVisibilityDecisionTime";
    private static final String GLIC_TIME_TO_APPEAR_HISTOGRAM =
            "Android.BottomBar.GlicTimeToAppearSinceBottomBarShown";

    private final PropertyModel mModel;
    private final BottomBarButtonManager mButtonManager;
    private final ThemeColorProvider mThemeColorProvider;
    private final NullableObservableSupplier<Tab> mTabSupplier;
    private final TabObserver mTabObserver;
    private final VisibilityDelegate mVisibilityDelegate;
    private final NonNullObservableSupplier<Boolean> mHomepageEnabledSupplier;
    private final NonNullObservableSupplier<Boolean> mOmniboxFocusStateSupplier;
    private final Callback<@Nullable Tab> mTabSupplierObserver = this::onTabChanged;
    private final Callback<Boolean> mHomepageEnabledObserver = this::onHomepageEnabledChanged;
    private final Callback<Boolean> mOmniboxFocusObserver = this::onOmniboxFocusChanged;
    private final boolean mShouldIncludeHomeButton;
    private final NullableObservableSupplier<Profile> mProfileSupplier;
    private final Callback<@Nullable Profile> mProfileObserver = this::updateGlicVisibility;
    private final IdentityManager.Observer mIdentityManagerObserver =
            new IdentityManager.Observer() {
                @Override
                public void onPrimaryAccountChanged(PrimaryAccountChangeEvent eventDetails) {
                    updateGlicVisibility(mProfileSupplier.get());
                }
            };
    private @Nullable IdentityManager mIdentityManager;

    private final SyncService.SyncStateChangedListener mSyncStateListener =
            new SyncService.SyncStateChangedListener() {
                @Override
                public void syncStateChanged() {
                    updateGlicVisibility(mProfileSupplier.get());
                }
            };
    private @Nullable SyncService mSyncService;
    private @Nullable Profile mOriginalProfile;

    private @Nullable Tab mCurrentTab;
    private @Nullable Boolean mIsVisible;
    private boolean mGlicWasVisible;
    private boolean mGlicTimeToAppearRecorded;
    private long mBottomBarShownTimeMs = -1;
    private long mGlicAppearedTimeMs = -1;

    /**
     * @param model The property model to update.
     * @param themeColorProvider The provider to observe theme changes from.
     * @param tabSupplier Supplier of the current tab.
     * @param visibilityDelegate Delegate to handle compositor-level visibility changes.
     */
    public BottomBarMediator(
            PropertyModel model,
            BottomBarButtonManager buttonManager,
            ThemeColorProvider themeColorProvider,
            NullableObservableSupplier<Tab> tabSupplier,
            NonNullObservableSupplier<Boolean> homepageEnabledSupplier,
            VisibilityDelegate visibilityDelegate,
            boolean shouldIncludeHomeButton,
            NullableObservableSupplier<Profile> profileSupplier,
            NonNullObservableSupplier<Boolean> omniboxFocusStateSupplier) {
        mModel = model;
        mButtonManager = buttonManager;
        mThemeColorProvider = themeColorProvider;
        mTabSupplier = tabSupplier;
        mHomepageEnabledSupplier = homepageEnabledSupplier;
        mVisibilityDelegate = visibilityDelegate;
        mShouldIncludeHomeButton = shouldIncludeHomeButton;
        mProfileSupplier = profileSupplier;
        mOmniboxFocusStateSupplier = omniboxFocusStateSupplier;
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
        mButtonManager.setListener(this::onButtonChanged);
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

        if (isVisible && (mIsVisible == null || !mIsVisible)) {
            mBottomBarShownTimeMs = SystemClock.uptimeMillis();
            if (mGlicAppearedTimeMs != -1 && !mGlicTimeToAppearRecorded) {
                RecordHistogram.recordLongTimesHistogram(GLIC_TIME_TO_APPEAR_HISTOGRAM, 0);
                mGlicTimeToAppearRecorded = true;
            }
        }

        mIsVisible = isVisible;

        mModel.set(BottomBarProperties.IS_VISIBLE, isVisible);
        mVisibilityDelegate.onVisibilityChanged(isVisible);
    }

    private void updateGlicVisibility(@Nullable Profile profile) {
        if (profile == null) {
            setButtonVisibility(ActionId.GLIC, false);
            return;
        }

        Profile originalProfile = profile.getOriginalProfile();

        // Manage observers for dynamic updates.
        updateObservers(originalProfile);

        // Calculate and set visibility.
        long startTime = SystemClock.uptimeMillis();
        boolean shouldBeVisible = shouldShowGlicButton(originalProfile);
        long decisionDuration = SystemClock.uptimeMillis() - startTime;

        RecordHistogram.recordTimesHistogram(
                GLIC_VISIBILITY_DECISION_TIME_HISTOGRAM, decisionDuration);

        if (shouldBeVisible && !mGlicWasVisible) {
            mGlicAppearedTimeMs = SystemClock.uptimeMillis();
            if (mBottomBarShownTimeMs != -1 && !mGlicTimeToAppearRecorded) {
                long timeSinceShown = mGlicAppearedTimeMs - mBottomBarShownTimeMs;
                RecordHistogram.recordLongTimesHistogram(
                        GLIC_TIME_TO_APPEAR_HISTOGRAM, timeSinceShown);
                mGlicTimeToAppearRecorded = true;
            }
        }
        mGlicWasVisible = shouldBeVisible;

        setButtonVisibility(ActionId.GLIC, shouldBeVisible);
    }

    private void updateObservers(Profile originalProfile) {
        if (mOriginalProfile == originalProfile) {
            return;
        }
        mOriginalProfile = originalProfile;

        IdentityManager identityManager =
                IdentityServicesProvider.get().getIdentityManager(originalProfile);
        if (mIdentityManager != identityManager) {
            if (mIdentityManager != null) {
                mIdentityManager.removeObserver(mIdentityManagerObserver);
            }
            mIdentityManager = identityManager;
            if (mIdentityManager != null) {
                mIdentityManager.addObserver(mIdentityManagerObserver);
            }
        }

        SyncService syncService = SyncServiceFactory.getForProfile(originalProfile);
        if (mSyncService != syncService) {
            if (mSyncService != null) {
                mSyncService.removeSyncStateChangedListener(mSyncStateListener);
            }
            mSyncService = syncService;
            if (mSyncService != null) {
                mSyncService.addSyncStateChangedListener(mSyncStateListener);
            }
        }
    }

    private boolean shouldShowGlicButton(Profile originalProfile) {
        if (!GlicEnabling.isEnabledForProfile(originalProfile)) {
            return false;
        }

        if (mIdentityManager == null || !mIdentityManager.hasPrimaryAccount()) {
            return false;
        }

        // Only show the button if there are no authentication errors (e.g. account paused).
        if (mSyncService == null) {
            return true;
        }
        return mSyncService.getAuthError().getState() == GoogleServiceAuthErrorState.NONE;
    }

    private void onHomepageEnabledChanged(boolean isEnabled) {
        setButtonVisibility(ActionId.HOME_BUTTON, isEnabled);
    }

    private void setButtonVisibility(int actionId, boolean visible) {
        mButtonManager.setButtonVisibility(actionId, visible);
    }

    private void onButtonChanged(boolean visibilityChanged) {
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
        if (mIdentityManager != null) {
            mIdentityManager.removeObserver(mIdentityManagerObserver);
            mIdentityManager = null;
        }
        if (mSyncService != null) {
            mSyncService.removeSyncStateChangedListener(mSyncStateListener);
            mSyncService = null;
        }
        mOmniboxFocusStateSupplier.removeObserver(mOmniboxFocusObserver);
    }
}
