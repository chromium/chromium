// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.ui.bottombar;

import android.content.res.ColorStateList;

import org.chromium.base.Callback;
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
import org.chromium.chrome.browser.ui.theme.BrandedColorScheme;
import org.chromium.components.embedder_support.util.UrlUtilities;
import org.chromium.ui.modelutil.PropertyModel;

/** Mediator for the bottom bar */
@NullMarked
public class BottomBarMediator implements ThemeColorProvider.TintObserver {
    /** Delegate for compositor-level visibility changes. */
    public interface VisibilityDelegate {
        /**
         * Called when the visibility of the bottom bar changes.
         *
         * @param isVisible True if the bottom bar is visible, false otherwise.
         */
        void onVisibilityChanged(boolean isVisible);
    }

    private final PropertyModel mModel;
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
            ThemeColorProvider themeColorProvider,
            NullableObservableSupplier<Tab> tabSupplier,
            NonNullObservableSupplier<Boolean> homepageEnabledSupplier,
            VisibilityDelegate visibilityDelegate,
            boolean shouldIncludeHomeButton,
            NullableObservableSupplier<Profile> profileSupplier,
            NonNullObservableSupplier<Boolean> omniboxFocusStateSupplier) {
        mModel = model;
        mThemeColorProvider = themeColorProvider;
        mTabSupplier = tabSupplier;
        mHomepageEnabledSupplier = homepageEnabledSupplier;
        mVisibilityDelegate = visibilityDelegate;
        mShouldIncludeHomeButton = shouldIncludeHomeButton;
        mProfileSupplier = profileSupplier;
        mOmniboxFocusStateSupplier = omniboxFocusStateSupplier;

        mProfileSupplier.addSyncObserverAndCallIfNonNull(mProfileObserver);
        mOmniboxFocusStateSupplier.addSyncObserver(mOmniboxFocusObserver);
        mTabObserver =
                new EmptyTabObserver() {
                    @Override
                    public void onUrlUpdated(Tab tab) {
                        updateVisibility();
                    }
                };

        mThemeColorProvider.addTintObserver(this);
        mModel.set(BottomBarProperties.COLOR_SCHEME, mThemeColorProvider.getBrandedColorScheme());
        onTabChanged(mTabSupplier.addSyncObserver(mTabSupplierObserver));
        if (mShouldIncludeHomeButton) {
            mHomepageEnabledSupplier.addSyncObserverAndCallIfNonNull(mHomepageEnabledObserver);
        } else {
            updateNewTabButtonBackground();
        }
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
        boolean isVisible =
                !(BottomBarConfigUtils.shouldDisableOnNtp() && currentTabIsRegularNtp)
                        && !isOmniboxFocused;

        if (mIsVisible != null && mIsVisible == isVisible) return;
        mIsVisible = isVisible;

        mModel.set(BottomBarProperties.IS_VISIBLE, isVisible);
        mVisibilityDelegate.onVisibilityChanged(isVisible);
    }

    private void updateGlicVisibility(@Nullable Profile profile) {
        if (profile == null) {
            mModel.set(BottomBarProperties.IS_GLIC_BUTTON_VISIBLE, false);
            return;
        }
        // We only care about whether the original profile allows GLIC. To disable on OTR profiles
        // we rely on the button state which is set elsewhere.
        boolean isGlicVisible = GlicEnabling.isEnabledForProfile(profile.getOriginalProfile());
        mModel.set(BottomBarProperties.IS_GLIC_BUTTON_VISIBLE, isGlicVisible);
    }

    private void onHomepageEnabledChanged(boolean isEnabled) {
        mModel.set(BottomBarProperties.IS_HOME_BUTTON_VISIBLE, isEnabled);
        updateNewTabButtonBackground();
    }

    private void updateNewTabButtonBackground() {
        // TODO(crbug.com/483096892): Come up with a more scalable solution for this.
        boolean isHomeButtonVisible = mModel.get(BottomBarProperties.IS_HOME_BUTTON_VISIBLE);
        int visibleLeft = isHomeButtonVisible ? 1 : 0;
        int visibleRight = 1 + (BottomBarConfigUtils.shouldIncludeAppMenuButton() ? 1 : 0);
        mModel.set(BottomBarProperties.IS_NEW_TAB_BACKGROUND_VISIBLE, visibleLeft == visibleRight);
    }

    /** Remove observers. */
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
        mOmniboxFocusStateSupplier.removeObserver(mOmniboxFocusObserver);
    }

    @Override
    public void onTintChanged(
            @Nullable ColorStateList tint,
            @Nullable ColorStateList activityFocusTint,
            @BrandedColorScheme int brandedColorScheme) {
        mModel.set(BottomBarProperties.COLOR_SCHEME, brandedColorScheme);
    }
}
