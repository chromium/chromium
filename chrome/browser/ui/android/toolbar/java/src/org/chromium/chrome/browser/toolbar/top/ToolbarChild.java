// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.toolbar.top;

import android.content.res.ColorStateList;
import android.graphics.Canvas;
import android.view.View;

import org.chromium.base.lifetime.Destroyable;
import org.chromium.build.annotations.NullMarked;
import org.chromium.build.annotations.Nullable;
import org.chromium.chrome.browser.tabmodel.IncognitoStateProvider;
import org.chromium.chrome.browser.tabmodel.IncognitoStateProvider.IncognitoStateObserver;
import org.chromium.chrome.browser.theme.ThemeColorProvider;
import org.chromium.chrome.browser.theme.ThemeColorProvider.TintObserver;

/** A child of top toolbar. Holds common methods that the sub-class should implement. */
@NullMarked
public abstract class ToolbarChild implements Destroyable, TintObserver, IncognitoStateObserver {

    protected final ThemeColorProvider mTopUiThemeColorProvider;
    protected final IncognitoStateProvider mIncognitoStateProvider;

    /**
     * Abstract parent class for a toolbar child view.
     *
     * @param topUiThemeColorProvider Provides theme and tint color that should be applied to the
     *     view.
     * @param incognitoStateProvider Provides incognito state used to update view.
     */
    public ToolbarChild(
            ThemeColorProvider topUiThemeColorProvider,
            IncognitoStateProvider incognitoStateProvider) {
        mTopUiThemeColorProvider = topUiThemeColorProvider;
        mIncognitoStateProvider = incognitoStateProvider;
        mTopUiThemeColorProvider.addTintObserver(this);
        mIncognitoStateProvider.addIncognitoStateObserverAndTrigger(this);
    }

    @Override
    public void destroy() {
        mTopUiThemeColorProvider.removeTintObserver(this);
        mIncognitoStateProvider.removeObserver(this);
    }

    @Override
    public void onTintChanged(
            @Nullable ColorStateList tint,
            @Nullable ColorStateList activityFocusTint,
            int brandedColorScheme) {}

    @Override
    public void onIncognitoStateChanged(boolean isIncognito) {}

    /**
     * Draws the current visual state of this component on provided canvas.
     *
     * @param root Root view for this view; used to position the canvas that's drawn on.
     * @param canvas Canvas to draw to.
     */
    public abstract void draw(View root, Canvas canvas);
}
