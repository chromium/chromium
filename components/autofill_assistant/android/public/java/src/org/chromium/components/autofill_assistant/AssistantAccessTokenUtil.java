// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.components.autofill_assistant;

import android.accounts.Account;

import org.chromium.components.signin.identitymanager.IdentityManager;

/**
 * Utility class for retrieving and invalidating access tokens. Implementations might differ
 * depending on where Autofill Assistant is running (e.g. WebLayer, Chrome).
 */
public interface AssistantAccessTokenUtil {
    /**
     * Call this method to retrieve an OAuth2 access token for the given account.
     */
    void getAccessToken(Account account, IdentityManager.GetAccessTokenCallback callback);

    /**
     * Invalidates an OAuth2 token.
     */
    void invalidateAccessToken(String accessToken);
}
