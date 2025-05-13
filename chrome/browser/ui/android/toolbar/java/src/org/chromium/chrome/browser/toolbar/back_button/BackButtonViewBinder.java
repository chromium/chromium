// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.toolbar.back_button;

import android.view.View;

import androidx.core.widget.ImageViewCompat;

import org.chromium.build.annotations.NullMarked;
import org.chromium.ui.modelutil.PropertyKey;
import org.chromium.ui.modelutil.PropertyModel;
import org.chromium.ui.widget.ChromeImageButton;

/**
 * A class that binds changed {@link PropertyKey} in the {@link PropertyModel} to actual Android
 * view.
 */
@NullMarked
class BackButtonViewBinder {

    private BackButtonViewBinder() {}

    /**
     * Determines which {@link PropertyKey} has changed in the {@link PropertyModel} and applies its
     * value to the {@link ChromeImageButton}.
     *
     * @param model back button model that represents most resent state.
     * @param button actual Android {@link ChromeImageButton} that is changed.
     * @param key a key that has changed in the {@link PropertyModel}.
     */
    public static void bind(PropertyModel model, ChromeImageButton button, PropertyKey key) {
        if (key == BackButtonProperties.CLICK_LISTENER) {
            final var callback = model.get(BackButtonProperties.CLICK_LISTENER);
            button.setClickCallback(callback);
        } else if (key == BackButtonProperties.TINT_COLOR_LIST) {
            ImageViewCompat.setImageTintList(
                    button, model.get(BackButtonProperties.TINT_COLOR_LIST));
        } else if (key == BackButtonProperties.BACKGROUND_HIGHLIGHT) {
            button.setBackground(model.get(BackButtonProperties.BACKGROUND_HIGHLIGHT));
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
        } else if (key == BackButtonProperties.KEY_LISTENER) {
            final var listener = model.get(BackButtonProperties.KEY_LISTENER);
            button.setOnKeyListener(listener);
        } else if (key == BackButtonProperties.IS_ENABLED) {
            button.setEnabled(model.get(BackButtonProperties.IS_ENABLED));
        } else if (key == BackButtonProperties.IS_FOCUSABLE) {
            button.setFocusable(model.get(BackButtonProperties.IS_FOCUSABLE));
        } else if (key == BackButtonProperties.IS_VISIBLE) {
            button.setVisibility(
                    model.get(BackButtonProperties.IS_VISIBLE) ? View.VISIBLE : View.GONE);
        } else if (key == BackButtonProperties.ALPHA) {
            button.setAlpha(model.get(BackButtonProperties.ALPHA));
        } else {
            assert false : String.format("Unsupported property key %s", key.toString());
        }
    }
}
