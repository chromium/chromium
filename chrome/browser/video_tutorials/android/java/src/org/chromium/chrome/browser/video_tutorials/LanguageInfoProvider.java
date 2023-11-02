// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.video_tutorials;

/**
 * Interface for obtaining the language information for a given locale in order to display on the
 * UI.
 */
public interface LanguageInfoProvider {
    /**
x     * @return The language info for a given {@code locale}.
     */
    Language getLanguageInfo(String locale);
}
