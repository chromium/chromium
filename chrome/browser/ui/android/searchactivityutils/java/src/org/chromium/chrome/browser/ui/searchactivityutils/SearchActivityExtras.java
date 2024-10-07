// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.ui.searchactivityutils;

import androidx.annotation.IntDef;

import java.lang.annotation.Retention;
import java.lang.annotation.RetentionPolicy;

/** Definitions of SearchActivity extras and values. */
public @interface SearchActivityExtras {
    /** The {@link IntentOrigin} specifies the origin of an Intent. */
    String EXTRA_ORIGIN = "org.chromium.chrome.browser.ui.searchactivityutils.origin";

    /** The Intent requested {@link SearchType}. */
    String EXTRA_SEARCH_TYPE = "org.chromium.chrome.browser.ui.searchactivityutils.search_type";

    /** The URL associated with the Intent to provide the context for suggestions. */
    String EXTRA_CURRENT_URL = "org.chromium.chrome.browser.ui.searchactivityutils.current_url";

    /** The package name (String) on behalf of which the search was requested. */
    String EXTRA_REFERRER = "org.chromium.chrome.browser.ui.searchactivityutils.referrer";

    // Only alpha-numeric characters, including dots and dashes.
    // Must be at least 2 characters long, and begin and end with an alphanumeric character.
    String REFERRER_VALIDATION_REGEX = "^[a-zA-Z0-9][a-zA-Z0-9._-]*[a-zA-Z0-9]$";

    /** The incognito status (boolean) associated with the origin activity. */
    String EXTRA_IS_INCOGNITO = "org.chromium.chrome.browser.ui.searchactivityutils.is_incognito";

    // LINT.IfChange(IntentOrigin)
    /** ID of the calling component */
    @IntDef({
        IntentOrigin.UNKNOWN,
        IntentOrigin.SEARCH_WIDGET,
        IntentOrigin.QUICK_ACTION_SEARCH_WIDGET,
        IntentOrigin.CUSTOM_TAB,
        IntentOrigin.HUB,
        IntentOrigin.COUNT
    })
    @Retention(RetentionPolicy.SOURCE)
    public @interface IntentOrigin {
        /** Calling component is unknown or unspecified. */
        int UNKNOWN = 0;

        /** Calling component is old SearchWidget. */
        int SEARCH_WIDGET = 1;

        /** Calling component is QuickActionSearchWidget. */
        int QUICK_ACTION_SEARCH_WIDGET = 2;

        /** Calling component is Chrome Custom Tab. */
        int CUSTOM_TAB = 3;

        /** Calling component is Hub. */
        int HUB = 4;

        /** Total count of items, used for histogram recording. */
        int COUNT = 5;
    }

    // LINT.ThenChange(//tools/metrics/histograms/metadata/android/enums.xml)

    /** The requested typ of service. */
    @IntDef({SearchType.TEXT, SearchType.VOICE, SearchType.LENS, SearchType.COUNT})
    @Retention(RetentionPolicy.SOURCE)
    public @interface SearchType {
        /** Regular text search / Omnibox aided Search. */
        int TEXT = 0;

        /** Voice search. */
        int VOICE = 1;

        /** Search with Lens. */
        int LENS = 2;

        /** Total count of items, used for histogram recording. */
        int COUNT = 3;
    }
}
