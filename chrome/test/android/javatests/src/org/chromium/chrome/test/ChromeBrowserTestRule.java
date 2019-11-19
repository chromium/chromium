// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.test;

import org.junit.runner.Description;
import org.junit.runners.model.Statement;

import org.chromium.chrome.test.util.browser.signin.SigninTestUtil;
import org.chromium.content_public.browser.test.NativeLibraryTestRule;

/**
 * JUnit test rule that takes care of important initialization for Chrome-specific tests, such as
 * initializing the AccountManagerFacade.
 */
public class ChromeBrowserTestRule extends NativeLibraryTestRule {
    private void setUp() {
        SigninTestUtil.setUpAuthForTest();
        loadNativeLibraryAndInitBrowserProcess();
    }

    @Override
    public Statement apply(final Statement base, Description description) {
        return super.apply(new Statement() {
            @Override
            public void evaluate() throws Throwable {
                /**
                 * Loads the native library on the activity UI thread (must not be called from the
                 * UI thread).  After loading the library, this will initialize the browser process
                 * if necessary.
                 */
                setUp();
                try {
                    base.evaluate();
                } finally {
                    tearDown();
                }
            }
        }, description);
    }

    private void tearDown() {
        SigninTestUtil.tearDownAuthForTest();
    }
}
