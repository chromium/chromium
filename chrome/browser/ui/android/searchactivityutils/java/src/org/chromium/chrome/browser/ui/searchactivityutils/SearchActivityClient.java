// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.ui.searchactivityutils;

import android.content.Context;
import android.content.Intent;

import androidx.annotation.NonNull;
import androidx.annotation.Nullable;

import org.chromium.chrome.browser.ui.searchactivityutils.SearchActivityExtras.IntentOrigin;
import org.chromium.chrome.browser.ui.searchactivityutils.SearchActivityExtras.SearchType;
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
