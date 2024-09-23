// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.toolbar.top;

import android.text.TextUtils;

import androidx.annotation.NonNull;
import androidx.annotation.Nullable;
import androidx.annotation.VisibleForTesting;

import org.chromium.chrome.browser.flags.ChromeFeatureList;

/**
 * A best effort to give equality between two visual states of URL text. Contains a hint that should
 * be the visible text, but under edge cases this will be null, and the full URL will be fallen back
 * upon. This means callers need to be able to tolerate false negatives. Typically fine for uses
 * such as performance optimizations where this failure state is just an inefficiency.
 */
public class VisibleUrlText {
    private final @NonNull CharSequence mUrlText;
    private final @Nullable CharSequence mVisibleTextPrefixHint;

    public VisibleUrlText(@NonNull CharSequence urlText, @NonNull CharSequence prefixHint) {
        mUrlText = urlText;
        boolean isPrefixValid = isValidVisibleTextPrefixHint(urlText, prefixHint);
        mVisibleTextPrefixHint = isPrefixValid ? prefixHint : null;
    }

    @Override
    public boolean equals(@Nullable Object o) {
        if (this == o) {
            return true;
        }
        if (!(o instanceof VisibleUrlText)) {
            return false;
        }
        VisibleUrlText that = (VisibleUrlText) o;
        if (mVisibleTextPrefixHint != null
                && TextUtils.equals(mVisibleTextPrefixHint, that.mVisibleTextPrefixHint)) {
            return true;
        }

        return TextUtils.equals(mUrlText, that.mUrlText);
    }

    /**
     * Returns true if updating currentVisibleUrlText to nextVisibleUrlText won't cause a visible
     * difference.
     */
    public static boolean isVisuallyEquivalent(
            VisibleUrlText currentVisibleUrlText, VisibleUrlText nextVisibleUrlText) {
        // If this feature is enabled, then we only calculate the visible hint when navigating
        // to a url with the same domain. This means the first navigation will result in a null
        // visible hint and a screenshot being captured. If we navigate to another url with the same
        // domain, we calculate the visible hint, but we don't have to capture a screenshot (even
        // though the two VisibleUrlTexts aren't equal) if the visible hint is a prefix of the
        // previous url.
        if (ChromeFeatureList.sNoVisibleHintForDifferentTLD.isEnabled()
                && TextUtils.isEmpty(currentVisibleUrlText.mVisibleTextPrefixHint)
                && !TextUtils.isEmpty(nextVisibleUrlText.mVisibleTextPrefixHint)
                && TextUtils.indexOf(
                                currentVisibleUrlText.mUrlText,
                                nextVisibleUrlText.mVisibleTextPrefixHint)
                        == 0) {
            return true;
        }

        return currentVisibleUrlText.equals(nextVisibleUrlText);
    }

    /**
     * Determines the validity of the hint text given the passed in full text.
     * @param fullText The full text that should start with the hint.
     * @param hintText The hint text to be checked.
     * @return Whether the full text starts with the specified hint text.
     */
    @VisibleForTesting
    static boolean isValidVisibleTextPrefixHint(CharSequence fullText, CharSequence hintText) {
        if (fullText == null || TextUtils.isEmpty(hintText)) return false;
        if (hintText.length() > fullText.length()) return false;
        return TextUtils.indexOf(fullText, hintText, 0, hintText.length()) == 0;
    }
}
