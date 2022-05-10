// Copyright 2022 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chromecast.shell;

import android.app.Service;
import android.content.Intent;
import android.os.IBinder;

/**
 * This Service allows launching the Cast browser module through an Intent. When the service is
 * created, the browser main loop will start. This allows launching the browser module separately
 * from the base module, so that the memory overhead of the browser is only incurred when needed.
 */
public class CastBrowserService extends Service {
    @Override
    public void onCreate() {}

    @Override
    public IBinder onBind(Intent intent) {
        CastBrowserHelper.initializeBrowser(getApplicationContext(), intent);
        return null;
    }

    @Override
    public boolean onUnbind(Intent intent) {
        return true;
    }
}
