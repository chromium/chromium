// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.browserservices;

import android.app.Activity;
import android.os.Bundle;

import androidx.annotation.Nullable;

/**
 * An Activity that accepts browsable intents to http://www.example.com/notifications. This is
 * required for {@link TestTrustedWebActivityService} to be found.
 */
public class ActivityWithDeepLink extends Activity {
    @Override
    protected void onCreate(@Nullable Bundle savedInstanceState) {
        super.onCreate(savedInstanceState);
        finish();
    }
}
