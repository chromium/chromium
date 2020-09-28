// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.video_tutorials.languages;

import android.content.res.Resources;
import android.text.TextUtils;

import org.chromium.chrome.browser.video_tutorials.R;

/**
 * Utility class that provides methods for converting between locales and languages.
 */
public class LanguageUtils {
    private static final String LOCALE_HINDI = "hi-IN";
    private static final String LOCALE_TAMIL = "ta-IN";
    private static final String LOCALE_TELUGU = "te-IN";
    private static final String LOCALE_KANNADA = "kn-IN";
    private static final String LOCALE_ENGLISH = "en-IN";

    /** For a given locale, returns the name of the language in the native text. */
    public static String getLanguageForLocaleInNativeText(Resources resources, String locale) {
        if (TextUtils.equals(locale, LOCALE_HINDI)) {
            return resources.getString(R.string.video_tutorials_language_hindi_native_text);
        } else if (TextUtils.equals(locale, LOCALE_TAMIL)) {
            return resources.getString(R.string.video_tutorials_language_tamil_native_text);
        } else if (TextUtils.equals(locale, LOCALE_TELUGU)) {
            return resources.getString(R.string.video_tutorials_language_telugu_native_text);
        } else if (TextUtils.equals(locale, LOCALE_KANNADA)) {
            return resources.getString(R.string.video_tutorials_language_kannada_native_text);
        } else if (TextUtils.equals(locale, LOCALE_ENGLISH)) {
            return resources.getString(R.string.video_tutorials_language_english_native_text);
        }
        return null;
    }

    /** For a given locale, returns the name of the language in the system text. */
    public static String getLanguageForLocale(Resources resources, String locale) {
        if (TextUtils.equals(locale, LOCALE_HINDI)) {
            return resources.getString(R.string.video_tutorials_language_hindi);
        } else if (TextUtils.equals(locale, LOCALE_TAMIL)) {
            return resources.getString(R.string.video_tutorials_language_tamil);
        } else if (TextUtils.equals(locale, LOCALE_TELUGU)) {
            return resources.getString(R.string.video_tutorials_language_telugu);
        } else if (TextUtils.equals(locale, LOCALE_KANNADA)) {
            return resources.getString(R.string.video_tutorials_language_kannada);
        } else if (TextUtils.equals(locale, LOCALE_ENGLISH)) {
            return resources.getString(R.string.video_tutorials_language_english);
        }
        return null;
    }
}
