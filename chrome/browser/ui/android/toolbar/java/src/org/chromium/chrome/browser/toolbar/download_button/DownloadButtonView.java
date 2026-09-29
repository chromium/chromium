// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.toolbar.download_button;

import android.content.Context;
import android.content.res.ColorStateList;
import android.util.AttributeSet;
import android.widget.FrameLayout;

import androidx.core.widget.ImageViewCompat;

import org.chromium.build.annotations.NullMarked;
import org.chromium.build.annotations.Nullable;
import org.chromium.chrome.browser.toolbar.R;
import org.chromium.chrome.browser.toolbar.top.ToolbarUtils;
import org.chromium.ui.widget.ChromeImageButton;

/**
 * View container hosting the download button in the toolbar. This container will host the download
 * progress ring and status animations alongside the action button.
 */
@NullMarked
public class DownloadButtonView extends FrameLayout {
    private ChromeImageButton mButton;

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
     * Sets the tint on the button icon.
     *
     * @param tint The color state list tint.
     */
    public void setTint(@Nullable ColorStateList tint) {
        if (tint != null) {
            ImageViewCompat.setImageTintList(mButton, tint);
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
}
