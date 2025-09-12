// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.components.browser_ui.accessibility;

import static org.chromium.build.NullUtil.assumeNonNull;

import android.content.Context;
import android.util.AttributeSet;
import android.view.View;
import android.widget.SeekBar;

import androidx.preference.PreferenceViewHolder;

import org.chromium.build.annotations.NullMarked;
import org.chromium.build.annotations.Nullable;

/** Custom preference for the page zoom section of Accessibility Settings, using a SeekBar. */
@NullMarked
public class PageZoomSeekbarPreference extends PageZoomPreference
        implements SeekBar.OnSeekBarChangeListener {
    private @Nullable SeekBar mSeekBar;
    private @Nullable SeekBar mTextSizeContrastSeekBar;

    public PageZoomSeekbarPreference(Context context, @Nullable AttributeSet attrs) {
        super(context, attrs);
    }

    @Override
    protected void initializeControls(PreferenceViewHolder holder) {
        mSeekBar = (SeekBar) holder.findViewById(R.id.page_zoom_slider);
        assumeNonNull(mSeekBar);
        mSeekBar.setVisibility(View.VISIBLE);
        mSeekBar.setOnSeekBarChangeListener(this);
        mSeekBar.setMax(PageZoomUtils.PAGE_ZOOM_MAXIMUM_BAR_VALUE);
        mSeekBar.setProgress(mInitialValue);
    }

    @Override
    protected void initializeContrastControl(PreferenceViewHolder holder) {
        mTextSizeContrastSeekBar = (SeekBar) holder.findViewById(R.id.text_size_contrast_slider);
        assumeNonNull(mTextSizeContrastSeekBar);
        mTextSizeContrastSeekBar.setVisibility(View.VISIBLE);
        mTextSizeContrastSeekBar.setOnSeekBarChangeListener(this);
        mTextSizeContrastSeekBar.setMax(PageZoomUtils.TEXT_SIZE_CONTRAST_MAX_LEVEL);
    }

    @Override
    protected int getCurrentZoomValue() {
        assumeNonNull(mSeekBar);
        return mSeekBar.getProgress();
    }

    @Override
    protected void setCurrentZoomValue(int value) {
        assumeNonNull(mSeekBar);
        mSeekBar.setProgress(value);
    }

    @Override
    protected int getCurrentContrastValue() {
        assumeNonNull(mTextSizeContrastSeekBar);
        return mTextSizeContrastSeekBar.getProgress();
    }

    @Override
    protected void setCurrentContrastValue(int value) {
        assumeNonNull(mTextSizeContrastSeekBar);
        mTextSizeContrastSeekBar.setProgress(value);
    }

    @Override
    public void onProgressChanged(SeekBar seekBar, int progress, boolean fromUser) {
        if (fromUser) {
            boolean isZoom = seekBar.getId() == R.id.page_zoom_slider;
            updateViews(progress, isZoom);
        }
    }

    @Override
    public void onStartTrackingTouch(SeekBar seekBar) {}

    @Override
    public void onStopTrackingTouch(SeekBar seekBar) {
        if (seekBar.getId() == R.id.page_zoom_slider) {
            callChangeListener(seekBar.getProgress());
        } else {
            saveTextSizeContrastValueToPreferences();
        }
    }

    // Testing methods.

    /**
     * Returns the zoom seekbar for testing.
     *
     * @return The zoom seekbar.
     */
    SeekBar getZoomSliderForTesting() {
        assumeNonNull(mSeekBar);
        return mSeekBar;
    }

    /**
     * Sets the zoom value for testing.
     *
     * @param progress The zoom value to set.
     */
    void setZoomValueForTesting(int progress) {
        assumeNonNull(mSeekBar);
        mSeekBar.setProgress(progress);
        updateViews(progress, true);
    }

    /**
     * Returns the text size contrast seekbar for testing.
     *
     * @return The text size contrast seekbar.
     */
    @Nullable SeekBar getTextSizeContrastSliderForTesting() {
        return mTextSizeContrastSeekBar;
    }

    /**
     * Sets the text contrast value for testing.
     *
     * @param contrast The text contrast value to set.
     */
    void setTextContrastValueForTesting(int contrast) {
        assumeNonNull(mTextSizeContrastSeekBar);
        mTextSizeContrastSeekBar.setProgress(contrast);
        updateViews(contrast, false);
    }
}
