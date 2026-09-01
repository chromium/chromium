// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.toolbar.top;

import android.content.Context;
import android.content.res.ColorStateList;
import android.graphics.Color;
import android.util.AttributeSet;

import androidx.core.widget.ImageViewCompat;

import org.chromium.build.annotations.NullMarked;
import org.chromium.build.annotations.Nullable;
import org.chromium.chrome.browser.toolbar.R;
import org.chromium.ui.widget.ChromeImageButton;

/** Button view for the AI overlay microphone in the toolbar. */
@NullMarked
public class AiOverlayMicrophoneButtonView extends ChromeImageButton {

    // The default icon size in dp (matching ic_mic_white_24dp).
    private static final float BASE_ICON_SIZE_DP = 24f;
    // The maximum scale expansion in dp when audio energy is at its maximum (1.0).
    private static final float MAX_ENERGY_EXPANSION_DP = 10f;
    // Minimum energy threshold above which the microphone icon tints active red.
    private static final float ACTIVE_ENERGY_THRESHOLD = 0.01f;

    private float mAudioEnergy;
    private @Nullable ColorStateList mIconTint;

    public AiOverlayMicrophoneButtonView(Context context, @Nullable AttributeSet attrs) {
        this(context, attrs, 0);
    }

    public AiOverlayMicrophoneButtonView(
            Context context, @Nullable AttributeSet attrs, int defStyleAttr) {
        super(context, attrs, defStyleAttr);
        if (getDrawable() == null) {
            setImageResource(R.drawable.ic_mic_white_24dp);
        }
        updateTint();
    }

    @Override
    protected void onFinishInflate() {
        super.onFinishInflate();
        if (getDrawable() == null) {
            setImageResource(R.drawable.ic_mic_white_24dp);
        }
        updateTint();
    }

    /**
     * Updates the audio energy level to scale the microphone icon and adjust its tint.
     *
     * @param energy The audio energy level in [0.0, 1.0].
     */
    public void updateAudioEnergy(float energy) {
        mAudioEnergy = energy;
        float scale = (BASE_ICON_SIZE_DP + (energy * MAX_ENERGY_EXPANSION_DP)) / BASE_ICON_SIZE_DP;
        setScaleX(scale);
        setScaleY(scale);
        updateTint();
    }

    /**
     * Sets the tint for the icon when not in active audio state.
     *
     * @param tint The {@link ColorStateList} to use for tinting.
     */
    public void setIconTint(@Nullable ColorStateList tint) {
        mIconTint = tint;
        updateTint();
    }

    private void updateTint() {
        if (mAudioEnergy > ACTIVE_ENERGY_THRESHOLD) {
            ImageViewCompat.setImageTintList(this, ColorStateList.valueOf(Color.RED));
        } else if (mIconTint != null) {
            ImageViewCompat.setImageTintList(this, mIconTint);
        } else {
            ImageViewCompat.setImageTintList(this, ColorStateList.valueOf(Color.WHITE));
        }
    }
}
