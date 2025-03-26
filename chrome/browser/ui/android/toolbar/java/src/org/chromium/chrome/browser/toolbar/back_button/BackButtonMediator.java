// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.toolbar.back_button;

import org.chromium.build.annotations.NullMarked;
import org.chromium.ui.modelutil.PropertyModel;

/**
 * A class responsible for mediating external events like theme changes or visibility changes from
 * external classes and reflects all these changes on back button model.
 */
@NullMarked
class BackButtonMediator {

    private final PropertyModel mModel;

    /**
     * Create an instance of {@link BackButtonMediator}.
     *
     * @param model a model that represents back button state.
     * @param onBackPressed a callback that is invoked on back button click event. Allows parent
     *     components to intercept click and navigate back in the history or hide custom UI
     *     components.
     */
    public BackButtonMediator(PropertyModel model, Runnable onBackPressed) {
        mModel = model;

        mModel.set(BackButtonProperties.CLICK_LISTENER, onBackPressed);
    }

    /**
     * Cleans up mediator resources and unsubscribes from external events. An instance can't be used
     * after this method is called.
     */
    public void destroy() {
        mModel.set(BackButtonProperties.CLICK_LISTENER, null);
    }
}
