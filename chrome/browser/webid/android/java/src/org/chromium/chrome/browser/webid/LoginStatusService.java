// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.webid;

import android.app.Service;
import android.content.Intent;
import android.os.Binder;
import android.os.IBinder;

import org.chromium.build.annotations.NullMarked;
import org.chromium.build.annotations.Nullable;

/** Android Bound Service accepting login status updates from external native IdP applications. */
@NullMarked
public class LoginStatusService extends Service {
    private static final String TAG = "LoginStatusSvc";

    public static class LoginStatusBinder extends Binder {
        public boolean setLoginStatus(String status, String origin) {
            // Phase 1 implementation: always returns true.
            // Subsequent CLs will verify DAL and update storage.
            return true;
        }
    }

    private final IBinder mBinder = new LoginStatusBinder();

    @Override
    public @Nullable IBinder onBind(Intent intent) {
        return mBinder;
    }
}
