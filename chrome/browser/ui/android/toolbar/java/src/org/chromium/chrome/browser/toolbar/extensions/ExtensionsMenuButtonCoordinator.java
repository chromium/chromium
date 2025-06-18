// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.toolbar.extensions;

import android.content.Context;
import android.content.res.ColorStateList;
import android.view.View;

import androidx.core.widget.ImageViewCompat;

import com.google.android.material.divider.MaterialDivider;

import org.chromium.base.Callback;
import org.chromium.base.lifetime.Destroyable;
import org.chromium.base.supplier.ObservableSupplier;
import org.chromium.build.annotations.NullMarked;
import org.chromium.build.annotations.Nullable;
import org.chromium.chrome.browser.profiles.Profile;
import org.chromium.chrome.browser.theme.ThemeColorProvider;
import org.chromium.chrome.browser.ui.theme.BrandedColorScheme;
import org.chromium.ui.listmenu.ListMenuButton;

/** Root component for the extension menu button. */
@NullMarked
public class ExtensionsMenuButtonCoordinator implements Destroyable {

    private final ListMenuButton mExtensionsMenuButton;
    private final MaterialDivider mExtensionsMenuTabSwitcherDivider;
    private final ThemeColorProvider mThemeColorProvider;
    private final ObservableSupplier<Profile> mProfileSupplier;

    private final ThemeColorProvider.TintObserver mTintObserver = this::onTintChanged;
    private final Callback<Profile> mProfileUpdatedCallback = this::onProfileUpdated;

    @Nullable private Profile mProfile;

    public ExtensionsMenuButtonCoordinator(
            Context context,
            ListMenuButton extensionsMenuButton,
            MaterialDivider extensionsMenuTabSwitcherDivider,
            ThemeColorProvider themeColorProvider,
            ObservableSupplier<Profile> profileSupplier) {
        mExtensionsMenuButton = extensionsMenuButton;
        mExtensionsMenuButton.setOnClickListener(this::onClick);

        mExtensionsMenuTabSwitcherDivider = extensionsMenuTabSwitcherDivider;

        mProfileSupplier = profileSupplier;

        mThemeColorProvider = themeColorProvider;
        mThemeColorProvider.addTintObserver(mTintObserver);

        mProfileSupplier.addObserver(mProfileUpdatedCallback);
    }

    private void onProfileUpdated(@Nullable Profile profile) {
        if (profile == mProfile) {
            return;
        }

        mProfile = profile;

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

    void onClick(View view) {
        if (view != mExtensionsMenuButton) return;

        // TODO(crbug.com/409181513): Implement popup view for extensions.
    }

    public void onTintChanged(
            @Nullable ColorStateList tintList,
            @Nullable ColorStateList activityFocusTintList,
            @BrandedColorScheme int brandedColorScheme) {
        ImageViewCompat.setImageTintList(mExtensionsMenuButton, activityFocusTintList);
    }

    @Override
    public void destroy() {
        mExtensionsMenuButton.setOnClickListener(null);
        mThemeColorProvider.removeTintObserver(mTintObserver);
        mProfileSupplier.removeObserver(mProfileUpdatedCallback);
        mProfile = null;
    }
}
