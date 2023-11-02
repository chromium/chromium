// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.video_tutorials;

/**
 * Class encapsulating data needed to show a language on the language selection UI.
 */
public class Language {
    /** The locale associated with the language. */
    public final String locale;

    /** The name of the language. */
    public final String name;

    /** The name of the language in native text. */
    public final String nativeName;

    /** Constructor */
    public Language(String locale, String name, String nativeName) {
        this.locale = locale;
        this.name = name;
        this.nativeName = nativeName;
    }
}
