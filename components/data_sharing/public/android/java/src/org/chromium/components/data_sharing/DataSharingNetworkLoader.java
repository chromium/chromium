// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.components.data_sharing;

import org.chromium.base.Callback;
import org.chromium.url.GURL;

/**
 * Class for managing network requests for data sharing service. It represents a native
 * DataSharingNetworkLoader object in Java.
 */
public interface DataSharingNetworkLoader {

    /**
     * Fetch data from the network. Callback will be invoked once the fetch completes.
     *
     * @param url URL to send the request.
     * @param scopes OAuth scopes of the request.
     * @param postData Data to be sent with the POST request.
     * @param requestType Network annotation tag of the request.
     * @param callback Callback to be invoked once the response is received.
     */
    void loadUrl(
            GURL url,
            String[] scopes,
            byte[] postData,
            @DataSharingRequestType int requestType,
            Callback<DataSharingNetworkResult> callback);
}
