// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.ui.bottombar;

import android.content.res.ColorStateList;

import org.chromium.base.Callback;
import org.chromium.base.lifetime.Destroyable;
import org.chromium.base.supplier.NonNullObservableSupplier;
import org.chromium.base.supplier.NullableObservableSupplier;
import org.chromium.build.annotations.NullMarked;
import org.chromium.build.annotations.Nullable;
import org.chromium.chrome.browser.glic.GlicEnabling;
import org.chromium.chrome.browser.profiles.Profile;
import org.chromium.chrome.browser.tab.EmptyTabObserver;
import org.chromium.chrome.browser.tab.Tab;
import org.chromium.chrome.browser.tab.TabObserver;
import org.chromium.chrome.browser.theme.ThemeColorProvider;
import org.chromium.chrome.browser.ui.actions.ActionId;
import org.chromium.chrome.browser.ui.theme.BrandedColorScheme;
import org.chromium.components.embedder_support.util.UrlUtilities;
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
    }

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
    private final boolean mShouldIncludeGlic;
    private final NullableObservableSupplier<Profile> mProfileSupplier;
    private final Callback<@Nullable Profile> mProfileObserver = this::updateGlicVisibility;

    private @Nullable Tab mCurrentTab;
    private @Nullable Boolean mIsVisible;

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
            boolean shouldIncludeGlic,
            NullableObservableSupplier<Profile> profileSupplier,
            NonNullObservableSupplier<Boolean> omniboxFocusStateSupplier) {
        mModel = model;
        mButtonManager = buttonManager;
        mThemeColorProvider = themeColorProvider;
        mTabSupplier = tabSupplier;
        mHomepageEnabledSupplier = homepageEnabledSupplier;
        mVisibilityDelegate = visibilityDelegate;
        mShouldIncludeHomeButton = shouldIncludeHomeButton;
        mShouldIncludeGlic = shouldIncludeGlic;
        mProfileSupplier = profileSupplier;
        mOmniboxFocusStateSupplier = omniboxFocusStateSupplier;

        mTabObserver =
                new EmptyTabObserver() {
                    @Override
                    public void onUrlUpdated(Tab tab) {
                        updateVisibility();
                    }
                };

        mThemeColorProvider.addTintObserver(this);
        mModel.set(BottomBarProperties.COLOR_SCHEME, mThemeColorProvider.getBrandedColorScheme());
        if (mShouldIncludeGlic) {
            mProfileSupplier.addSyncObserverAndCallIfNonNull(mProfileObserver);
        }
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
        mIsVisible = isVisible;

        mModel.set(BottomBarProperties.IS_VISIBLE, isVisible);
        mVisibilityDelegate.onVisibilityChanged(isVisible);
    }

    private void updateGlicVisibility(@Nullable Profile profile) {
        // We only care about whether the original profile allows GLIC. To disable on OTR profiles
        // we rely on the button state which is set elsewhere.
        boolean shouldBeVisible =
                profile != null && GlicEnabling.isEnabledForProfile(profile.getOriginalProfile());
        setButtonVisibility(ActionId.GLIC, shouldBeVisible);
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
        if (mShouldIncludeGlic) {
            mProfileSupplier.removeObserver(mProfileObserver);
        }
        mOmniboxFocusStateSupplier.removeObserver(mOmniboxFocusObserver);
    }
}
