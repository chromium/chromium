// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.components.browser_ui.accessibility;

import static org.chromium.content_public.browser.HostZoomMap.AVAILABLE_ZOOM_FACTORS;

import android.content.Context;
import android.util.AttributeSet;
import android.util.TypedValue;
import android.widget.ImageView;
import android.widget.LinearLayout;
import android.widget.SeekBar;
import android.widget.TextView;

import androidx.annotation.NonNull;
import androidx.annotation.Nullable;
import androidx.preference.Preference;
import androidx.preference.PreferenceViewHolder;

import org.chromium.ui.widget.ChromeImageButton;

/**
 * Custom preference for the page zoom section of Accessibility Settings.
 */
public class PageZoomPreference extends Preference implements SeekBar.OnSeekBarChangeListener {
    private int mInitialValue;
    private SeekBar mSeekBar;
    private ChromeImageButton mDecreaseButton;
    private ChromeImageButton mIncreaseButton;
    private TextView mCurrentValueText;

    // Values taken from dimens of text_size_* in //ui/android/java/res/values/dimens.xml
    private static final float DEFAULT_LARGE_TEXT_SIZE_SP = 16.0f;
    private static final float DEFAULT_MEDIUM_TEXT_SIZE_SP = 14.0f;
    private static final float DEFAULT_SMALL_TEXT_SIZE_SP = 12.0f;

    private float mDefaultPreviewImageSize;
    private ImageView mPreviewImage;
    private LinearLayout.LayoutParams mPreviewImageParams;

    private TextView mPreviewLargeText;
    private TextView mPreviewMediumText;
    private TextView mPreviewSmallText;

    public PageZoomPreference(@NonNull Context context, @Nullable AttributeSet attrs) {
        super(context, attrs);

        setLayoutResource(R.layout.page_zoom_preference);
    }

    @Override
    public void onBindViewHolder(@NonNull PreferenceViewHolder holder) {
        super.onBindViewHolder(holder);

        // Re-use the main control layout, but remove extra padding and background.
        LinearLayout container = (LinearLayout) holder.findViewById(R.id.page_zoom_view_container);
        int top = container.getPaddingTop();
        int bot = container.getPaddingBottom();
        container.setBackground(null);
        container.setPadding(0, top, 0, bot);

        mPreviewLargeText = (TextView) holder.findViewById(R.id.page_zoom_preview_large_text);
        mPreviewMediumText = (TextView) holder.findViewById(R.id.page_zoom_preview_medium_text);
        mPreviewSmallText = (TextView) holder.findViewById(R.id.page_zoom_preview_small_text);

        mDefaultPreviewImageSize = getContext().getResources().getDimensionPixelSize(
                R.dimen.page_zoom_preview_image_size);
        mPreviewImage = (ImageView) holder.findViewById(R.id.page_zoom_preview_image);
        mPreviewImageParams =
                new LinearLayout.LayoutParams(mPreviewImage.getWidth(), mPreviewImage.getHeight());

        mCurrentValueText = (TextView) holder.findViewById(R.id.page_zoom_current_value_text);
        mCurrentValueText.setText(
                getContext().getResources().getString(R.string.page_zoom_level, 100));

        mDecreaseButton =
                (ChromeImageButton) holder.findViewById(R.id.page_zoom_decrease_zoom_button);
        mDecreaseButton.setOnClickListener(v -> onHandleDecreaseClicked());

        mIncreaseButton =
                (ChromeImageButton) holder.findViewById(R.id.page_zoom_increase_zoom_button);
        mIncreaseButton.setOnClickListener(v -> onHandleIncreaseClicked());

        mSeekBar = (SeekBar) holder.findViewById(R.id.page_zoom_slider);
        mSeekBar.setOnSeekBarChangeListener(this);
        mSeekBar.setMax(PageZoomUtils.PAGE_ZOOM_MAXIMUM_SEEKBAR_VALUE);
        mSeekBar.setProgress(mInitialValue);
        updateViewsOnProgressChanged(mInitialValue);
    }

    /**
     * Initial value to set the progress of the seekbar.
     * @param value int - existing user pref value (or default).
     */
    public void setInitialValue(int value) {
        mInitialValue = value;
    }

    private void updateViewsOnProgressChanged(int progress) {
        updateZoomPercentageText(progress);
        updatePreviewWidget(progress);
        updateButtonStates(progress);
    }

    private void updateZoomPercentageText(int progress) {
        mCurrentValueText.setText(getContext().getResources().getString(R.string.page_zoom_level,
                Math.round(100 * PageZoomUtils.convertSeekBarValueToZoomLevel(progress))));
    }

    private void updatePreviewWidget(int progress) {
        float multiplier = (float) PageZoomUtils.convertSeekBarValueToZoomLevel(progress);

        mPreviewLargeText.setTextSize(
                TypedValue.COMPLEX_UNIT_SP, DEFAULT_LARGE_TEXT_SIZE_SP * multiplier);
        mPreviewMediumText.setTextSize(
                TypedValue.COMPLEX_UNIT_SP, DEFAULT_MEDIUM_TEXT_SIZE_SP * multiplier);
        mPreviewSmallText.setTextSize(
                TypedValue.COMPLEX_UNIT_SP, DEFAULT_SMALL_TEXT_SIZE_SP * multiplier);

        mPreviewImageParams.width = (int) (mDefaultPreviewImageSize * multiplier);
        mPreviewImageParams.height = (int) (mDefaultPreviewImageSize * multiplier);
        mPreviewImage.setLayoutParams(mPreviewImageParams);
    }

    private void updateButtonStates(int progress) {
        double newZoomFactor = PageZoomUtils.convertSeekBarValueToZoomFactor(progress);

        // If the new zoom factor is greater than the minimum zoom factor, enable decrease button.
        mDecreaseButton.setEnabled(newZoomFactor > AVAILABLE_ZOOM_FACTORS[0]);

        // If the new zoom factor is less than the maximum zoom factor, enable increase button.
        mIncreaseButton.setEnabled(
                newZoomFactor < AVAILABLE_ZOOM_FACTORS[AVAILABLE_ZOOM_FACTORS.length - 1]);
    }

    @Override
    public void onProgressChanged(SeekBar seekBar, int progress, boolean fromUser) {
        // Update the zoom percentage text, preview widget and enabled state of increase/decrease
        // buttons as the slider is updated.
        updateViewsOnProgressChanged(progress);
    }

    @Override
    public void onStartTrackingTouch(SeekBar seekBar) {}

    @Override
    public void onStopTrackingTouch(SeekBar seekBar) {
        // When a user stops changing the slider value, record the new value in prefs.
        callChangeListener(seekBar.getProgress());
    }

    private void onHandleDecreaseClicked() {
        // When decreasing zoom, "snap" to the greatest preset value that is less than the current.
        double currentZoomFactor =
                PageZoomUtils.convertSeekBarValueToZoomFactor(mSeekBar.getProgress());
        int index = PageZoomUtils.getNextIndex(true, currentZoomFactor);

        if (index >= 0) {
            int seekBarValue =
                    PageZoomUtils.convertZoomFactorToSeekBarValue(AVAILABLE_ZOOM_FACTORS[index]);
            mSeekBar.setProgress(seekBarValue);
            callChangeListener(seekBarValue);
        }
    }

    private void onHandleIncreaseClicked() {
        // When increasing zoom, "snap" to the smallest preset value that is more than the current.
        double currentZoomFactor =
                PageZoomUtils.convertSeekBarValueToZoomFactor(mSeekBar.getProgress());
        int index = PageZoomUtils.getNextIndex(false, currentZoomFactor);

        if (index <= AVAILABLE_ZOOM_FACTORS.length - 1) {
            int seekBarValue =
                    PageZoomUtils.convertZoomFactorToSeekBarValue(AVAILABLE_ZOOM_FACTORS[index]);
            mSeekBar.setProgress(seekBarValue);
            callChangeListener(seekBarValue);
        }
    }
}