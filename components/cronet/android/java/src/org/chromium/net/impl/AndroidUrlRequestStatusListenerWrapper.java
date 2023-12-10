// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.net.impl;

import static org.chromium.net.impl.HttpEngineNativeProvider.EXT_API_LEVEL;
import static org.chromium.net.impl.HttpEngineNativeProvider.EXT_VERSION;

import androidx.annotation.RequiresExtension;

@RequiresExtension(extension = EXT_API_LEVEL, version = EXT_VERSION)
@SuppressWarnings("Override")
class AndroidUrlRequestStatusListenerWrapper implements android.net.http.UrlRequest.StatusListener {
    private final org.chromium.net.UrlRequest.StatusListener mBackend;

    public AndroidUrlRequestStatusListenerWrapper(
            org.chromium.net.UrlRequest.StatusListener backend) {
        this.mBackend = backend;
    }

    @Override
    public void onStatus(int i) {
        mBackend.onStatus(i);
    }
}
