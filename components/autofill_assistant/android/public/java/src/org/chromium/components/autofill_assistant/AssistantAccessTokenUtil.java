// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.components.autofill_assistant;

import android.accounts.Account;

import org.chromium.components.signin.identitymanager.IdentityManager;

/**
 * Utility interface for retrieving and invalidating access tokens. This is
 * legacy and can likely be removed - at the time of creation, this layer of
 * abstraction was needed to support different implementations between Chrome
 * and WebLayer.
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
