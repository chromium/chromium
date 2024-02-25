// Copyright 2014 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.omnibox;

import org.jni_zero.NativeMethods;

/** Utilities for the Omnibox view component. */
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

    @NativeMethods
    interface Natives {
        String sanitizeTextForPaste(String clipboardString);
    }
}
