// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.net.impl;

import androidx.annotation.RequiresApi;

import java.util.List;
import java.util.Map;
import java.util.Map.Entry;

@RequiresApi(api = 34)
class AndroidHeaderBlockWrapper extends org.chromium.net.UrlResponseInfo.HeaderBlock {
    private final android.net.http.HeaderBlock mBackend;

    AndroidHeaderBlockWrapper(android.net.http.HeaderBlock backend) {
        this.mBackend = backend;
    }

    @Override
    public List<Entry<String, String>> getAsList() {
        return mBackend.getAsList();
    }

    @Override
    public Map<String, List<String>> getAsMap() {
        return mBackend.getAsMap();
    }
}
