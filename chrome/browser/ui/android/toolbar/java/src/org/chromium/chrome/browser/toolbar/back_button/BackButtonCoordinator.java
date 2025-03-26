// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.toolbar.back_button;

import android.widget.ImageButton;

import org.chromium.build.annotations.NullMarked;
import org.chromium.ui.modelutil.PropertyModel;
import org.chromium.ui.modelutil.PropertyModelChangeProcessor;

/**
 * Root component for the reload button. Exposes public API for external consumers to interact with
 * the button and affect its state.
 */
@NullMarked
public class BackButtonCoordinator {
    private final BackButtonMediator mMediator;

    /**
     * Creates an instance of {@link BackButtonCoordinator}.
     *
     * @param view an Android {@link ImageButton}.
     * @param onBackPressed a callback that is invoked on back button click event. Allows parent
     *     components to intercept click and navigate back in the history or hide custom UI
     *     components.
     */
    public BackButtonCoordinator(ImageButton view, Runnable onBackPressed) {
        final var model = new PropertyModel.Builder(BackButtonProperties.ALL_KEYS).build();
        mMediator = new BackButtonMediator(model, onBackPressed);
        PropertyModelChangeProcessor.create(model, view, BackButtonViewBinder::bind);
    }

    /**
     * Cleans up coordinator resources and unsubscribes from external events. An instance can't be
     * used after this method is called.
     */
    public void destroy() {
        mMediator.destroy();
    }
}
