// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.components.language;
import org.chromium.base.annotations.NativeMethods;

/**
 * A bridge to language metrics functions that require access to native code.
 */
public class AndroidLanguageMetricsBridge {
    /**
     * Called when a user adds or removes a language from the list of languages they
     * can read using the Explicit Language Ask prompt at 2nd run.
     * @param language The language code that was added or removed from the list.
     * @param added True if the language was added, false if it was removed.
     */
    public static void reportExplicitLanguageAskStateChanged(String language, boolean added) {
        AndroidLanguageMetricsBridgeJni.get().reportExplicitLanguageAskStateChanged(
                language, added);
    }

    @NativeMethods
    interface Natives {
        void reportExplicitLanguageAskStateChanged(String language, boolean added);
    }
}
