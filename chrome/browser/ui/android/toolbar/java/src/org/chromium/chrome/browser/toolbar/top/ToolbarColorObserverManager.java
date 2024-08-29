// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.toolbar.top;


import androidx.annotation.ColorInt;
import androidx.annotation.NonNull;
import androidx.annotation.Nullable;

import org.chromium.base.Callback;
import org.chromium.base.supplier.ObservableSupplier;
import org.chromium.chrome.browser.toolbar.top.TopToolbarCoordinator.ToolbarAlphaInOverviewObserver;
import org.chromium.chrome.browser.toolbar.top.TopToolbarCoordinator.ToolbarColorObserver;
import org.chromium.ui.util.ColorUtils;

/**
 * A class to receive toolbar color change updates from toolbar components and send the
 * rendering toolbar color to the ToolbarColorObserver.
 */
class ToolbarColorObserverManager implements ToolbarAlphaInOverviewObserver, ToolbarColorObserver {
    private final Callback<Integer> mOnOverviewIncognitoChange = this::onOverviewColorChange;

    private @Nullable ToolbarColorObserver mToolbarColorObserver;
    private ObservableSupplier<Integer> mOverviewColorSupplier;
    private float mOverviewAlpha;
    private @ColorInt int mToolbarColor;

    ToolbarColorObserverManager() {
        mOverviewAlpha = 0;
    }

    /**
     * @param overviewColorSupplier Notifies when the overview color changes.
     */
    void setOverviewColorSupplier(ObservableSupplier<Integer> overviewColorSupplier) {
        if (mOverviewColorSupplier != null) {
            mOverviewColorSupplier.removeObserver(mOnOverviewIncognitoChange);
        }
        mOverviewColorSupplier = overviewColorSupplier;
        mOverviewColorSupplier.addObserver(mOnOverviewIncognitoChange);

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

    private void onOverviewColorChange(Integer ignored) {
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
        if (mToolbarColorObserver == null
                || mOverviewColorSupplier == null
                || mOverviewColorSupplier.get() == null) {
            return;
        }

        @ColorInt int overviewColor = mOverviewColorSupplier.get();

        // #overlayColor does not allow colors with any transparency. During toolbar expansion,
        // our toolbar color does contain transparency, but this should all be gone once the
        // overview fade animation begins. However this class has no real concept of what the
        // true color is behind the toolbar is. It is possible to guess with
        // #getPrimaryBackgroundColor, but when showing new tab page, that isn't strictly
        // true. Just making the toolbar color opaque is good enough, though could cause some
        // colors to be slightly off.
        @ColorInt int opaqueToolbarColor = ColorUtils.getOpaqueColor(mToolbarColor);

        final @ColorInt int toolbarRenderingColor =
                ColorUtils.getColorWithOverlay(opaqueToolbarColor, overviewColor, mOverviewAlpha);
        mToolbarColorObserver.onToolbarColorChanged(toolbarRenderingColor);
    }
}
