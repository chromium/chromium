// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.android_browsertests_apk;

import android.content.Intent;
import android.net.Uri;

import org.chromium.build.gtest_apk.NativeTestInstrumentationTestRunner;
import org.chromium.content_public.common.ContentUrlConstants;

/** An Instrumentation for android_browsertests that includes chrome:blank in the intent. */
public class ChromeBrowserTestsInstrumentationTestRunner
        extends NativeTestInstrumentationTestRunner {
    @Override
    protected Intent createShardMainIntent() {
        Intent i = super.createShardMainIntent();
        i.setData(Uri.parse(ContentUrlConstants.ABOUT_BLANK_DISPLAY_URL));
        return i;
    }
}
