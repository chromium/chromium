// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.ui.floating_pill;

import android.view.ViewGroup.MarginLayoutParams;

import org.chromium.base.lifetime.Destroyable;
import org.chromium.build.annotations.NullMarked;
import org.chromium.chrome.browser.browser_controls.BrowserControlsStateProvider;
import org.chromium.chrome.browser.browser_controls.BrowserControlsStateProvider.ControlsPosition;
import org.chromium.chrome.browser.browser_controls.TopControlLayer;
import org.chromium.chrome.browser.browser_controls.TopControlsStacker;
import org.chromium.chrome.browser.browser_controls.TopControlsStacker.TopControlType;
import org.chromium.chrome.browser.browser_controls.TopControlsStacker.TopControlVisibility;
import org.chromium.chrome.browser.flags.ChromeFeatureList;
import org.chromium.components.browser_ui.widget.floating_pill.FloatingPillContainer;

import java.util.ArrayList;
import java.util.List;

/**
 * The top controls layer for a window's floating pills, like auto browse's "Take over task" button.
 * Keeps each added {@link FloatingPillContainer} on the bottom edge of the top controls, or a
 * margin below them when the controls are at the bottom, as the controls resize or move. See {@link
 * FloatingPillContainer#updateTopMargin}. It doesn't add to the controls' height, and it ignores
 * their scroll offset and min height. It only joins the {@link TopControlsStacker} with the
 * FloatingPillLayer feature; without it, it follows the controls' height on its own.
 *
 * <p>TODO(b/580045795): Own a single container that lays the pills out side by side.
 */
@NullMarked
public class FloatingPillLayer implements TopControlLayer, Destroyable {
    private final TopControlsStacker mTopControlsStacker;
    private final BrowserControlsStateProvider mBrowserControlsStateProvider;
    private final boolean mJoinsStacker;
    private final List<FloatingPillContainer> mContainers = new ArrayList<>();
    private final BrowserControlsStateProvider.Observer mBrowserControlsObserver =
            new BrowserControlsStateProvider.Observer() {
                @Override
                public void onTopControlsHeightChanged(
                        int topControlsHeight, int topControlsMinHeight) {
                    // In the stacker, this comes through onTopControlLayerHeightChanged().
                    if (!mJoinsStacker) updateTopMargins(topControlsHeight);
                }

                @Override
                public void onControlsPositionChanged(@ControlsPosition int controlsPosition) {
                    updateTopMargins(mBrowserControlsStateProvider.getTopControlsHeight());
                }
            };

    /**
     * @param topControlsStacker The window's stacker, which this joins while it has containers.
     * @param browserControlsStateProvider Provides the height and position of the controls.
     */
    public FloatingPillLayer(
            TopControlsStacker topControlsStacker,
            BrowserControlsStateProvider browserControlsStateProvider) {
        mTopControlsStacker = topControlsStacker;
        mBrowserControlsStateProvider = browserControlsStateProvider;
        mJoinsStacker = ChromeFeatureList.sFloatingPillLayer.isEnabled();
    }

    /**
     * Places {@code container} against the browser controls now and whenever they change, until
     * it's removed. Add it only while it's shown, and remove it when it's hidden: the layer reports
     * itself as visible to the stacker while it has any container.
     *
     * @param container The container to place. Must already be in a parent with margin layout
     *     params, e.g. a FrameLayout, so that it's placed right away.
     */
    public void addContainer(FloatingPillContainer container) {
        assert container.getLayoutParams() instanceof MarginLayoutParams
                : "The container must be in a parent with margin layout params.";
        assert !mContainers.contains(container) : "The container was already added.";
        if (mContainers.isEmpty()) {
            if (mJoinsStacker) mTopControlsStacker.addControl(this);
            mBrowserControlsStateProvider.addObserver(mBrowserControlsObserver);
        }
        mContainers.add(container);
        updateTopMargin(container, mBrowserControlsStateProvider.getTopControlsHeight());
    }

    /** Stops placing {@code container}. No-op if it wasn't added. */
    public void removeContainer(FloatingPillContainer container) {
        if (!mContainers.remove(container) || !mContainers.isEmpty()) return;
        if (mJoinsStacker) mTopControlsStacker.removeControl(this);
        mBrowserControlsStateProvider.removeObserver(mBrowserControlsObserver);
    }

    @Override
    public void destroy() {
        if (mContainers.isEmpty()) return;
        mContainers.clear();
        if (mJoinsStacker) mTopControlsStacker.removeControl(this);
        mBrowserControlsStateProvider.removeObserver(mBrowserControlsObserver);
    }

    // TopControlLayer:

    @Override
    public @TopControlType int getTopControlType() {
        return TopControlType.FLOATING_PILL;
    }

    @Override
    public int getTopControlHeight() {
        return 0;
    }

    @Override
    public @TopControlVisibility int getTopControlVisibility() {
        return TopControlVisibility.VISIBLE;
    }

    @Override
    public boolean contributesToTotalHeight() {
        // The pills draw over the web contents, so they don't add to the controls' height.
        return false;
    }

    @Override
    public void onTopControlLayerHeightChanged(int topControlsHeight, int topControlsMinHeight) {
        updateTopMargins(topControlsHeight);
    }

    private void updateTopMargins(int topControlsHeight) {
        for (FloatingPillContainer container : mContainers) {
            updateTopMargin(container, topControlsHeight);
        }
    }

    private void updateTopMargin(FloatingPillContainer container, int topControlsHeight) {
        container.updateTopMargin(
                topControlsHeight,
                mBrowserControlsStateProvider.getControlsPosition() == ControlsPosition.BOTTOM);
    }
}
