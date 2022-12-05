// Copyright 2014 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.net;

import android.app.Application;
import android.content.Context;
import android.os.Build;

import androidx.multidex.MultiDex;

/**
 * Application for managing the Cronet Test.
 */
public class CronetTestApplication extends Application {
    @Override
    protected void attachBaseContext(Context base) {
        super.attachBaseContext(base);

        // install multidex for Kitkat - crbug/1393424
        if (Build.VERSION.SDK_INT == Build.VERSION_CODES.KITKAT) {
            MultiDex.install(this);
        }
    }
}
