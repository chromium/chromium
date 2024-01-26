// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.toolbar.top;

import android.content.Context;

import androidx.annotation.ColorInt;
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
    private float mOverviewAlpha;
    private @ColorInt int mToolbarColor;

    ToolbarColorObserverManager(Context context) {
        mContext = context;
        mOverviewAlpha = 0;
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
    public void onToolbarColorChanged(@ColorInt int color) {
        mToolbarColor = color;
        notifyToolbarColorChanged();
    }

    // TopToolbarCoordinator.ToolbarAlphaInOverviewObserver implementation.
    @Override
    public void onOverviewAlphaChanged(float fraction) {
        mOverviewAlpha = fraction;
        notifyToolbarColorChanged();
    }

    /**
     * Notify the observer that the toolbar color is changed based on alpha value and toolbar color,
     * and send the rendering toolbar color to the observer.
     */
    private void notifyToolbarColorChanged() {
        if (mToolbarColorObserver != null && mIncognitoStateProvider != null) {
            boolean isIncognito = mIncognitoStateProvider.isIncognitoSelected();
            @ColorInt
            int overviewColor = ChromeColors.getPrimaryBackgroundColor(mContext, isIncognito);

            // #overlayColor does not allow colors with any transparency. During toolbar expansion,
            // our toolbar color does contain transparency, but this should all be gone once the
            // overview fade animation begins. However this class has no real concept of what the
            // true color is behind the toolbar is. It is possible to guess with
            // #getPrimaryBackgroundColor, but with surface polish enabled, that isn't strictly
            // true. Just making the toolbar color opaque is good enough, though could cause some
            // colors to be slightly off.
            @ColorInt int opaqueToolbarColor = ColorUtils.getOpaqueColor(mToolbarColor);

            @ColorInt
            int toolbarRenderingColor =
                    ColorUtils.getColorWithOverlay(
                            opaqueToolbarColor, overviewColor, mOverviewAlpha);
            mToolbarColorObserver.onToolbarColorChanged(toolbarRenderingColor);
        }
    }
}
