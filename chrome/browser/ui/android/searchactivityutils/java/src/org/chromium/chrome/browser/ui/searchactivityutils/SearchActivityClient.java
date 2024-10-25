// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.ui.searchactivityutils;

import android.app.Activity;
import android.content.Intent;

import androidx.annotation.NonNull;
import androidx.annotation.Nullable;

import org.chromium.chrome.browser.ui.searchactivityutils.SearchActivityExtras.ResolutionType;
import org.chromium.chrome.browser.ui.searchactivityutils.SearchActivityExtras.SearchType;
import org.chromium.content_public.browser.LoadUrlParams;
import org.chromium.url.GURL;

/**
 * Interface to use to interact with the SearchActivity as a Client.
 *
 * <p>TODO(crbug/329663295): Utils classes aren't usually instantiable; this is a temporary measure
 * to expose certain functionality while we try to migrate the entire class into an accessible
 * location. Explore the feasibility of relocating the logic directly here.
 */
public interface SearchActivityClient {
    /** Interface for building intents to request Omnibox-powered Search Activity. */
    public interface IntentBuilder {
        /**
         * Sets the search type for the intent. The default value is SearchType.TEXT.
         *
         * @param searchType The type of search to perform.
         * @return The IntentBuilder instance for method chaining.
         */
        IntentBuilder setSearchType(@SearchType int searchType);

        /**
         * Sets the page URL as a Search context. The default value is inferred from the
         * IntentOrigin, most typically pointing to the Default Search Engine landing page, e.g.
         * https://www.google.com.
         *
         * @param url The URL of the page.
         * @return The IntentBuilder instance for method chaining.
         */
        IntentBuilder setPageUrl(GURL url);

        /**
         * Sets the referrer for the intent. Referrer is the CCT host package name. The default
         * value is `null`, indicating no referrer package name.
         *
         * @param referrer The referrer string.
         * @return The IntentBuilder instance for method chaining.
         */
        IntentBuilder setReferrer(String referrer);

        /**
         * Sets whether the SearchActivity should be opened in incognito mode. The default value is
         * `false`, indicating regular profile.
         *
         * @param isIncognito True if the Search should be opened in incognito mode, false
         *     otherwise.
         * @return The IntentBuilder instance for method chaining.
         */
        IntentBuilder setIncognito(boolean isIncognito);

        /**
         * Sets the resolution type for the intent. The default resolution type is
         * ResolutionType.OPEN_IN_CHROME.
         *
         * @param resolutionType The type of resolution to use.
         * @return The IntentBuilder instance for method chaining.
         */
        IntentBuilder setResolutionType(@ResolutionType int resolutionType);

        /** Returns the intent capturing all the relevant details. */
        Intent build();
    }

    /** Construct an intent starting SearchActivity that opens Chrome browser. */
    public IntentBuilder newIntentBuilder();

    /**
     * Call up SearchActivity/Omnibox on behalf of the Activity the client is associated with.
     *
     * <p>Allows the caller to instantiate the Omnibox and retrieve Suggestions for the supplied
     * webpage. Response will be delivered via {@link Activity#onActivityResult}.
     *
     * @param intent The intent to send.
     */
    public void requestOmniboxForResult(Intent intent);

    /**
     * Utility method to determine whether the {@link Activity#onActivityResult} payload carries the
     * response to {@link requestOmniboxForResult}.
     *
     * @param requestCode the request code received in {@link Activity#onActivityResult}
     * @param intent the intent data received in {@link Activity#onActivityResult}
     * @return true if the response captures legitimate Omnibox result.
     */
    public boolean isOmniboxResult(int requestCode, @NonNull Intent intent);

    /**
     * Process the {@link Activity#onActivityResult} payload for Omnibox navigation result.
     *
     * @param requestCode the request code received in {@link Activity#onActivityResult}
     * @param resultCode the result code received in {@link Activity#onActivityResult}
     * @param intent the intent data received in {@link Activity#onActivityResult}
     * @return null, if result is not a valid Omnibox result, otherwise valid LoadUrlParams object
     */
    public @Nullable LoadUrlParams getOmniboxResult(
            int requestCode, int resultCode, @NonNull Intent intent);
}
