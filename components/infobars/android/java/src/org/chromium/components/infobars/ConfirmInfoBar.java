// Copyright 2013 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.components.infobars;

import android.graphics.Bitmap;

import androidx.annotation.ColorRes;

import org.jni_zero.CalledByNative;
import org.jni_zero.JNINamespace;

import org.chromium.build.annotations.NullMarked;
import org.chromium.build.annotations.Nullable;
import org.chromium.chrome.browser.infobar.ActionType;

/**
 * An infobar that presents the user with several buttons.
 *
 * TODO(newt): merge this into InfoBar.java.
 */
@JNINamespace("infobars")
@NullMarked
public class ConfirmInfoBar extends InfoBar {
    /** Text shown on the primary button, e.g. "OK". */
    private final String mPrimaryButtonText;

    /** Text shown on the secondary button, e.g. "Cancel". */
    private final @Nullable String mSecondaryButtonText;

    /** Text shown on the link, e.g. "Learn more". */
    private final @Nullable String mLinkText;

    protected ConfirmInfoBar(
            int iconDrawableId,
            @ColorRes int iconTintId,
            @Nullable Bitmap iconBitmap,
            String message,
            @Nullable String linkText,
            String primaryButtonText,
            @Nullable String secondaryButtonText) {
        super(iconDrawableId, iconTintId, message, iconBitmap);
        mPrimaryButtonText = primaryButtonText;
        mSecondaryButtonText = secondaryButtonText;
        mLinkText = linkText;
    }

    @Override
    public void createContent(InfoBarLayout layout) {
        setButtons(layout, mPrimaryButtonText, mSecondaryButtonText);
        if (mLinkText != null && !mLinkText.isEmpty()) layout.appendMessageLinkText(mLinkText);
    }

    /**
     * If your custom infobar overrides this function, YOU'RE PROBABLY DOING SOMETHING WRONG.
     *
     * <p>Adds buttons to the infobar. This should only be overridden in cases where an infobar
     * requires adding something other than a button for its secondary View on the bottom row
     * (almost never).
     *
     * @param primaryText Text to display on the primary button.
     * @param secondaryText Text to display on the secondary button. May be null.
     */
    protected void setButtons(
            InfoBarLayout layout, String primaryText, @Nullable String secondaryText) {
        layout.setButtons(primaryText, secondaryText);
    }

    @Override
    public void onButtonClicked(final boolean isPrimaryButton) {
        int action = isPrimaryButton ? ActionType.OK : ActionType.CANCEL;
        onButtonClicked(action);
    }

    /**
     * Creates and begins the process for showing a ConfirmInfoBar.
     * @param iconId ID corresponding to the icon that will be shown for the infobar.
     * @param iconBitmap Bitmap to use if there is no equivalent Java resource for
     *                   iconId.
     * @param message Message to display to the user indicating what the infobar is for.
     * @param linkText Link text to display in addition to the message.
     * @param buttonOk String to display on the OK button.
     * @param buttonCancel String to display on the Cancel button.
     */
    @CalledByNative
    private static ConfirmInfoBar create(
            int iconId,
            Bitmap iconBitmap,
            String message,
            String linkText,
            String buttonOk,
            String buttonCancel) {
        ConfirmInfoBar infoBar =
                new ConfirmInfoBar(
                        iconId, 0, iconBitmap, message, linkText, buttonOk, buttonCancel);

        return infoBar;
    }
}
