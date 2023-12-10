// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.test.pagecontroller.rules;

import androidx.test.core.app.ApplicationProvider;
import androidx.test.platform.app.InstrumentationRegistry;

import org.junit.rules.ExternalResource;

/** Test rule that provides access to a Chrome application. */
public class ChromeUiApplicationTestRule extends ExternalResource {
    public static final String PACKAGE_NAME_ARG =
            "org.chromium.chrome.test.pagecontroller.rules."
                    + "ChromeUiApplicationTestRule.PackageUnderTest";

    private String mPackageName;

    public String getApplicationPackage() {
        return mPackageName;
    }

    @Override
    protected void before() throws Throwable {
        super.before();
        mPackageName = InstrumentationRegistry.getArguments().getString(PACKAGE_NAME_ARG);
        // If the runner didn't pass the package name under test to us, then we can assume
        // that the target package name provided in the AndroidManifest is the app-under-test.
        if (mPackageName == null) {
            mPackageName = ApplicationProvider.getApplicationContext().getPackageName();
        }
    }
}
