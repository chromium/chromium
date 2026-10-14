// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.toolbar.download_button;

import android.content.Context;
import android.content.res.ColorStateList;
import android.graphics.Color;
import android.util.AttributeSet;
import android.widget.FrameLayout;

import androidx.annotation.ColorInt;
import androidx.core.graphics.ColorUtils;
import androidx.core.widget.ImageViewCompat;

import org.chromium.build.annotations.NullMarked;
import org.chromium.build.annotations.Nullable;
import org.chromium.chrome.browser.toolbar.R;
import org.chromium.chrome.browser.toolbar.download_button.DownloadProgressRingDrawable.RingStatus;
import org.chromium.chrome.browser.toolbar.top.ToolbarUtils;
import org.chromium.ui.widget.ChromeImageButton;

/**
 * View container hosting the download button in the toolbar, along with its progress ring and
 * download count badge.
 */
@NullMarked
public class DownloadButtonView extends FrameLayout {
    /** Alpha of the ring's background track relative to the icon tint. */
    private static final float RING_TRACK_ALPHA = 0.3f;

    private ChromeImageButton mButton;
    private DownloadProgressRingDrawable mRing;
    private @ColorInt int mBadgeBackgroundColor = Color.TRANSPARENT;

    /**
     * Constructor for inflating from XML.
     *
     * @param context The Android context.
     * @param attrs Attribute set from layout inflation.
     */
    public DownloadButtonView(Context context, @Nullable AttributeSet attrs) {
        super(context, attrs);
    }

    @Override
    protected void onFinishInflate() {
        super.onFinishInflate();
        mButton = findViewById(R.id.download_button);
        mRing = new DownloadProgressRingDrawable(getContext());
        // As the foreground, the ring gets the button's bounds and visibility callbacks for free,
        // draws above the icon, and stays out of the accessibility tree.
        mButton.setForeground(mRing);
    }

    /**
     * Returns the underlying {@link ChromeImageButton}.
     *
     * @return The inner button view.
     */
    public ChromeImageButton getButton() {
        return mButton;
    }

    /**
     * Sets the tint on the button icon. The progress ring and badge text use the same color, so
     * they stay in sync with the icon's active/inactive tint.
     *
     * @param tint The color state list tint.
     */
    public void setTint(@Nullable ColorStateList tint) {
        if (tint != null) {
            ImageViewCompat.setImageTintList(mButton, tint);
            updateRingColors();
        }
    }

    /**
     * Updates the button background ripple for incognito state.
     *
     * @param isIncognito True if incognito; false otherwise.
     */
    public void setIsIncognito(boolean isIncognito) {
        mButton.setBackgroundResource(ToolbarUtils.getToolbarIconRippleId(isIncognito));
    }

    /**
     * Sets the badge fill color, which should match the toolbar so the badge stands out from the
     * ring.
     *
     * @param color The badge background color.
     */
    public void setBadgeBackgroundColor(@ColorInt int color) {
        mBadgeBackgroundColor = color;
        updateRingColors();
    }

    /**
     * Sets how the progress ring is drawn.
     *
     * @param status The {@link RingStatus} to draw.
     * @param progressPercent Progress in [0, 100], used only when {@code status} is DOWNLOADING.
     */
    public void setRingStatus(@RingStatus int status, int progressPercent) {
        switch (status) {
            case RingStatus.IDLE:
                mRing.setIdle();
                break;
            case RingStatus.SCANNING:
                mRing.setScanning();
                break;
            case RingStatus.DOWNLOADING:
                mRing.setDownloading(progressPercent);
                break;
        }
    }

    /**
     * Sets the number shown in the badge.
     *
     * @param count The count to show, or 0 to hide the badge.
     */
    public void setBadgeCount(int count) {
        mRing.setBadgeCount(count);
    }

    private void updateRingColors() {
        // The layout always sets a tint, so this is only null if the view was built otherwise.
        ColorStateList tint = ImageViewCompat.getImageTintList(mButton);
        if (tint == null) return;
        int tintColor = tint.getDefaultColor();
        mRing.setColors(
                ColorUtils.setAlphaComponent(tintColor, Math.round(255 * RING_TRACK_ALPHA)),
                /* progressColor= */ tintColor,
                mBadgeBackgroundColor,
                /* badgeTextColor= */ tintColor);
    }

    void setRingForTesting(DownloadProgressRingDrawable ring) {
        mRing = ring;
        mButton.setForeground(ring);
    }
}
