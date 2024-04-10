// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.components.data_sharing;

import org.chromium.base.UserDataHost;

/**
 * DataSharingService is the core class for managing data sharing. It represents a native
 * DataSharingService object in Java.
 */
public interface DataSharingService {
    /**
     * Whether the service is an empty implementation. This is here because the Chromium build
     * disables RTTI, and we need to be able to verify that we are using an empty service from the
     * Chrome embedder.
     *
     * @return Whether the service implementation is empty.
     */
    boolean isEmptyService();

    /** Returns the network loader for sending out network calls to backend services. */
    DataSharingNetworkLoader getNetworkLoader();

    /**
     * @return {@link UserDataHost} that manages {@link UserData} objects attached to.
     */
    UserDataHost getUserDataHost();
}
