// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.components.browser_ui.accessibility;

import static org.chromium.content_public.browser.HostZoomMap.AVAILABLE_ZOOM_FACTORS;

import android.content.Context;
import android.util.AttributeSet;
import android.util.TypedValue;
import android.view.View;
import android.widget.ImageView;
import android.widget.LinearLayout;
import android.widget.SeekBar;
import android.widget.TextView;

import androidx.annotation.NonNull;
import androidx.annotation.Nullable;
import androidx.preference.Preference;
import androidx.preference.PreferenceViewHolder;

import org.chromium.components.browser_ui.accessibility.AccessibilitySettingsDelegate.IntegerPreferenceDelegate;
import org.chromium.content_public.browser.ContentFeatureList;
import org.chromium.content_public.browser.ContentFeatureMap;
import org.chromium.ui.widget.ChromeImageButton;

/** Custom preference for the page zoom section of Accessibility Settings. */
public class PageZoomPreference extends Preference implements SeekBar.OnSeekBarChangeListener {
    private int mInitialValue;
    private SeekBar mSeekBar;
    private ChromeImageButton mDecreaseButton;
    private ChromeImageButton mIncreaseButton;
    private TextView mCurrentValueText;

    private SeekBar mTextSizeContrastSeekBar;
    private ChromeImageButton mTextSizeContrastDecreaseButton;
    private ChromeImageButton mTextSizeContrastIncreaseButton;
    private TextView mTextSizeContrastCurrentLevelText;
    private IntegerPreferenceDelegate mTextSizeContrastDelegate;
    private static final int TEXT_SIZE_CONTRAST_BUTTON_INCREMENT = 10;

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

    private float mCurrentMultiplier;
    private int mTextSizeContrastFactor;

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

        var resources = getContext().getResources();
        mDefaultPreviewImageSize =
                resources.getDimensionPixelSize(R.dimen.page_zoom_preview_image_size);
        mPreviewImage = (ImageView) holder.findViewById(R.id.page_zoom_preview_image);
        mPreviewImageParams =
                new LinearLayout.LayoutParams(mPreviewImage.getWidth(), mPreviewImage.getHeight());

        // Set up Page Zoom slider.
        mCurrentValueText = (TextView) holder.findViewById(R.id.page_zoom_current_value_text);
        mCurrentValueText.setText(resources.getString(R.string.page_zoom_level, 100));

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
        mCurrentMultiplier = mInitialValue;
        updateViewsOnProgressChanged(mInitialValue, mSeekBar);

        // Set up text size contrast slider.
        if (ContentFeatureMap.isEnabled(ContentFeatureList.SMART_ZOOM)) {
            holder.findViewById(R.id.text_size_contrast_title).setVisibility(View.VISIBLE);
            holder.findViewById(R.id.text_size_contrast_summary).setVisibility(View.VISIBLE);
            holder.findViewById(R.id.text_size_contrast_layout_container)
                    .setVisibility(View.VISIBLE);

            mTextSizeContrastCurrentLevelText =
                    (TextView) holder.findViewById(R.id.text_size_contrast_current_value_text);
            mTextSizeContrastCurrentLevelText.setText(
                    resources.getString(R.string.text_size_contrast_level, 0));
            mTextSizeContrastCurrentLevelText.setVisibility(View.VISIBLE);

            mTextSizeContrastDecreaseButton =
                    (ChromeImageButton)
                            holder.findViewById(R.id.text_size_contrast_decrease_zoom_button);
            mTextSizeContrastDecreaseButton.setOnClickListener(
                    v -> onHandleContrastDecreaseClicked());
            mTextSizeContrastDecreaseButton.setVisibility(View.VISIBLE);

            mTextSizeContrastIncreaseButton =
                    (ChromeImageButton)
                            holder.findViewById(R.id.text_size_contrast_increase_zoom_button);
            mTextSizeContrastIncreaseButton.setOnClickListener(
                    v -> onHandleContrastIncreaseClicked());
            mTextSizeContrastIncreaseButton.setVisibility(View.VISIBLE);

            mTextSizeContrastSeekBar =
                    (SeekBar) holder.findViewById(R.id.text_size_contrast_slider);
            mTextSizeContrastSeekBar.setOnSeekBarChangeListener(this);
            mTextSizeContrastSeekBar.setMax(PageZoomUtils.TEXT_SIZE_CONTRAST_MAX_LEVEL);
            mTextSizeContrastFactor = mTextSizeContrastDelegate.getValue();
            mTextSizeContrastSeekBar.setProgress(mTextSizeContrastFactor);
            mTextSizeContrastSeekBar.setVisibility(View.VISIBLE);
            updateViewsOnProgressChanged(mTextSizeContrastFactor, mTextSizeContrastSeekBar);
        }
    }

    /**
     * Initial values to set the progress of seekbars.
     * @param zoomLevel int - existing user pref value for zoom level (or default).
     */
    public void setInitialValue(int zoomLevel) {
        mInitialValue = zoomLevel;
    }

    /**
     * Set a delegate for the text size contrast slider functionality.
     * @param delegate IntegerPreferenceDelegate - embedder's instance of a preference delegate.
     */
    public void setTextSizeContrastDelegate(IntegerPreferenceDelegate delegate) {
        mTextSizeContrastDelegate = delegate;
    }

    private void updateViewsOnProgressChanged(int progress, SeekBar seekBar) {
        updateZoomPercentageText(progress, seekBar);
        updatePreviewWidget(progress, seekBar);
        updateButtonStates(progress, seekBar);
    }

    private void updateZoomPercentageText(int progress, SeekBar seekBar) {
        if (seekBar.getId() == R.id.page_zoom_slider) {
            int zoomLevel =
                    (int) Math.round(100 * PageZoomUtils.convertSeekBarValueToZoomLevel(progress));
            mCurrentValueText.setText(
                    getContext().getResources().getString(R.string.page_zoom_level, zoomLevel));
        } else if (seekBar.getId() == R.id.text_size_contrast_slider) {
            mTextSizeContrastCurrentLevelText.setText(
                    getContext()
                            .getResources()
                            .getString(R.string.text_size_contrast_level, progress));
        }
    }

    private void updatePreviewWidget(int progress, SeekBar seekBar) {
        if (seekBar.getId() == R.id.page_zoom_slider) {
            mCurrentMultiplier = (float) PageZoomUtils.convertSeekBarValueToZoomLevel(progress);
        } else if (seekBar.getId() == R.id.text_size_contrast_slider) {
            mTextSizeContrastFactor = progress;
        }

        mPreviewLargeText.setTextSize(
                TypedValue.COMPLEX_UNIT_SP, calculateAdjustedTextSize(DEFAULT_LARGE_TEXT_SIZE_SP));
        mPreviewMediumText.setTextSize(
                TypedValue.COMPLEX_UNIT_SP, calculateAdjustedTextSize(DEFAULT_MEDIUM_TEXT_SIZE_SP));
        mPreviewSmallText.setTextSize(
                TypedValue.COMPLEX_UNIT_SP, calculateAdjustedTextSize(DEFAULT_SMALL_TEXT_SIZE_SP));

        // TODO(crbug.com/40919531): Edit preview images when smart image sizing is added.
        mPreviewImageParams.width = (int) (mDefaultPreviewImageSize * mCurrentMultiplier);
        mPreviewImageParams.height = (int) (mDefaultPreviewImageSize * mCurrentMultiplier);
        mPreviewImage.setLayoutParams(mPreviewImageParams);
    }

    private float calculateAdjustedTextSize(float startingTextSize) {
        // TODO(crbug.com/40919531): Add the following non-linear formula
        if (mTextSizeContrastFactor > 0
                && ContentFeatureMap.isEnabled(ContentFeatureList.SMART_ZOOM)) {}
        return startingTextSize * mCurrentMultiplier;
    }

    private void updateButtonStates(int progress, SeekBar seekBar) {
        if (seekBar.getId() == R.id.page_zoom_slider) {
            double newZoomFactor = PageZoomUtils.convertSeekBarValueToZoomFactor(progress);

            // If the new zoom factor is greater than the minimum zoom factor, enable decrease
            // button.
            mDecreaseButton.setEnabled(newZoomFactor > AVAILABLE_ZOOM_FACTORS[0]);

            // If the new zoom factor is less than the maximum zoom factor, enable increase button.
            mIncreaseButton.setEnabled(
                    newZoomFactor < AVAILABLE_ZOOM_FACTORS[AVAILABLE_ZOOM_FACTORS.length - 1]);
        } else if (seekBar.getId() == R.id.text_size_contrast_slider) {
            mTextSizeContrastDecreaseButton.setEnabled(progress > 0);
            mTextSizeContrastIncreaseButton.setEnabled(
                    progress < PageZoomUtils.TEXT_SIZE_CONTRAST_MAX_LEVEL);
        }
    }

    @Override
    public void onProgressChanged(SeekBar seekBar, int progress, boolean fromUser) {
        // Update the zoom percentage text, preview widget and enabled state of increase/decrease
        // buttons as the slider is updated.
        updateViewsOnProgressChanged(progress, seekBar);
    }

    @Override
    public void onStartTrackingTouch(SeekBar seekBar) {}

    @Override
    public void onStopTrackingTouch(SeekBar seekBar) {
        if (seekBar.getId() == R.id.page_zoom_slider) {
            // When a user stops changing the slider value, record the new value in prefs.
            callChangeListener(seekBar.getProgress());
        } else if (seekBar.getId() == R.id.text_size_contrast_slider) {
            saveTextSizeContrastValueToPreferences();
        }
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

    private void onHandleContrastDecreaseClicked() {
        // Decrease the contrast slider by defined increment.
        mTextSizeContrastSeekBar.setProgress(
                mTextSizeContrastSeekBar.getProgress() - TEXT_SIZE_CONTRAST_BUTTON_INCREMENT);
        saveTextSizeContrastValueToPreferences();
    }

    private void onHandleContrastIncreaseClicked() {
        // Increase the contrast slider by defined increment.
        mTextSizeContrastSeekBar.setProgress(
                mTextSizeContrastSeekBar.getProgress() + TEXT_SIZE_CONTRAST_BUTTON_INCREMENT);
        saveTextSizeContrastValueToPreferences();
    }

    private void saveTextSizeContrastValueToPreferences() {
        mTextSizeContrastDelegate.setValue(mTextSizeContrastSeekBar.getProgress());
    }

    // Testing methods.

    public SeekBar getZoomSliderForTesting() {
        return mSeekBar;
    }

    public void setZoomValueForTesting(int progress) {
        mSeekBar.setProgress(progress);
        updateViewsOnProgressChanged(progress, mSeekBar);
    }

    public SeekBar getTextSizeContrastSliderForTesting() {
        return mTextSizeContrastSeekBar;
    }

    public void setTextContrastValueForTesting(int contrast) {
        mTextSizeContrastSeekBar.setProgress(contrast);
        updateViewsOnProgressChanged(contrast, mTextSizeContrastSeekBar);
    }
}
