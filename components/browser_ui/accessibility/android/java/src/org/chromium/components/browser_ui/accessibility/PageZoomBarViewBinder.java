// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.components.browser_ui.accessibility;

import android.view.View;
import android.widget.SeekBar;
import android.widget.TextView;

import com.google.android.material.slider.Slider;

import org.chromium.build.annotations.NullMarked;
import org.chromium.ui.modelutil.PropertyKey;
import org.chromium.ui.modelutil.PropertyModel;

/** ViewBinder for the page zoom feature. */
@NullMarked
class PageZoomBarViewBinder {
    public static void bind(PropertyModel model, View view, PropertyKey propertyKey) {
        Slider slider = view.findViewById(R.id.page_zoom_slider);
        SeekBar seekBar = view.findViewById(R.id.page_zoom_slider_legacy);

        if (PageZoomProperties.CURRENT_BAR_VALUE == propertyKey) {
            if (model.get(PageZoomProperties.USE_SLIDER)) {
                slider.setValue(model.get(PageZoomProperties.CURRENT_BAR_VALUE));
            } else {
                seekBar.setProgress(model.get(PageZoomProperties.CURRENT_BAR_VALUE));
            }

            TextView textView = view.findViewById(R.id.page_zoom_current_zoom_level);

            long zoomLevel =
                    Math.round(
                            100
                                    * PageZoomUtils.convertBarValueToZoomLevel(
                                            model.get(PageZoomProperties.CURRENT_BAR_VALUE)));
            textView.setText(view.getContext().getString(R.string.page_zoom_level, zoomLevel));
            textView.setContentDescription(
                    view.getContext().getString(R.string.page_zoom_level_label, zoomLevel));
        } else if (PageZoomProperties.MAXIMUM_BAR_VALUE == propertyKey) {
            if (model.get(PageZoomProperties.USE_SLIDER)) {
                slider.setValueTo(model.get(PageZoomProperties.MAXIMUM_BAR_VALUE));
            } else {
                seekBar.setMax(model.get(PageZoomProperties.MAXIMUM_BAR_VALUE));
            }
        } else if (PageZoomProperties.DECREASE_ZOOM_CALLBACK == propertyKey) {
            view.findViewById(R.id.page_zoom_decrease_zoom_button)
                    .setOnClickListener(
                            v -> {
                                model.get(PageZoomProperties.DECREASE_ZOOM_CALLBACK).run();
                                model.get(PageZoomProperties.USER_INTERACTION_CALLBACK)
                                        .onResult(null);
                            });
        } else if (PageZoomProperties.INCREASE_ZOOM_CALLBACK == propertyKey) {
            view.findViewById(R.id.page_zoom_increase_zoom_button)
                    .setOnClickListener(
                            v -> {
                                model.get(PageZoomProperties.INCREASE_ZOOM_CALLBACK).run();
                                model.get(PageZoomProperties.USER_INTERACTION_CALLBACK)
                                        .onResult(null);
                            });
        } else if (PageZoomProperties.RESET_ZOOM_CALLBACK == propertyKey) {
            view.findViewById(R.id.page_zoom_reset_zoom_button)
                    .setOnClickListener(
                            v -> {
                                model.get(PageZoomProperties.RESET_ZOOM_CALLBACK).run();
                                model.get(PageZoomProperties.USER_INTERACTION_CALLBACK)
                                        .onResult(null);
                            });
        } else if (PageZoomProperties.DECREASE_ZOOM_ENABLED == propertyKey) {
            view.findViewById(R.id.page_zoom_decrease_zoom_button)
                    .setEnabled(model.get(PageZoomProperties.DECREASE_ZOOM_ENABLED));
        } else if (PageZoomProperties.INCREASE_ZOOM_ENABLED == propertyKey) {
            view.findViewById(R.id.page_zoom_increase_zoom_button)
                    .setEnabled(model.get(PageZoomProperties.INCREASE_ZOOM_ENABLED));
        } else if (PageZoomProperties.BAR_VALUE_CHANGE_CALLBACK == propertyKey) {
            if (model.get(PageZoomProperties.USE_SLIDER)) {
                slider.addOnChangeListener(
                        (s, value, fromUser) -> {
                            if (fromUser) {
                                model.get(PageZoomProperties.BAR_VALUE_CHANGE_CALLBACK)
                                        .onResult((int) value);
                                model.get(PageZoomProperties.USER_INTERACTION_CALLBACK)
                                        .onResult(null);
                            }
                        });
            } else {
                seekBar.setOnSeekBarChangeListener(
                        new SeekBar.OnSeekBarChangeListener() {
                            @Override
                            public void onProgressChanged(
                                    SeekBar seekBar, int progress, boolean fromUser) {
                                if (fromUser) {
                                    model.get(PageZoomProperties.BAR_VALUE_CHANGE_CALLBACK)
                                            .onResult(progress);
                                    model.get(PageZoomProperties.USER_INTERACTION_CALLBACK)
                                            .onResult(null);
                                }
                            }

                            @Override
                            public void onStartTrackingTouch(SeekBar seekBar) {}

                            @Override
                            public void onStopTrackingTouch(SeekBar seekBar) {}
                        });
            }
        } else if (PageZoomProperties.USE_SLIDER == propertyKey) {
            boolean useSlider = model.get(PageZoomProperties.USE_SLIDER);
            slider.setVisibility(useSlider ? View.VISIBLE : View.GONE);
            seekBar.setVisibility(useSlider ? View.GONE : View.VISIBLE);
        }
    }
}
