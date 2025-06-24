// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.toolbar.load_progress;

import android.view.View;

import org.chromium.base.supplier.ObservableSupplier;
import org.chromium.build.annotations.NullMarked;
import org.chromium.build.annotations.Nullable;
import org.chromium.chrome.browser.browser_controls.TopControlLayer;
import org.chromium.chrome.browser.browser_controls.TopControlsStacker;
import org.chromium.chrome.browser.browser_controls.TopControlsStacker.TopControlVisibility;
import org.chromium.chrome.browser.tab.Tab;
import org.chromium.chrome.browser.toolbar.ToolbarProgressBar;
import org.chromium.ui.modelutil.PropertyModel;
import org.chromium.ui.modelutil.PropertyModelChangeProcessor;

/** Coordinator for the load progress bar. Owns all progress bar sub-components. */
@NullMarked
public class LoadProgressCoordinator implements TopControlLayer {
    private final PropertyModel mModel;
    private final LoadProgressMediator mMediator;
    private final ToolbarProgressBar mProgressBarView;
    private final LoadProgressViewBinder mLoadProgressViewBinder;
    private final TopControlsStacker mTopControlsStacker;

    /**
     * @param tabSupplier An observable supplier of the current {@link Tab}.
     * @param progressBarView Toolbar progress bar view.
     * @param topControlsStacker TopControlsStacker to manage the view's y-offset.
     */
    public LoadProgressCoordinator(
            ObservableSupplier<@Nullable Tab> tabSupplier,
            ToolbarProgressBar progressBarView,
            TopControlsStacker topControlsStacker) {
        mProgressBarView = progressBarView;
        mModel = new PropertyModel(LoadProgressProperties.ALL_KEYS);
        mMediator = new LoadProgressMediator(tabSupplier, mModel);
        mLoadProgressViewBinder = new LoadProgressViewBinder();

        PropertyModelChangeProcessor.create(
                mModel, mProgressBarView, mLoadProgressViewBinder::bind);

        mTopControlsStacker = topControlsStacker;
        mTopControlsStacker.addControl(this);
    }

    /** Simulates progressbar being filled over a short time. */
    public void simulateLoadProgressCompletion() {
        mMediator.simulateLoadProgressCompletion();
    }

    /**
     * Whether progressbar should be updated on tab progress changes.
     * @param preventUpdates If true, prevents updating progressbar when the tab it's observing
     *                       is being loaded.
     */
    public void setPreventUpdates(boolean preventUpdates) {
        mMediator.setPreventUpdates(preventUpdates);
    }

    /** Destroy load progress bar object. */
    public void destroy() {
        mMediator.destroy();
        mTopControlsStacker.removeControl(this);
    }

    // TopControlLayer implementation:

    @Override
    public @TopControlsStacker.TopControlType int getTopControlType() {
        return TopControlsStacker.TopControlType.PROGRESS_BAR;
    }

    @Override
    public int getTopControlHeight() {
        // The height likely isn't relevant to the TopControlsStacker since the progress bar does
        // not contribute to the total height of the top controls, but we add it for consistency.
        return mProgressBarView.getHeight();
    }

    @Override
    public int getTopControlVisibility() {
        // TODO(crbug.com/417238089): Possibly add way to notify stacker of visibility changes.
        return mProgressBarView.getVisibility() == View.VISIBLE
                ? TopControlVisibility.VISIBLE
                : TopControlVisibility.HIDDEN;
    }

    @Override
    public boolean contributesToTotalHeight() {
        // The progress bar draws over other views, so it does not add height to the top controls.
        return false;
    }
}
