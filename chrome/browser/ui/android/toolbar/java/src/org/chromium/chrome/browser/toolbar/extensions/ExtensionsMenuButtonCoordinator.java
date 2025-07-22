// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.toolbar.extensions;

import android.content.Context;
import android.content.res.ColorStateList;
import android.view.View;

import androidx.annotation.VisibleForTesting;
import androidx.core.widget.ImageViewCompat;

import com.google.android.material.divider.MaterialDivider;

import org.chromium.base.Callback;
import org.chromium.base.lifetime.Destroyable;
import org.chromium.base.supplier.ObservableSupplier;
import org.chromium.build.annotations.NullMarked;
import org.chromium.build.annotations.Nullable;
import org.chromium.chrome.browser.profiles.Profile;
import org.chromium.chrome.browser.tab.Tab;
import org.chromium.chrome.browser.tabmodel.TabCreator;
import org.chromium.chrome.browser.theme.ThemeColorProvider;
import org.chromium.chrome.browser.ui.extensions.ExtensionActionsBridge;
import org.chromium.chrome.browser.ui.theme.BrandedColorScheme;
import org.chromium.ui.listmenu.ListMenuButton;

/** Root component for the extension menu button. */
@NullMarked
public class ExtensionsMenuButtonCoordinator implements Destroyable {

    private final Context mContext;
    private final ListMenuButton mExtensionsMenuButton;
    private final MaterialDivider mExtensionsMenuTabSwitcherDivider;
    private final ThemeColorProvider mThemeColorProvider;
    private final ObservableSupplier<Profile> mProfileSupplier;
    private final ObservableSupplier<Tab> mCurrentTabSupplier;
    private final TabCreator mTabCreator;

    private final ThemeColorProvider.TintObserver mTintObserver = this::onTintChanged;
    private final Callback<Profile> mProfileUpdatedCallback = this::onProfileUpdated;
    private final Callback<Tab> mTabChangedCallback = this::onTabChanged;

    @Nullable private Profile mProfile;
    @Nullable private Tab mCurrentTab;
    @VisibleForTesting @Nullable ExtensionsMenuCoordinator mExtensionsMenuCoordinator;

    public ExtensionsMenuButtonCoordinator(
            Context context,
            ListMenuButton extensionsMenuButton,
            MaterialDivider extensionsMenuTabSwitcherDivider,
            ThemeColorProvider themeColorProvider,
            ObservableSupplier<Profile> profileSupplier,
            ObservableSupplier<Tab> currentTabSupplier,
            TabCreator tabCreator) {
        mContext = context;

        mExtensionsMenuButton = extensionsMenuButton;
        mExtensionsMenuButton.setOnClickListener(this::onClick);

        mExtensionsMenuTabSwitcherDivider = extensionsMenuTabSwitcherDivider;

        mThemeColorProvider = themeColorProvider;
        mThemeColorProvider.addTintObserver(mTintObserver);

        mProfileSupplier = profileSupplier;
        mProfileSupplier.addObserver(mProfileUpdatedCallback);

        mCurrentTabSupplier = currentTabSupplier;
        mCurrentTabSupplier.addObserver(mTabChangedCallback);
        mTabCreator = tabCreator;
    }

    private void onProfileUpdated(@Nullable Profile profile) {
        if (profile == mProfile) {
            return;
        }

        mProfile = profile;

        // If the current tab belongs to a different profile, onTabChanged will be called soon, so
        // do not update actions now to avoid duplicated updates.
        if (mCurrentTab != null && mCurrentTab.getProfile() != mProfile) {
            return;
        }

        // TODO(crbug.com/431915409): Provide the ability to keep the menu open.
        if (mExtensionsMenuCoordinator != null) {
            mExtensionsMenuCoordinator.destroy();
            mExtensionsMenuCoordinator = null;
        }

        // TODO(crbug.com/422307625): Remove this check once extensions are ready for
        // dogfooding.
        int visibility = View.GONE;
        if (mProfile != null) {
            ExtensionActionsBridge extensionActionsBridge = ExtensionActionsBridge.get(mProfile);
            if (extensionActionsBridge != null && extensionActionsBridge.extensionsEnabled()) {
                visibility = View.VISIBLE;
            }
        }

        mExtensionsMenuButton.setVisibility(visibility);
        mExtensionsMenuTabSwitcherDivider.setVisibility(visibility);
    }

    private void onTabChanged(@Nullable Tab tab) {
        if (tab == mCurrentTab) {
            return;
        }

        if (tab == null) {
            // The current tab can be null when a non-tab UI is shown (e.g. tab switcher). In this
            // case, we do not bother refreshing actions as they're hidden anyway. We do not set
            // mCurrentTab to null because we can skip updating actions if the current tab is set
            // back to the previous tab.
            return;
        }

        mCurrentTab = tab;

        // If the tab belongs to a different profile, onProfileUpdated will be called soon, so
        // do not update actions now to avoid duplicated updates.
        if (tab.getProfile() != mProfile) {
            return;
        }

        // TODO(crbug.com/431915409): Provide the ability to keep the menu open.
        if (mExtensionsMenuCoordinator != null) {
            mExtensionsMenuCoordinator.destroy();
            mExtensionsMenuCoordinator = null;
        }
    }

    void onClick(View view) {
        if (mExtensionsMenuCoordinator == null) {
            mExtensionsMenuCoordinator =
                    new ExtensionsMenuCoordinator(
                            mContext,
                            mExtensionsMenuButton,
                            mProfileSupplier,
                            mCurrentTabSupplier,
                            mTabCreator);
        }

        mExtensionsMenuCoordinator.showMenu();
    }

    public void onTintChanged(
            @Nullable ColorStateList tintList,
            @Nullable ColorStateList activityFocusTintList,
            @BrandedColorScheme int brandedColorScheme) {
        ImageViewCompat.setImageTintList(mExtensionsMenuButton, activityFocusTintList);
    }

    public void updateButtonBackground(int backgroundResource) {
        mExtensionsMenuButton.setBackgroundResource(backgroundResource);
    }

    @Override
    public void destroy() {
        if (mExtensionsMenuCoordinator != null) {
            mExtensionsMenuCoordinator.destroy();
            mExtensionsMenuCoordinator = null;
        }
        mExtensionsMenuButton.setOnClickListener(null);
        mCurrentTabSupplier.removeObserver(mTabChangedCallback);
        mThemeColorProvider.removeTintObserver(mTintObserver);
        mProfileSupplier.removeObserver(mProfileUpdatedCallback);
        mProfile = null;
    }
}
