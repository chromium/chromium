// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.android_browsertests_apk;

import android.content.Intent;

import org.chromium.base.test.util.UrlUtils;
import org.chromium.chrome.browser.ChromeTabbedActivity;
import org.chromium.content_public.browser.BrowserStartupController;
import org.chromium.native_test.NativeBrowserTest;
import org.chromium.native_test.NativeTest;

import java.io.File;

/**
 * Android activity for running chrome browser tests.
 */
public class ChromeBrowserTestsActivity extends ChromeTabbedActivity {
    private static final String TAG = "browser_test";

    private NativeTest mTest = new NativeTest();

    @Override
    public void performPreInflationStartup() {
        // These steps for NativeTest are usually performed in onCreate, but we can not
        // override onCreate in this class since a super class marks it as final. The
        // performPreInflationStartup() steps is another early step in initialization of the
        // activity so we do that here.
        mTest.preCreate(this);
        super.performPreInflationStartup();
        // Sets up the command line for tests.
        mTest.postCreate(this);
        // Append things we want for Android-based browser tests. C++ will also append things.
        for (String flag : NativeBrowserTest.BROWSER_TESTS_FLAGS) {
            mTest.appendCommandLineFlags(flag);
        }
        mTest.appendCommandLineFlags(
                "--remote-debugging-socket-name android_browsertests_devtools_remote");

        NativeBrowserTest.deletePrivateDataDirectory(getPrivateDataDirectory());

        // Replace ContentMain() with running our NativeTest suite.
        BrowserStartupController.getInstance().setContentMainCallbackForTests(() -> {
            // This jumps into C++ to set up and run the test harness. The test harness runs
            // ContentMain()-equivalent code, and then waits for javaStartupTasksComplete()
            // to be called. We delay that until finishNativeInitialization() is done which
            // marks the end of the startup tasks posted from C++ in ContentMain() and then
            // by Java in BrowserStartupControllerImpl::browserStartupComplete().
            mTest.postStart(this, false);
        });
    }

    /**
     * Tests don't use the preallocated child connection.
     */
    @Override
    public boolean shouldAllocateChildConnection() {
        return false;
    }

    /**
     * Tests should not go through the first run process every time.
     */
    @Override
    protected boolean requiresFirstRunToBeCompleted(Intent intent) {
        return false;
    }

    /**
     * This is the point at which Java initialization tasks are done and tests can be run.
     * While mTest.postStart() runs the test harness, it waits for Java initialization
     * tasks, and this signals that they are done.
     */
    @Override
    public void finishNativeInitialization() {
        super.finishNativeInitialization();
        NativeBrowserTest.javaStartupTasksComplete();
    }

    private File getPrivateDataDirectory() {
        // TODO(agrieve): We should not be touching the side-loaded test data directory.
        //     https://crbug.com/617734
        return new File(UrlUtils.getIsolatedTestRoot(),
                ChromeBrowserTestsApplication.PRIVATE_DATA_DIRECTORY_SUFFIX);
    }
}
