// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.components.browser_ui.accessibility;

import static org.chromium.build.NullUtil.assumeNonNull;

import android.content.Context;
import android.graphics.Color;
import android.util.AttributeSet;
import android.view.View;

import androidx.annotation.NonNull;
import androidx.preference.PreferenceViewHolder;

import com.google.android.material.slider.Slider;

import org.chromium.build.annotations.NullMarked;
import org.chromium.build.annotations.Nullable;
import org.chromium.components.browser_ui.widget.containment.ContainmentItem;

/** Custom preference for the page zoom section of Accessibility Settings. */
@NullMarked
public class PageZoomSliderPreference extends PageZoomPreference implements ContainmentItem {
    private @Nullable Slider mSlider;
    private @Nullable Slider mTextSizeContrastSlider;

    public PageZoomSliderPreference(Context context, @Nullable AttributeSet attrs) {
        super(context, attrs);
        setLayoutResource(R.layout.page_zoom_preference);
    }

    @Override
    protected void initializeControls(PreferenceViewHolder holder) {
        mDecreaseButton.setBackgroundColor(Color.TRANSPARENT);
        mIncreaseButton.setBackgroundColor(Color.TRANSPARENT);

        mSlider = (Slider) holder.findViewById(R.id.page_zoom_slider);
        assumeNonNull(mSlider);
        mSlider.setVisibility(View.VISIBLE);
        mSlider.setValueFrom(0);
        mSlider.setValueTo(PageZoomUtils.PAGE_ZOOM_MAXIMUM_BAR_VALUE);
        mSlider.setValue(mInitialValue);
        mSlider.addOnChangeListener(
                (slider, value, fromUser) -> {
                    if (fromUser) {
                        updateViews((int) value, true);
                    }
                });
        mSlider.addOnSliderTouchListener(
                new Slider.OnSliderTouchListener() {
                    @Override
                    public void onStartTrackingTouch(@NonNull Slider slider) {}

                    @Override
                    public void onStopTrackingTouch(@NonNull Slider slider) {
                        callChangeListener((int) slider.getValue());
                    }
                });
    }

    @Override
    protected void initializeContrastControl(PreferenceViewHolder holder) {
        assumeNonNull(mTextSizeContrastDecreaseButton);
        mTextSizeContrastDecreaseButton.setBackgroundColor(Color.TRANSPARENT);
        assumeNonNull(mTextSizeContrastIncreaseButton);
        mTextSizeContrastIncreaseButton.setBackgroundColor(Color.TRANSPARENT);

        mTextSizeContrastSlider = (Slider) holder.findViewById(R.id.text_size_contrast_slider);
        assumeNonNull(mTextSizeContrastSlider);
        mTextSizeContrastSlider.setVisibility(View.VISIBLE);
        mTextSizeContrastSlider.setValueFrom(0);
        mTextSizeContrastSlider.setValueTo(PageZoomUtils.TEXT_SIZE_CONTRAST_MAX_LEVEL);
        mTextSizeContrastSlider.addOnChangeListener(
                (slider, value, fromUser) -> {
                    if (fromUser) {
                        updateViews((int) value, false);
                    }
                });
        mTextSizeContrastSlider.addOnSliderTouchListener(
                new Slider.OnSliderTouchListener() {
                    @Override
                    public void onStartTrackingTouch(@NonNull Slider slider) {}

                    @Override
                    public void onStopTrackingTouch(@NonNull Slider slider) {
                        saveTextSizeContrastValueToPreferences();
                    }
                });
    }

    @Override
    protected int getCurrentZoomValue() {
        if (mSlider == null) return 0;
        return (int) mSlider.getValue();
    }

    @Override
    protected void setCurrentZoomValue(int value) {
        if (mSlider == null) return;
        mSlider.setValue(value);
    }

    @Override
    protected int getCurrentContrastValue() {
        if (mTextSizeContrastSlider == null) return 0;
        return (int) mTextSizeContrastSlider.getValue();
    }

    @Override
    protected void setCurrentContrastValue(int value) {
        if (mTextSizeContrastSlider == null) return;
        mTextSizeContrastSlider.setValue(value);
    }

    /**
     * Sets the zoom value for testing.
     *
     * @param value The zoom value to set.
     */
    @Override
    protected void setZoomValueForTesting(int value) {
        setCurrentZoomValue(value);
        updateViews(value, true);
    }

    /**
     * Sets the text contrast value for testing.
     *
     * @param contrast The text contrast value to set.
     */
    @Override
    protected void setTextContrastValueForTesting(int contrast) {
        setCurrentContrastValue(contrast);
        updateViews(contrast, false);
    }

    @Override
    public @BackgroundStyle int getCustomBackgroundStyle() {
        // This ensures the Preference itself doesn't have a background,
        // allowing the sub-sections to have their own custom background styles
        return BackgroundStyle.NONE;
    }
}
