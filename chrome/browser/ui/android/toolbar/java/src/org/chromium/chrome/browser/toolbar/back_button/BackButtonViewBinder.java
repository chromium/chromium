// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.toolbar.back_button;

import android.widget.ImageButton;

import androidx.core.widget.ImageViewCompat;

import org.chromium.ui.modelutil.PropertyKey;
import org.chromium.ui.modelutil.PropertyModel;

/**
 * A class that binds changed {@link PropertyKey} in the {@link PropertyModel} to actual Android
 * view.
 */
class BackButtonViewBinder {

    private BackButtonViewBinder() {}

    /**
     * Determines which {@link PropertyKey} has changed in the {@link PropertyModel} and applies its
     * value to the {@link ImageButton}.
     *
     * @param model back button model that represents most resent state.
     * @param button actual Android {@link ImageButton} that is changed.
     * @param key a key that has changed in the {@link PropertyModel}.
     */
    public static void bind(PropertyModel model, ImageButton button, PropertyKey key) {
        if (key == BackButtonProperties.CLICK_LISTENER) {
            final var listener = model.get(BackButtonProperties.CLICK_LISTENER);
            button.setOnClickListener(
                    view -> {
                        if (listener != null) {
                            listener.run();
                        }
                    });
        } else if (key == BackButtonProperties.TINT_COLOR_LIST) {
            ImageViewCompat.setImageTintList(
                    button, model.get(BackButtonProperties.TINT_COLOR_LIST));
        } else if (key == BackButtonProperties.BACKGROUND_HIGHLIGHT_RESOURCE) {
            button.setBackgroundResource(
                    model.get(BackButtonProperties.BACKGROUND_HIGHLIGHT_RESOURCE));
        } else if (key == BackButtonProperties.LONG_CLICK_LISTENER) {
            final var listener = model.get(BackButtonProperties.LONG_CLICK_LISTENER);
            button.setOnLongClickListener(
                    view -> {
                        if (listener == null) {
                            return false;
                        }

                        listener.run();
                        return true;
                    });
        } else {
            assert false : String.format("Unsupported property key %s", key.toString());
        }
    }
}
