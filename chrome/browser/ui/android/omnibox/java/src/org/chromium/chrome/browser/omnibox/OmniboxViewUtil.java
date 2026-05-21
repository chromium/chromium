// Copyright 2014 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.omnibox;

import org.jni_zero.NativeMethods;

import org.chromium.build.annotations.NullMarked;
import org.chromium.components.metrics.OmniboxEventProtos.OmniboxEventProto.PageClassification;

/** Utilities for the Omnibox view component. */
@NullMarked
public class OmniboxViewUtil {

    /**
     * Sanitizing the given string to be safe to paste into the omnibox.
     *
     * @param clipboardString The string from the clipboard.
     * @return The sanitized version of the string.
     */
    public static String sanitizeTextForPaste(String clipboardString) {
        return OmniboxViewUtilJni.get().sanitizeTextForPaste(clipboardString);
    }

    /**
     * Returns whether the given page classification represents a regular tab context (i.e., not the
     * Hub, Custom Tabs, or Co-Browsing Composebox).
     *
     * @param pageClassification The PageClassification value to check.
     * @return True if it is a regular tab context.
     */
    public static boolean isRegularTabContext(int pageClassification) {
        // TODO(crbug.com/507471408): Revisit logic to guard it more strictly.
        return pageClassification != PageClassification.ANDROID_HUB_VALUE
                && pageClassification != PageClassification.OTHER_ON_CCT_VALUE
                && pageClassification != PageClassification.CO_BROWSING_COMPOSEBOX_VALUE
                && pageClassification != PageClassification.ANDROID_SEARCH_WIDGET_VALUE
                && pageClassification != PageClassification.ANDROID_SHORTCUTS_WIDGET_VALUE;
    }

    @NativeMethods
    interface Natives {
        String sanitizeTextForPaste(String clipboardString);
    }
}
