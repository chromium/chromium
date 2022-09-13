// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.components.browser_ui.accessibility;

import android.view.View;
import android.widget.SeekBar;

import org.chromium.ui.modelutil.PropertyKey;
import org.chromium.ui.modelutil.PropertyModel;

/**
 * ViewBinder for the page zoom feature.
 */
class PageZoomViewBinder {
    public static void bind(PropertyModel model, View view, PropertyKey propertyKey) {
        if (PageZoomProperties.CURRENT_SEEK_VALUE == propertyKey) {
            ((SeekBar) view.findViewById(R.id.page_zoom_slider))
                    .setProgress(model.get(PageZoomProperties.CURRENT_SEEK_VALUE));
        } else if (PageZoomProperties.MAXIMUM_SEEK_VALUE == propertyKey) {
            ((SeekBar) view.findViewById(R.id.page_zoom_slider))
                    .setMax(model.get(PageZoomProperties.MAXIMUM_SEEK_VALUE));
        } else if (PageZoomProperties.DECREASE_ZOOM_CALLBACK == propertyKey) {
            view.findViewById(R.id.page_zoom_decrease_zoom_button)
                    .setOnClickListener(v
                            -> model.get(PageZoomProperties.DECREASE_ZOOM_CALLBACK).onResult(null));
        } else if (PageZoomProperties.INCREASE_ZOOM_CALLBACK == propertyKey) {
            view.findViewById(R.id.page_zoom_increase_zoom_button)
                    .setOnClickListener(v
                            -> model.get(PageZoomProperties.INCREASE_ZOOM_CALLBACK).onResult(null));
        } else if (PageZoomProperties.DECREASE_ZOOM_ENABLED == propertyKey) {
            view.findViewById(R.id.page_zoom_decrease_zoom_button)
                    .setEnabled(model.get(PageZoomProperties.DECREASE_ZOOM_ENABLED));
        } else if (PageZoomProperties.INCREASE_ZOOM_ENABLED == propertyKey) {
            view.findViewById(R.id.page_zoom_increase_zoom_button)
                    .setEnabled(model.get(PageZoomProperties.INCREASE_ZOOM_ENABLED));
        } else if (PageZoomProperties.SEEKBAR_CHANGE_CALLBACK == propertyKey) {
            ((SeekBar) view.findViewById(R.id.page_zoom_slider))
                    .setOnSeekBarChangeListener(new SeekBar.OnSeekBarChangeListener() {
                        @Override
                        public void onProgressChanged(
                                SeekBar seekBar, int progress, boolean fromUser) {
                            if (fromUser) {
                                model.get(PageZoomProperties.SEEKBAR_CHANGE_CALLBACK)
                                        .onResult(progress);
                            }
                        }

                        @Override
                        public void onStartTrackingTouch(SeekBar seekBar) {}

                        @Override
                        public void onStopTrackingTouch(SeekBar seekBar) {}
                    });
        }
    }
}