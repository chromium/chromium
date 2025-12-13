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

/**
 * Custom preference for the page zoom section of Accessibility Settings.
 *
 * <p>TODO(crbug.com/439911511): Legacy preference.
 */
@NullMarked
public class PageZoomSeekbarPreference extends PageZoomPreference
        implements SeekBar.OnSeekBarChangeListener {
    private @Nullable SeekBar mSeekBar;
    private @Nullable SeekBar mTextSizeContrastSeekBar;

    public PageZoomSeekbarPreference(Context context, @Nullable AttributeSet attrs) {
        super(context, attrs);
        setLayoutResource(R.layout.page_zoom_legacy_preference);
    }

    @Override
    protected void initializeControls(PreferenceViewHolder holder) {
        mSeekBar = (SeekBar) holder.findViewById(R.id.page_zoom_slider_legacy);
        assumeNonNull(mSeekBar);
        mSeekBar.setVisibility(View.VISIBLE);
        mSeekBar.setOnSeekBarChangeListener(this);
        mSeekBar.setMax(PageZoomUtils.PAGE_ZOOM_MAXIMUM_BAR_VALUE);
        mSeekBar.setProgress(mInitialValue);
    }

    @Override
    protected void initializeContrastControl(PreferenceViewHolder holder) {
        mTextSizeContrastSeekBar =
                (SeekBar) holder.findViewById(R.id.text_size_contrast_slider_legacy);
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
        // mSeekBar can be null if this method is called before the preference is bound to a view.
        if (mSeekBar == null) return;
        mSeekBar.setProgress(value);
    }

    @Override
    protected int getCurrentContrastValue() {
        if (mTextSizeContrastSeekBar == null) return 0;
        return mTextSizeContrastSeekBar.getProgress();
    }

    @Override
    protected void setCurrentContrastValue(int value) {
        if (mTextSizeContrastSeekBar == null) return;
        mTextSizeContrastSeekBar.setProgress(value);
    }

    @Override
    public void onProgressChanged(SeekBar seekBar, int progress, boolean fromUser) {
        if (fromUser) {
            boolean isZoom = seekBar.getId() == R.id.page_zoom_slider_legacy;
            updateViews(progress, isZoom);
        }
    }

    @Override
    public void onStartTrackingTouch(SeekBar seekBar) {}

    @Override
    public void onStopTrackingTouch(SeekBar seekBar) {
        if (seekBar.getId() == R.id.page_zoom_slider_legacy) {
            callChangeListener(seekBar.getProgress());
        } else {
            saveTextSizeContrastValueToPreferences();
        }
    }

    // Testing methods.

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
}
