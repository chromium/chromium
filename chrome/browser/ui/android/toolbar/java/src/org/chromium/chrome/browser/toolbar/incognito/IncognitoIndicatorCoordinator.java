// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
package org.chromium.chrome.browser.toolbar.incognito;

import static android.view.View.GONE;
import static android.view.View.VISIBLE;

import android.graphics.Canvas;
import android.view.View;
import android.view.ViewStub;

import org.chromium.build.annotations.NullMarked;
import org.chromium.build.annotations.Nullable;
import org.chromium.chrome.browser.flags.ChromeFeatureList;
import org.chromium.chrome.browser.tabmodel.IncognitoStateProvider;
import org.chromium.chrome.browser.theme.ThemeColorProvider;
import org.chromium.chrome.browser.toolbar.R;
import org.chromium.chrome.browser.toolbar.top.ToolbarChild;
import org.chromium.chrome.browser.toolbar.top.ToolbarLayout;

@NullMarked
public class IncognitoIndicatorCoordinator extends ToolbarChild {
    private final ToolbarLayout mParentToolbar;
    private @Nullable Boolean mIsIncognitoBranded;
    private boolean mVisible;
    private @Nullable View mIncognitoIndicator;

    /**
     * Creates an IncognitoIndicatorCoordinator for managing the incognito indicator on the top
     * toolbar.
     *
     * @param parentToolbar The parent view that contains the incognito indicator.
     * @param topUiThemeColorProvider Provides theme and tint color that should be applied to the
     *     view.
     * @param incognitoStateProvider Provides incognito state to update view.
     * @param visible Whether the toolbar buttons should start out being visible.
     */
    public IncognitoIndicatorCoordinator(
            ToolbarLayout parentToolbar,
            ThemeColorProvider topUiThemeColorProvider,
            IncognitoStateProvider incognitoStateProvider,
            boolean visible) {
        super(topUiThemeColorProvider, incognitoStateProvider);
        mParentToolbar = parentToolbar;
        mVisible = visible;
        setVisibility(mVisible);
    }

    @Override
    public void onIncognitoStateChanged(boolean isIncognito) {
        super.onIncognitoStateChanged(isIncognito);
        if (mIsIncognitoBranded != null && mIsIncognitoBranded == isIncognito) return;

        mIsIncognitoBranded = isIncognito;
        setVisibility(mVisible);
    }

    /**
     * Updates the visibility of the incognito indicator.
     *
     * @param visible Whether the toolbar buttons are visible.
     */
    public void setVisibility(boolean visible) {
        mVisible = visible;

        // If the parent toolbar is null, this was called before initialization was completed. Skip
        // for now and wait until a subsequent call after initialization has finished.
        if (mParentToolbar == null) return;
        if (mIsIncognitoBranded == null) return;
        if (!ChromeFeatureList.sTabStripIncognitoMigration.isEnabled()) return;

        if (mIncognitoIndicator == null && mIsIncognitoBranded) {
            ViewStub stub = mParentToolbar.findViewById(R.id.incognito_indicator_stub);
            mIncognitoIndicator = stub.inflate();
        }

        if (mIncognitoIndicator != null) {
            mIncognitoIndicator.setVisibility(mIsIncognitoBranded && visible ? VISIBLE : GONE);
        }
    }

    @Override
    public void draw(View root, Canvas canvas) {
        throw new UnsupportedOperationException("This method call is not yet supported.");
    }

    @Nullable View getIncognitoIndicatorViewForTesting() {
        return mIncognitoIndicator;
    }
}
