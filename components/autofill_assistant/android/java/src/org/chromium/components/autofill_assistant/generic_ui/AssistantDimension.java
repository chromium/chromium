// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.components.autofill_assistant.generic_ui;

import android.content.Context;
import android.util.TypedValue;

import org.chromium.base.annotations.CalledByNative;
import org.chromium.base.annotations.JNINamespace;

/** Contains methods to convert size dimensions to pixels. */
@JNINamespace("autofill_assistant")
public abstract class AssistantDimension {
    /** Converts from DP to pixels with respect to {@code context}. */
    @CalledByNative
    public static int getPixelSizeDp(Context context, int dp) {
        return Math.round(TypedValue.applyDimension(
                TypedValue.COMPLEX_UNIT_DIP, dp, context.getResources().getDisplayMetrics()));
    }

    /** Returns the size in pixels of {@code factor} * width of display. */
    @CalledByNative
    public static int getPixelSizeWidthFactor(Context context, float factor) {
        return Math.round(context.getResources().getDisplayMetrics().widthPixels * factor);
    }

    /** Returns the size in pixels of {@code factor} * height of display. */
    @CalledByNative
    public static int getPixelSizeHeightFactor(Context context, float factor) {
        return Math.round(context.getResources().getDisplayMetrics().heightPixels * factor);
    }
}
