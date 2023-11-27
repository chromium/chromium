// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.net.impl;

import static org.chromium.net.impl.HttpEngineNativeProvider.EXT_API_LEVEL;
import static org.chromium.net.impl.HttpEngineNativeProvider.EXT_VERSION;

import androidx.annotation.RequiresExtension;

import java.util.List;
import java.util.Map;

@RequiresExtension(extension = EXT_API_LEVEL, version = EXT_VERSION)
class AndroidUrlResponseInfoWrapper extends org.chromium.net.UrlResponseInfo {
    private final android.net.http.UrlResponseInfo mBackend;
    /* org.chromium.net.UrlRequest and org.chromium.net.BidirectionalStream map the direct or
     * no-proxy scenarios to different values of org.chromium.net.UrlResponseInfo#getProxyServer:
     * UrlRequest will return ":0", while BidirectionalStream will return a null string.
     * This variable is needed to maintain compatibility with this non-documented behavior, as
     * android.net.http.UrlResponseInfo doesn't expose a getProxyServer method.
     * TODO(b/309121551): Clean this up.
     */
    private final String mProxyServerCompat;

    private AndroidUrlResponseInfoWrapper(
            android.net.http.UrlResponseInfo backend, String proxyServerCompat) {
        this.mBackend = backend;
        this.mProxyServerCompat = proxyServerCompat;
    }

    // See mProxyServerCompat's Javadoc.
    public static AndroidUrlResponseInfoWrapper createForUrlRequest(
            android.net.http.UrlResponseInfo backend) {
        // From //components/cronet/cronet_url_request.cc's GetProxy.
        return new AndroidUrlResponseInfoWrapper(backend, ":0");
    }

    // See mProxyServerCompat's Javadoc.
    public static AndroidUrlResponseInfoWrapper createForBidirectionalStream(
            android.net.http.UrlResponseInfo backend) {
        // From
        // //components/cronet/android/java/src/org/chromium/net/impl/CronetBidirectionalStream.java
        // prepareResponseInfoOnNetworkThread.
        return new AndroidUrlResponseInfoWrapper(backend, null);
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
        return mProxyServerCompat;
    }

    @Override
    public long getReceivedByteCount() {
        return mBackend.getReceivedByteCount();
    }
}
