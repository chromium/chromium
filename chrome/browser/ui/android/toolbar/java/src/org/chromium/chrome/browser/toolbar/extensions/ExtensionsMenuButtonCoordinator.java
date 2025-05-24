// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.toolbar.extensions;

import android.content.Context;
import android.content.res.ColorStateList;
import android.view.View;

import androidx.core.widget.ImageViewCompat;

import org.chromium.base.lifetime.Destroyable;
import org.chromium.build.annotations.NullMarked;
import org.chromium.build.annotations.Nullable;
import org.chromium.chrome.browser.theme.ThemeColorProvider;
import org.chromium.chrome.browser.ui.theme.BrandedColorScheme;
import org.chromium.ui.listmenu.ListMenuButton;

/** Root component for the extension menu button. */
@NullMarked
public class ExtensionsMenuButtonCoordinator implements Destroyable {

    private final ListMenuButton mExtensionsMenuButton;
    private final ThemeColorProvider mThemeColorProvider;
    private final ThemeColorProvider.TintObserver mTintObserver;

    public ExtensionsMenuButtonCoordinator(
            Context context,
            ListMenuButton extensionsMenuButton,
            ThemeColorProvider themeColorProvider) {
        mExtensionsMenuButton = extensionsMenuButton;
        mExtensionsMenuButton.setOnClickListener(this::onClick);

        mThemeColorProvider = themeColorProvider;
        mTintObserver = this::onTintChanged;
        mThemeColorProvider.addTintObserver(mTintObserver);
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
    }
}
