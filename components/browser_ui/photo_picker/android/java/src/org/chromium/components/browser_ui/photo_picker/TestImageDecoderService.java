// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.components.browser_ui.photo_picker;

import android.app.Service;
import android.content.Intent;
import android.os.IBinder;

import org.chromium.base.library_loader.LibraryLoader;

/** A service to accept requests to take image file contents and decode them, used for tests. */
public class TestImageDecoderService extends Service {
    private final ImageDecoder mDecoder = new ImageDecoder();

    @Override
    public void onCreate() {
        LibraryLoader.getInstance().ensureInitialized();
        mDecoder.initializeSandbox();
    }

    @Override
    public IBinder onBind(Intent intent) {
        return mDecoder;
    }
}
