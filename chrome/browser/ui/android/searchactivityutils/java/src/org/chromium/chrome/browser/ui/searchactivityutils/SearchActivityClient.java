// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.ui.searchactivityutils;

import android.content.Context;
import android.content.Intent;

import androidx.annotation.IntDef;
import androidx.annotation.NonNull;
import androidx.annotation.Nullable;

import org.chromium.url.GURL;

import java.lang.annotation.Retention;
import java.lang.annotation.RetentionPolicy;

/**
 * Interface to use to interact with the SearchActivity as a Client.
 *
 * <p>TODO(crbug/329663295): Utils classes aren't usually instantiable; this is a temporary measure
 * to expose certain functionality while we try to migrate the entire class into an accessible
 * location. Explore the feasibility of relocating the logic directly here.
 */
public interface SearchActivityClient {
    /** ID of the calling component */
    @IntDef({
        IntentOrigin.UNKNOWN,
        IntentOrigin.SEARCH_WIDGET,
        IntentOrigin.QUICK_ACTION_SEARCH_WIDGET,
        IntentOrigin.CUSTOM_TAB
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
    }

    /** The requested typ of service. */
    @IntDef({SearchType.TEXT, SearchType.VOICE, SearchType.LENS})
    @Retention(RetentionPolicy.SOURCE)
    public @interface SearchType {
        /** Regular text search / Omnibox aided Search. */
        int TEXT = 0;

        /** Voice search. */
        int VOICE = 1;

        /** Search with Lens. */
        int LENS = 2;
    }

    /**
     * Construct an intent starting SearchActivity that opens Chrome browser.
     *
     * @param context current context
     * @param origin the ID of component requesting service
     * @param url the URL associated with the service
     * @param searchType the service type
     */
    public Intent createIntent(
            @NonNull Context context,
            @IntentOrigin int origin,
            @Nullable GURL url,
            @SearchType int searchType);
}
