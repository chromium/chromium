// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.ui.searchactivityutils;

import android.app.Activity;
import android.content.Context;
import android.content.Intent;

import androidx.annotation.NonNull;
import androidx.annotation.Nullable;

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
    /**
     * Construct an intent starting SearchActivity that opens Chrome browser.
     *
     * @param context current context
     * @param url the URL associated with the service
     * @param searchType the service type
     */
    public Intent createIntent(
            @NonNull Context context, @Nullable GURL url, @SearchType int searchType);

    /**
     * Call up SearchActivity/Omnibox on behalf of the current Activity.
     *
     * <p>Allows the caller to instantiate the Omnibox and retrieve Suggestions for the supplied
     * webpage. Response will be delivered via {@link Activity#onActivityResult}.
     *
     * @param activity The current activity; may be {@code null}, in which case intent will not be
     *     issued.
     * @param url The URL of the page to retrieve suggestions for.
     * @param referrer The referrer package name.
     * @param isIncognito The incognito status of the current Activity.
     */
    public void requestOmniboxForResult(
            @Nullable Activity activity,
            @NonNull GURL currentUrl,
            @Nullable String referrer,
            boolean isIncognito);

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
