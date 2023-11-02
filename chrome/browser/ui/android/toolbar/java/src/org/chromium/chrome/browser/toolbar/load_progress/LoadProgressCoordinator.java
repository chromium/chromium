// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.toolbar.load_progress;

import androidx.annotation.NonNull;

import org.chromium.base.supplier.ObservableSupplier;
import org.chromium.chrome.browser.tab.Tab;
import org.chromium.chrome.browser.toolbar.ToolbarProgressBar;
import org.chromium.ui.modelutil.PropertyKey;
import org.chromium.ui.modelutil.PropertyModel;
import org.chromium.ui.modelutil.PropertyModelChangeProcessor;

/**
 * Coordinator for the load progress bar. Owns all progress bar sub-components.
 */
public class LoadProgressCoordinator {
    private final PropertyModel mModel;
    private final LoadProgressMediator mMediator;
    private final ToolbarProgressBar mProgressBarView;
    private final LoadProgressViewBinder mLoadProgressViewBinder;
    private final PropertyModelChangeProcessor<PropertyModel, ToolbarProgressBar, PropertyKey>
            mPropertyModelChangeProcessor;

    /**
     * @param tabSupplier An observable supplier of the current {@link Tab}.
     * @param progressBarView Toolbar progress bar view.
     * @param isStartSurfaceEnabled Whether start surface is enabled via a feature flag.
     */
    public LoadProgressCoordinator(@NonNull ObservableSupplier<Tab> tabSupplier,
            @NonNull ToolbarProgressBar progressBarView, boolean isStartSurfaceEnabled) {
        mProgressBarView = progressBarView;
        mModel = new PropertyModel(LoadProgressProperties.ALL_KEYS);
        mMediator = new LoadProgressMediator(tabSupplier, mModel, isStartSurfaceEnabled);
        mLoadProgressViewBinder = new LoadProgressViewBinder();

        mPropertyModelChangeProcessor = PropertyModelChangeProcessor.create(
                mModel, mProgressBarView, mLoadProgressViewBinder::bind);
    }

    /**
     * Simulates progressbar being filled over a short time.
     */
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
    }
}
