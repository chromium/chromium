// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.components.ip_protection_auth;

public interface IpProtectionAuthServiceCallback {
    /**
     * Called if/when IpProtectionAuthClient.CreateConnectedInstance has completed.
     *
     * <p>Will be called on the UI thread.
     *
     * @param client will not be null.
     */
    public void onResult(IpProtectionAuthClient client);

    /**
     * Called if/when IpProtectionAuthClient.CreateConnectedInstance fails.
     *
     * <p>Will be called on the UI thread.
     *
     * @param error unstructured description of error.
     */
    public void onError(String error);
}
