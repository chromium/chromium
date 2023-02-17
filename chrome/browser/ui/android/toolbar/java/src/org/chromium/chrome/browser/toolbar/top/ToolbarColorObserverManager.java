// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.toolbar.top;

import android.content.Context;

import androidx.annotation.NonNull;
import androidx.annotation.Nullable;

import org.chromium.chrome.browser.tabmodel.IncognitoStateProvider;
import org.chromium.chrome.browser.toolbar.top.TopToolbarCoordinator.ToolbarAlphaInOverviewObserver;
import org.chromium.chrome.browser.toolbar.top.TopToolbarCoordinator.ToolbarColorObserver;
import org.chromium.components.browser_ui.styles.ChromeColors;
import org.chromium.ui.util.ColorUtils;

/**
 * A class to receive toolbar color change updates from toolbar components and send the
 * rendering toolbar color to the ToolbarColorObserver.
 */
class ToolbarColorObserverManager implements ToolbarAlphaInOverviewObserver, ToolbarColorObserver {
    private @Nullable ToolbarColorObserver mToolbarColorObserver;

    private Context mContext;
    private IncognitoStateProvider mIncognitoStateProvider;
    private float mToolbarAlphaValue;
    private int mToolbarColor;

    ToolbarColorObserverManager(Context context, ToolbarLayout toolbarLayout) {
        mContext = context;
        mToolbarAlphaValue = 0;
        // Initialize mToolbarColor for first load of website.
        if (toolbarLayout instanceof ToolbarPhone) {
            mToolbarColor = ((ToolbarPhone) toolbarLayout).getToolbarBackgroundColor();
        }
    }

    /**
     * @param provider The provider used to determine incognito state.
     */
    void setIncognitoStateProvider(IncognitoStateProvider provider) {
        mIncognitoStateProvider = provider;
        notifyToolbarColorChanged();
    }

    /**
     * Set Toolbar Color Observer for the toolbar color changes.
     * @param toolbarColorObserver The observer to listen to toolbar color change.
     */
    void setToolbarColorObserver(@NonNull ToolbarColorObserver toolbarColorObserver) {
        mToolbarColorObserver = toolbarColorObserver;
        notifyToolbarColorChanged();
    }

    // TopToolbarCoordinator.ToolbarColorObserver implementation.
    @Override
    public void onToolbarColorChanged(int color) {
        mToolbarColor = color;
        notifyToolbarColorChanged();
    }

    // TopToolbarCoordinator.ToolbarAlphaInOverviewObserver implementation.
    @Override
    public void onToolbarAlphaInOverviewChanged(float fraction) {
        mToolbarAlphaValue = fraction;
        notifyToolbarColorChanged();
    }

    /**
     * Notify the observer that the toolbar color is changed based on alpha value and toolbar color,
     * and send the rendering toolbar color to the observer.
     */
    private void notifyToolbarColorChanged() {
        if (mToolbarColorObserver != null && mIncognitoStateProvider != null) {
            boolean isIncognito = mIncognitoStateProvider.isIncognitoSelected();
            int overviewColor = ChromeColors.getPrimaryBackgroundColor(mContext, isIncognito);
            int toolbarRenderingColor = ColorUtils.getColorWithOverlay(
                    mToolbarColor, overviewColor, mToolbarAlphaValue);
            mToolbarColorObserver.onToolbarColorChanged(toolbarRenderingColor);
        }
    }
}