// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
package org.chromium.chrome.browser.toolbar.top;

import static org.chromium.chrome.browser.toolbar.top.ToolbarUtils.isToolbarTabletResizeRefactorEnabled;

import android.content.Context;

import org.chromium.build.annotations.NullMarked;
import org.chromium.chrome.browser.tabmodel.IncognitoStateProvider;
import org.chromium.chrome.browser.theme.ThemeColorProvider;
import org.chromium.chrome.browser.toolbar.R;

/**
 * A child of top toolbar that is a button. Holds common methods that toolbar button views share.
 */
@NullMarked
public abstract class ToolbarChildButton extends ToolbarChild {
    private final Context mContext;

    /**
     * Abstract parent class for a toolbar child button view.
     *
     * @param context The context in which this view is hosted.
     * @param topUiThemeColorProvider Provides theme and tint color that should be applied to the
     *     view.
     * @param incognitoStateProvider Provides incognito state used to update view.
     */
    public ToolbarChildButton(
            Context context,
            ThemeColorProvider topUiThemeColorProvider,
            IncognitoStateProvider incognitoStateProvider) {
        super(topUiThemeColorProvider, incognitoStateProvider);
        mContext = context;
    }

    /**
     * Sets button visibility.
     *
     * @param hasSpaceToShow indicates whether the button view has space to show.
     */
    public abstract void setHasSpaceToShow(boolean hasSpaceToShow);

    @Override
    public int updateVisibility(int availableWidth) {
        assert isToolbarTabletResizeRefactorEnabled();

        int width = mContext.getResources().getDimensionPixelSize(R.dimen.toolbar_button_width);
        if (availableWidth >= width) {
            setHasSpaceToShow(true);
            return width;
        } else {
            setHasSpaceToShow(false);
            return 0;
        }
    }
}
