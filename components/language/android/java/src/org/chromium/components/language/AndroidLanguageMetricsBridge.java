// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.components.language;

import androidx.annotation.VisibleForTesting;

import org.jni_zero.NativeMethods;

/** A bridge to language metrics functions that require access to native code. */
public class AndroidLanguageMetricsBridge {
    @VisibleForTesting
    public static final String OVERRIDE_LANGUAGE_HISTOGRAM =
            "LanguageUsage.UI.Android.OverrideLanguage";

    public static final String APP_LANGUAGE_PROMPT_HISTOGRAM =
            "LanguageSettings.AppLanguagePrompt.Language";

    /**
     * Report the app override language code in a sparse histogram.
     * @param language ISO-639 code of the app override language.
     */
    public static void reportAppOverrideLanguage(String language) {
        AndroidLanguageMetricsBridgeJni.get()
                .reportHashMetricName(OVERRIDE_LANGUAGE_HISTOGRAM, language);
    }

    /**
     * Report the language selected from the app language prompt.
     * @param language ISO-639 code of the selected app override language.
     */
    public static void reportAppLanguagePromptLanguage(String language) {
        AndroidLanguageMetricsBridgeJni.get()
                .reportHashMetricName(APP_LANGUAGE_PROMPT_HISTOGRAM, language);
    }

    @NativeMethods
    interface Natives {
        // report the hash of value in |histogramName| sparse histogram.
        void reportHashMetricName(String histogramName, String value);
    }
}
