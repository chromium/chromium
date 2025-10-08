// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.components.browser_ui.accessibility;

import static org.chromium.components.browser_ui.accessibility.PageZoomProperties.DECREASE_ZOOM_CALLBACK;
import static org.chromium.components.browser_ui.accessibility.PageZoomProperties.DECREASE_ZOOM_ENABLED;
import static org.chromium.components.browser_ui.accessibility.PageZoomProperties.INCREASE_ZOOM_CALLBACK;
import static org.chromium.components.browser_ui.accessibility.PageZoomProperties.INCREASE_ZOOM_ENABLED;
import static org.chromium.components.browser_ui.accessibility.PageZoomProperties.RESET_ZOOM_CALLBACK;
import static org.chromium.components.browser_ui.accessibility.PageZoomProperties.ZOOM_PERCENT_TEXT;

import android.view.View;
import android.widget.ImageButton;
import android.widget.TextView;

import org.chromium.build.annotations.NullMarked;
import org.chromium.ui.modelutil.PropertyKey;
import org.chromium.ui.modelutil.PropertyModel;

/** A custom binder used to bind the zoom menu item. */
@NullMarked
public class PageZoomIndicatorViewBinder {
    /** Handles binding the view and models changes. */
    public static void bind(PropertyModel model, View view, PropertyKey key) {
        if (key == INCREASE_ZOOM_CALLBACK) {
            View zoomInButton = view.findViewById(R.id.zoom_in_button);
            zoomInButton.setOnClickListener(v -> model.get(INCREASE_ZOOM_CALLBACK).run());
        } else if (key == DECREASE_ZOOM_CALLBACK) {
            View zoomOutButton = view.findViewById(R.id.zoom_out_button);
            zoomOutButton.setOnClickListener(v -> model.get(DECREASE_ZOOM_CALLBACK).run());
        } else if (key == INCREASE_ZOOM_ENABLED) {
            ImageButton zoomInButton = view.findViewById(R.id.zoom_in_button);
            zoomInButton.setEnabled(model.get(INCREASE_ZOOM_ENABLED));
            zoomInButton.setFocusable(model.get(INCREASE_ZOOM_ENABLED));
        } else if (key == DECREASE_ZOOM_ENABLED) {
            ImageButton zoomOutButton = view.findViewById(R.id.zoom_out_button);
            zoomOutButton.setEnabled(model.get(DECREASE_ZOOM_ENABLED));
            zoomOutButton.setFocusable(model.get(DECREASE_ZOOM_ENABLED));
        } else if (key == ZOOM_PERCENT_TEXT) {
            ((TextView) view.findViewById(R.id.zoom_percentage))
                    .setText(model.get(ZOOM_PERCENT_TEXT));
        } else if (key == RESET_ZOOM_CALLBACK) {
            View resetZoomButton = view.findViewById(R.id.reset_zoom_button);
            resetZoomButton.setOnClickListener(v -> model.get(RESET_ZOOM_CALLBACK).run());
        }
    }
}
