// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.net.impl;

import androidx.annotation.RequiresApi;

@RequiresApi(api = 34)
@SuppressWarnings("Override")
class UrlRequestStatusListenerWrapper implements android.net.http.UrlRequest.StatusListener {
    private final org.chromium.net.UrlRequest.StatusListener mBackend;

    public UrlRequestStatusListenerWrapper(org.chromium.net.UrlRequest.StatusListener backend) {
        this.mBackend = backend;
    }

    @Override
    public void onStatus(int i) {
        mBackend.onStatus(i);
    }
}
