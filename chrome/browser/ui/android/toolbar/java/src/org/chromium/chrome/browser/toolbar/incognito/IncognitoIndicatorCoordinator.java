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
import org.chromium.chrome.browser.toolbar.top.ToolbarUtils;

@NullMarked
public class IncognitoIndicatorCoordinator extends ToolbarChild {
    private final ToolbarLayout mParentToolbar;
    private @Nullable Boolean mIsIncognitoBranded;
    private boolean mVisible;
    private @Nullable View mIncognitoIndicator;
    private final int mDefaultFallbackWidth;
    private int mCachedWidth;

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

        // Use a width of three toolbar buttons as a fallback for displaying the incognito
        // indicator.
        int buttonWidth =
                mParentToolbar
                        .getContext()
                        .getResources()
                        .getDimensionPixelSize(R.dimen.toolbar_button_width);
        mDefaultFallbackWidth = 3 * buttonWidth;
    }

    @Override
    public void onIncognitoStateChanged(boolean isIncognito) {
        super.onIncognitoStateChanged(isIncognito);
        if (mIsIncognitoBranded != null && mIsIncognitoBranded == isIncognito) return;

        mIsIncognitoBranded = isIncognito;
        setVisibility(mVisible);
    }

    @Override
    public boolean isVisible() {
        return mIncognitoIndicator != null && mIncognitoIndicator.getVisibility() == View.VISIBLE;
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

    @Override
    public int updateVisibility(int availableWidth) {
        assert ToolbarUtils.isToolbarTabletResizeRefactorEnabled();
        // Hide and consume no width if the incognito indicator feature is not enabled, or if the
        // device is not in incognito mode. Do not cache the width of the indicator.
        if (!ChromeFeatureList.sTabStripIncognitoMigration.isEnabled()
                || Boolean.FALSE.equals(mIsIncognitoBranded)) {
            setVisibility(false);
            return 0;
        }

        // If the incognito indicator has been displayed, cache its measured width.
        if (mIncognitoIndicator != null && mIncognitoIndicator.getMeasuredWidth() > 0) {
            mCachedWidth = mIncognitoIndicator.getMeasuredWidth();
        } else {
            mCachedWidth = mDefaultFallbackWidth;
        }

        // Only display the indicator if there is enough available width. If the available width
        // is less than necessary, though, that extra width should still be consumed to avoid
        // showing any more buttons, as it might be confusing to users. This extra width will end up
        // absorbed into the location bar.
        setVisibility(availableWidth >= mCachedWidth);
        return Math.min(availableWidth, mCachedWidth);
    }

    /**
     * Returns whether the incognito indicator has a current measured width that is different from
     * its allocated width, and therefore needs another allocation update before being shown.
     */
    public boolean needsUpdateBeforeShowing() {
        if (!mVisible || Boolean.FALSE.equals(mIsIncognitoBranded) || mIncognitoIndicator == null) {
            return false;
        }
        return mCachedWidth != mIncognitoIndicator.getMeasuredWidth();
    }

    public @Nullable View getIncognitoIndicatorView() {
        return mIncognitoIndicator;
    }
}
