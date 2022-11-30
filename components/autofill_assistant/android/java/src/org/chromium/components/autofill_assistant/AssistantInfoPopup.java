// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.components.autofill_assistant;

import android.content.Context;
import android.text.method.LinkMovementMethod;
import android.widget.TextView;

import androidx.annotation.Nullable;
import androidx.appcompat.app.AlertDialog;

import org.chromium.base.annotations.CalledByNative;
import org.chromium.base.annotations.JNINamespace;

/**
 * Represents a simple info popup.
 */
@JNINamespace("autofill_assistant")
public class AssistantInfoPopup {
    private final String mTitle;
    private final String mText;
    @Nullable
    private final AssistantDialogButton mPositiveButton;
    @Nullable
    private final AssistantDialogButton mNegativeButton;
    @Nullable
    private final AssistantDialogButton mNeutralButton;
    @Nullable
    private final AssistantTextLinkDelegate mTextLinkDelegate;

    @CalledByNative
    public AssistantInfoPopup(String title, String text,
            @Nullable AssistantDialogButton positiveButton,
            @Nullable AssistantDialogButton negativeButton,
            @Nullable AssistantDialogButton neutralButton,
            @Nullable AssistantTextLinkDelegate textLinkDelegate) {
        mTitle = title;
        mText = text;
        mPositiveButton = positiveButton;
        mNegativeButton = negativeButton;
        mNeutralButton = neutralButton;
        mTextLinkDelegate = textLinkDelegate;
    }

    public String getTitle() {
        return mTitle;
    }

    public String getText() {
        return mText;
    }

    @CalledByNative
    public void show(Context context) {
        AlertDialog.Builder builder =
                new AlertDialog
                        .Builder(context,
                                org.chromium.components.autofill_assistant.R.style
                                        .ThemeOverlay_BrowserUI_AlertDialog)
                        .setTitle(mTitle)
                        .setMessage(mTextLinkDelegate == null
                                        ? mText
                                        : AssistantTextUtils.applyVisualAppearanceTags(
                                                mText, context, (linkId) -> {
                                                    mTextLinkDelegate.onTextLinkClicked(linkId);
                                                }));

        if (mPositiveButton != null) {
            builder.setPositiveButton(mPositiveButton.getLabel(),
                    (dialog, which) -> mPositiveButton.onClick(context));
        }
        if (mNegativeButton != null) {
            builder.setNegativeButton(mNegativeButton.getLabel(),
                    (dialog, which) -> mNegativeButton.onClick(context));
        }
        if (mNeutralButton != null) {
            builder.setNeutralButton(
                    mNeutralButton.getLabel(), (dialog, which) -> mNeutralButton.onClick(context));
        }
        AlertDialog alertDialog = builder.create();
        alertDialog.show();
        if (mTextLinkDelegate != null) {
            ((TextView) alertDialog.findViewById(android.R.id.message))
                    .setMovementMethod(LinkMovementMethod.getInstance());
        }
    }
}
