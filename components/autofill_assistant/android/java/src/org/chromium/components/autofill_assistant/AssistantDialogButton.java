// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.components.autofill_assistant;

import android.content.Context;

import androidx.annotation.Nullable;

import org.chromium.base.annotations.CalledByNative;
import org.chromium.base.annotations.JNINamespace;

/**
 * Represents a button.
 */
@JNINamespace("autofill_assistant")
public class AssistantDialogButton {
    private final AssistantInfoPageUtil mInfoPageUtil;
    private final String mLabel;
    @Nullable
    private final String mUrl;

    @CalledByNative
    public AssistantDialogButton(
            AssistantInfoPageUtil infoPageUtil, String label, @Nullable String url) {
        mInfoPageUtil = infoPageUtil;
        mLabel = label;
        mUrl = url;
    }

    public void onClick(Context context) {
        if (mUrl != null) {
            mInfoPageUtil.showInfoPage(context, mUrl);
        }
    }

    public String getLabel() {
        return mLabel;
    }
}
