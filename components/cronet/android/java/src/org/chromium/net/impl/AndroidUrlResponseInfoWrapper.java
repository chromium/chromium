// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.net.impl;

import androidx.annotation.RequiresApi;

import java.util.List;
import java.util.Map;

@RequiresApi(api = 34)
class AndroidUrlResponseInfoWrapper extends org.chromium.net.UrlResponseInfo {
    private final android.net.http.UrlResponseInfo mBackend;

    AndroidUrlResponseInfoWrapper(android.net.http.UrlResponseInfo backend) {
        this.mBackend = backend;
    }

    @Override
    public String getUrl() {
        return mBackend.getUrl();
    }

    @Override
    public List<String> getUrlChain() {
        return mBackend.getUrlChain();
    }

    @Override
    public int getHttpStatusCode() {
        return mBackend.getHttpStatusCode();
    }

    @Override
    public String getHttpStatusText() {
        return mBackend.getHttpStatusText();
    }

    @Override
    public List<Map.Entry<String, String>> getAllHeadersAsList() {
        return mBackend.getHeaders().getAsList();
    }

    @Override
    public Map<String, List<String>> getAllHeaders() {
        return mBackend.getHeaders().getAsMap();
    }

    @Override
    public boolean wasCached() {
        return mBackend.wasCached();
    }

    @Override
    public String getNegotiatedProtocol() {
        return mBackend.getNegotiatedProtocol();
    }

    @Override
    public String getProxyServer() {
        return null;
    }

    @Override
    public long getReceivedByteCount() {
        return mBackend.getReceivedByteCount();
    }
}
