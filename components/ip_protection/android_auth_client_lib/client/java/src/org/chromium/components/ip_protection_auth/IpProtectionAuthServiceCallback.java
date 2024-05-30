// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.components.ip_protection_auth;

import androidx.annotation.VisibleForTesting;

@VisibleForTesting
public interface IpProtectionAuthServiceCallback {
    /**
     * Called when IpProtectionAuthClient.CreateConnectedInstance has completed.
     *
     * @param client will not be null.
     */
    public void onResult(IpProtectionAuthClient client);

    // TODO(elburrito): use this function instead of throwing exceptions
    public void onError(String error);
}
