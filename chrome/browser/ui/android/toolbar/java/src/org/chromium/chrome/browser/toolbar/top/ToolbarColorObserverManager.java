// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.toolbar.top;

import androidx.annotation.ColorInt;

import org.chromium.build.annotations.NullMarked;
import org.chromium.build.annotations.Nullable;
import org.chromium.chrome.browser.toolbar.top.TopToolbarCoordinator.ToolbarColorObserver;
import org.chromium.ui.util.ColorUtils;

/**
 * A class to receive toolbar color change updates from toolbar components and send the rendering
 * toolbar color to the ToolbarColorObserver.
 */
@NullMarked
class ToolbarColorObserverManager implements ToolbarColorObserver {
    private @Nullable ToolbarColorObserver mToolbarColorObserver;
    private @ColorInt int mToolbarColor;

    /**
     * Set Toolbar Color Observer for the toolbar color changes.
     *
     * @param toolbarColorObserver The observer to listen to toolbar color change.
     */
    void setToolbarColorObserver(ToolbarColorObserver toolbarColorObserver) {
        mToolbarColorObserver = toolbarColorObserver;
        notifyToolbarColorChanged();
    }

    // TopToolbarCoordinator.ToolbarColorObserver implementation.
    @Override
    public void onToolbarColorChanged(@ColorInt int color) {
        mToolbarColor = color;
        notifyToolbarColorChanged();
    }

    /**
     * Notify the observer that the toolbar color is changed based on alpha value and toolbar color,
     * and send the rendering toolbar color to the observer.
     */
    private void notifyToolbarColorChanged() {
        if (mToolbarColorObserver == null) {
            return;
        }

        // #overlayColor does not allow colors with any transparency. During toolbar expansion,
        // our toolbar color does contain transparency, but this should all be gone once the
        // overview fade animation begins. However this class has no real concept of what the
        // true color is behind the toolbar is. It is possible to guess with
        // #getPrimaryBackgroundColor, but when showing new tab page, that isn't strictly
        // true. Just making the toolbar color opaque is good enough, though could cause some
        // colors to be slightly off.
        @ColorInt int opaqueToolbarColor = ColorUtils.getOpaqueColor(mToolbarColor);
        mToolbarColorObserver.onToolbarColorChanged(opaqueToolbarColor);
    }
}
