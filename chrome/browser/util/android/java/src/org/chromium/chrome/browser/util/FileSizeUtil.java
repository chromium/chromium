// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.util;

import android.annotation.TargetApi;
import android.content.Context;
import android.os.Build;
import android.text.Spannable;
import android.text.SpannableString;
import android.text.format.Formatter;
import android.text.style.TtsSpan;

/**
 * Helper functions for file size info that is shown to the user.
 */
public class FileSizeUtil {
    /**
     * Formats the file size and also sets TtsSpans of type TYPE_MEASURE so that "B" is announced
     * as "byte" by the TTS engine.
     * @see Formatter.formatFileSize
     */
    @TargetApi(Build.VERSION_CODES.LOLLIPOP)
    public static CharSequence formatFileSize(Context context, long bytes) {
        String phrase = Formatter.formatFileSize(context, bytes);
        // For some languages TTS does not speak "B" as "bytes", so the text is wrapped with a span.
        if (Build.VERSION.SDK_INT >= Build.VERSION_CODES.LOLLIPOP) {
            // Only add spans for numbers which will be displayed as bytes. KB, MB etc are spoken
            // correctly by the TTS.
            if (ConversionUtils.bytesToKilobytes(bytes) < 1) {
                TtsSpan ttsSpan =
                        new TtsSpan.MeasureBuilder().setNumber(bytes).setUnit("byte").build();
                Spannable phraseSpannable = new SpannableString(phrase);
                phraseSpannable.setSpan(ttsSpan, 0, phraseSpannable.length(), 0);
                return phraseSpannable;
            }
        }
        return phrase;
    }
}