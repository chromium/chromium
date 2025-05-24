// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.toolbar.reload_button;

import android.annotation.SuppressLint;
import android.view.MotionEvent;
import android.view.View;
import android.widget.ImageButton;

import androidx.core.widget.ImageViewCompat;

import org.chromium.base.Callback;
import org.chromium.build.annotations.NullMarked;
import org.chromium.build.annotations.Nullable;
import org.chromium.ui.modelutil.PropertyKey;
import org.chromium.ui.modelutil.PropertyModel;

/** A binder that binds model changes to view state. */
@NullMarked
class ReloadButtonViewBinder {
    /**
     * Reflects model property change based on they key on the view.
     *
     * @param model reload button model.
     * @param button reload button.
     * @param key key of a property that has changed in the model.
     */
    public static void bind(PropertyModel model, ImageButton button, PropertyKey key) {
        if (key == ReloadButtonProperties.CLICK_LISTENER) {
            final Runnable listener = model.get(ReloadButtonProperties.CLICK_LISTENER);
            button.setOnClickListener(
                    view -> {
                        if (listener != null) {
                            listener.run();
                        }
                    });
        } else if (key == ReloadButtonProperties.TOUCH_LISTENER) {
            final Callback<MotionEvent> listener = model.get(ReloadButtonProperties.TOUCH_LISTENER);
            setTouchListener(button, listener);
        } else if (key == ReloadButtonProperties.KEY_LISTENER) {
            final View.OnKeyListener listener = model.get(ReloadButtonProperties.KEY_LISTENER);
            button.setOnKeyListener(listener);
        } else if (key == ReloadButtonProperties.CONTENT_DESCRIPTION) {
            button.setContentDescription(model.get(ReloadButtonProperties.CONTENT_DESCRIPTION));
        } else if (key == ReloadButtonProperties.DRAWABLE_LEVEL) {
            button.getDrawable().setLevel(model.get(ReloadButtonProperties.DRAWABLE_LEVEL));
        } else if (key == ReloadButtonProperties.IS_ENABLED) {
            button.setEnabled(model.get(ReloadButtonProperties.IS_ENABLED));
        } else if (key == ReloadButtonProperties.IS_VISIBLE) {
            button.setVisibility(
                    model.get(ReloadButtonProperties.IS_VISIBLE) ? View.VISIBLE : View.GONE);
        } else if (key == ReloadButtonProperties.ALPHA) {
            button.setAlpha(model.get(ReloadButtonProperties.ALPHA));
        } else if (key == ReloadButtonProperties.TINT_LIST) {
            ImageViewCompat.setImageTintList(button, model.get(ReloadButtonProperties.TINT_LIST));
        } else if (key == ReloadButtonProperties.BACKGROUND_HIGHLIGHT) {
            button.setBackground(model.get(ReloadButtonProperties.BACKGROUND_HIGHLIGHT));
        } else if (key == ReloadButtonProperties.LONG_CLICK_LISTENER) {
            final var listener = model.get(ReloadButtonProperties.LONG_CLICK_LISTENER);
            button.setOnLongClickListener(
                    view -> {
                        if (listener != null) {
                            listener.run();
                        }
                        return listener != null;
                    });
        } else {
            assert false : String.format("Unsupported property key %s", key.toString());
        }
    }

    // required to set a touch listener to intercept clicks with shift
    @SuppressLint("ClickableViewAccessibility")
    private static void setTouchListener(
            ImageButton button, @Nullable Callback<MotionEvent> listener) {
        if (listener == null) {
            button.setOnTouchListener(null);
        } else {
            button.setOnTouchListener(
                    (view, event) -> {
                        listener.onResult(event);
                        return false;
                    });
        }
    }

    private ReloadButtonViewBinder() {}
}
