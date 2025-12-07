// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.components.browser_ui.accessibility;

import static org.chromium.build.NullUtil.assumeNonNull;
import static org.chromium.content_public.browser.HostZoomMap.AVAILABLE_ZOOM_FACTORS;

import android.content.Context;
import android.util.AttributeSet;
import android.util.TypedValue;
import android.view.View;
import android.widget.ImageView;
import android.widget.LinearLayout;
import android.widget.TextView;

import androidx.preference.Preference;
import androidx.preference.PreferenceViewHolder;

import org.chromium.build.annotations.Initializer;
import org.chromium.build.annotations.NullMarked;
import org.chromium.build.annotations.Nullable;
import org.chromium.components.browser_ui.accessibility.AccessibilitySettingsDelegate.IntegerPreferenceDelegate;
import org.chromium.content_public.browser.ContentFeatureList;
import org.chromium.content_public.browser.ContentFeatureMap;
import org.chromium.ui.widget.ChromeImageButton;

/** Abstract base class for the page zoom section of Accessibility Settings. */
@NullMarked
public abstract class PageZoomPreference extends Preference {
    protected int mInitialValue;
    protected ChromeImageButton mDecreaseButton;
    protected ChromeImageButton mIncreaseButton;
    protected TextView mCurrentValueText;

    protected @Nullable ChromeImageButton mTextSizeContrastDecreaseButton;
    protected @Nullable ChromeImageButton mTextSizeContrastIncreaseButton;
    protected @Nullable TextView mTextSizeContrastCurrentLevelText;
    protected @Nullable IntegerPreferenceDelegate mTextSizeContrastDelegate;
    protected static final int TEXT_SIZE_CONTRAST_BUTTON_INCREMENT = 10;

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

    public PageZoomPreference(Context context, @Nullable AttributeSet attrs) {
        super(context, attrs);
    }

    @Initializer
    @Override
    public void onBindViewHolder(PreferenceViewHolder holder) {
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

        // Set up Page Zoom controls.
        mCurrentValueText = (TextView) holder.findViewById(R.id.page_zoom_current_value_text);
        mCurrentValueText.setText(resources.getString(R.string.page_zoom_level, 100));

        mDecreaseButton =
                (ChromeImageButton) holder.findViewById(R.id.page_zoom_decrease_zoom_button);
        mDecreaseButton.setOnClickListener(v -> onHandleDecreaseClicked());

        mIncreaseButton =
                (ChromeImageButton) holder.findViewById(R.id.page_zoom_increase_zoom_button);
        mIncreaseButton.setOnClickListener(v -> onHandleIncreaseClicked());

        // Subclasses must initialize their specific Slider/seekbar.
        initializeControls(holder);

        mCurrentMultiplier = mInitialValue;
        updateViews(getCurrentZoomValue(), true);

        // Set up text size contrast slider.
        if (ContentFeatureMap.isEnabled(ContentFeatureList.SMART_ZOOM)) {
            holder.findViewById(R.id.text_size_contrast_section).setVisibility(View.VISIBLE);

            mTextSizeContrastCurrentLevelText =
                    (TextView) holder.findViewById(R.id.text_size_contrast_current_value_text);
            mTextSizeContrastCurrentLevelText.setText(
                    resources.getString(R.string.text_size_contrast_level, 0));

            mTextSizeContrastDecreaseButton =
                    (ChromeImageButton)
                            holder.findViewById(R.id.text_size_contrast_decrease_zoom_button);
            assumeNonNull(mTextSizeContrastDecreaseButton);
            mTextSizeContrastDecreaseButton.setOnClickListener(
                    v -> onHandleContrastDecreaseClicked());
            mTextSizeContrastDecreaseButton.setVisibility(View.VISIBLE);

            mTextSizeContrastIncreaseButton =
                    (ChromeImageButton)
                            holder.findViewById(R.id.text_size_contrast_increase_zoom_button);
            assumeNonNull(mTextSizeContrastIncreaseButton);
            mTextSizeContrastIncreaseButton.setOnClickListener(
                    v -> onHandleContrastIncreaseClicked());
            mTextSizeContrastIncreaseButton.setVisibility(View.VISIBLE);

            initializeContrastControl(holder);

            assumeNonNull(mTextSizeContrastDelegate);
            mTextSizeContrastFactor = mTextSizeContrastDelegate.getValue();
            setCurrentContrastValue(mTextSizeContrastFactor);
            updateViews(mTextSizeContrastFactor, false);
        }
    }

    /**
     * Set initial zoom level.
     *
     * @param zoomLevel int - existing user pref value for zoom level (or default).
     */
    public void setInitialValue(int zoomLevel) {
        mInitialValue = zoomLevel;
    }

    /**
     * Set a delegate for the text size contrast slider functionality.
     *
     * @param delegate IntegerPreferenceDelegate - embedder's instance of a preference delegate.
     */
    public void setTextSizeContrastDelegate(IntegerPreferenceDelegate delegate) {
        mTextSizeContrastDelegate = delegate;
    }

    protected void updateViews(int progress, boolean isZoom) {
        updateZoomPercentageText(progress, isZoom);
        updatePreviewWidget(progress, isZoom);
        updateButtonStates(progress, isZoom);
    }

    private void updateZoomPercentageText(int progress, boolean isZoom) {
        if (isZoom) {
            int zoomLevel =
                    (int) Math.round(100 * PageZoomUtils.convertBarValueToZoomLevel(progress));
            mCurrentValueText.setText(getContext().getString(R.string.page_zoom_level, zoomLevel));
        } else {
            assumeNonNull(mTextSizeContrastCurrentLevelText);
            mTextSizeContrastCurrentLevelText.setText(
                    getContext().getString(R.string.text_size_contrast_level, progress));
        }
    }

    private void updatePreviewWidget(int progress, boolean isZoom) {
        if (isZoom) {
            mCurrentMultiplier = (float) PageZoomUtils.convertBarValueToZoomLevel(progress);
        } else {
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

    private void updateButtonStates(int progress, boolean isZoom) {
        if (isZoom) {
            double newZoomFactor = PageZoomUtils.convertBarValueToZoomFactor(progress);

            // If the new zoom factor is greater than the minimum zoom factor, enable decrease
            // button.
            mDecreaseButton.setEnabled(newZoomFactor > AVAILABLE_ZOOM_FACTORS[0]);

            // If the new zoom factor is less than the maximum zoom factor, enable increase button.
            mIncreaseButton.setEnabled(
                    newZoomFactor < AVAILABLE_ZOOM_FACTORS[AVAILABLE_ZOOM_FACTORS.length - 1]);
        } else {
            assumeNonNull(mTextSizeContrastDecreaseButton);
            assumeNonNull(mTextSizeContrastIncreaseButton);
            mTextSizeContrastDecreaseButton.setEnabled(progress > 0);
            mTextSizeContrastIncreaseButton.setEnabled(
                    progress < PageZoomUtils.TEXT_SIZE_CONTRAST_MAX_LEVEL);
        }
    }

    private void onHandleDecreaseClicked() {
        // When decreasing zoom, "snap" to the greatest preset value that is less than the current.
        double currentZoomFactor = PageZoomUtils.convertBarValueToZoomFactor(getCurrentZoomValue());
        int index = PageZoomUtils.getNextIndex(true, currentZoomFactor);

        if (index >= 0) {
            int barValue = PageZoomUtils.convertZoomFactorToBarValue(AVAILABLE_ZOOM_FACTORS[index]);
            setCurrentZoomValue(barValue);
            updateViews(barValue, true);
            mCurrentValueText.setAccessibilityLiveRegion(View.ACCESSIBILITY_LIVE_REGION_POLITE);
            callChangeListener(barValue);
        }
    }

    private void onHandleIncreaseClicked() {
        // When increasing zoom, "snap" to the smallest preset value that is more than the current.
        double currentZoomFactor = PageZoomUtils.convertBarValueToZoomFactor(getCurrentZoomValue());
        int index = PageZoomUtils.getNextIndex(false, currentZoomFactor);

        if (index <= AVAILABLE_ZOOM_FACTORS.length - 1) {
            int barValue = PageZoomUtils.convertZoomFactorToBarValue(AVAILABLE_ZOOM_FACTORS[index]);
            setCurrentZoomValue(barValue);
            updateViews(barValue, true);
            mCurrentValueText.setAccessibilityLiveRegion(View.ACCESSIBILITY_LIVE_REGION_POLITE);
            callChangeListener(barValue);
        }
    }

    private void onHandleContrastDecreaseClicked() {
        // Decrease the contrast slider by defined increment.
        int newValue = getCurrentContrastValue() - TEXT_SIZE_CONTRAST_BUTTON_INCREMENT;
        setCurrentContrastValue(newValue);
        updateViews(newValue, false);
        saveTextSizeContrastValueToPreferences();
    }

    private void onHandleContrastIncreaseClicked() {
        // Increase the contrast slider by defined increment.
        int newValue = getCurrentContrastValue() + TEXT_SIZE_CONTRAST_BUTTON_INCREMENT;
        setCurrentContrastValue(newValue);
        updateViews(newValue, false);
        saveTextSizeContrastValueToPreferences();
    }

    protected void saveTextSizeContrastValueToPreferences() {
        assumeNonNull(mTextSizeContrastDelegate);
        mTextSizeContrastDelegate.setValue(getCurrentContrastValue());
    }

    // Abstract methods for subclasses to implement.
    protected abstract void initializeControls(PreferenceViewHolder holder);

    protected abstract void initializeContrastControl(PreferenceViewHolder holder);

    protected abstract int getCurrentZoomValue();

    protected abstract void setCurrentZoomValue(int value);

    protected abstract int getCurrentContrastValue();

    protected abstract void setCurrentContrastValue(int value);

    protected abstract void setZoomValueForTesting(int value);

    protected abstract void setTextContrastValueForTesting(int contrast);
}
