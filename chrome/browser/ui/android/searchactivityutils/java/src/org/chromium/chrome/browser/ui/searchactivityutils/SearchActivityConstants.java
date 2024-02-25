// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.ui.searchactivityutils;

/** Constant Strings to be used by {@link SearchActivity} */
public class SearchActivityConstants {
    /** Intent Action indicating that the Intent should initiate Text search. */
    public static final String ACTION_START_TEXT_SEARCH =
            "org.chromium.chrome.browser.ui.searchactivityutils.ACTION_START_TEXT_SEARCH";

    /** Intent Action indicating that the Intent should initiate Search-Engine aided Text search. */
    public static final String ACTION_START_EXTENDED_TEXT_SEARCH =
            "org.chromium.chrome.browser.ui.searchactivityutils.ACTION_START_EXTENDED_TEXT_SEARCH";

    /** Intent Action indicating that the Intent should initiate Voice search. */
    public static final String ACTION_START_VOICE_SEARCH =
            "org.chromium.chrome.browser.ui.searchactivityutils.ACTION_START_VOICE_SEARCH";

    /**
     * Intent Action indicating that the Intent should initiate Search-Engine aided Voice search.
     */
    public static final String ACTION_START_EXTENDED_VOICE_SEARCH =
            "org.chromium.chrome.browser.ui.searchactivityutils.ACTION_START_EXTENDED_VOICE_SEARCH";

    /** Intent Action indicating that the Intent should initiate Lens assisted search. */
    public static final String ACTION_START_LENS_SEARCH =
            "org.chromium.chrome.browser.ui.searchactivityutils.ACTION_START_LENS_SEARCH";

    /** Extra value indicating that SearchActivity was launched from QuickActionSearchWidget. */
    public static final String EXTRA_BOOLEAN_FROM_QUICK_ACTION_SEARCH_WIDGET =
            "org.chromium.chrome.browser.ui.searchactivityutils.FROM_QUICK_ACTION_SEARCH_WIDGET";
}
