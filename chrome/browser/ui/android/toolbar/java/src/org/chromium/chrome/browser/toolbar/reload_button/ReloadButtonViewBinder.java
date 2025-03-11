// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.toolbar.reload_button;

import android.widget.ImageButton;

import org.chromium.ui.modelutil.PropertyKey;
import org.chromium.ui.modelutil.PropertyModel;

/** A binder that binds model changes to view state. */
class ReloadButtonViewBinder {
    /**
     * Reflects model property change based on they key on the view.
     *
     * @param model reload button model.
     * @param button reload button.
     * @param key key of a property that has changed in the model.
     */
    public static void bind(PropertyModel model, ImageButton button, PropertyKey key) {}

    private ReloadButtonViewBinder() {}
}
