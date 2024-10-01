// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.test;

import org.junit.rules.TestRule;
import org.junit.runner.Description;
import org.junit.runners.model.Statement;

import org.chromium.chrome.test.util.browser.signin.SigninTestRule;
import org.chromium.components.signin.base.CoreAccountInfo;
import org.chromium.content_public.browser.test.NativeLibraryTestUtils;

/**
 * JUnit test rule that takes care of important initialization for Chrome-specific tests, such as
 * initializing the AccountManagerFacade.
 */
public class ChromeBrowserTestRule implements TestRule {
    private final SigninTestRule mSigninTestRule = new SigninTestRule();

    @Override
    public Statement apply(final Statement base, Description description) {
        Statement statement =
                new Statement() {
                    @Override
                    public void evaluate() throws Throwable {
                        // Loads the native library on the activity UI thread. After loading the
                        // library, this will initialize the browser process if necessary.
                        NativeLibraryTestUtils.loadNativeLibraryAndInitBrowserProcess();
                        base.evaluate();
                    }
                };
        return mSigninTestRule.apply(statement, description);
    }

    /** Adds an account of the given accountName to the fake AccountManagerFacade. */
    public CoreAccountInfo addAccount(String accountName) {
        return mSigninTestRule.addAccount(accountName);
    }

    /** Adds and signs in an account with the default name without sync consent. */
    public CoreAccountInfo addTestAccountThenSignin() {
        return mSigninTestRule.addTestAccountThenSignin();
    }

    /** Add and sign in an account with the default name. */
    public CoreAccountInfo addTestAccountThenSigninAndEnableSync() {
        return mSigninTestRule.addTestAccountThenSigninAndEnableSync();
    }
}
