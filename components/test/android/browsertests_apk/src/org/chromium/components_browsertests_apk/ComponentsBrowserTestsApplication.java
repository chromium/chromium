// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.components_browsertests_apk;

import android.content.Context;

import org.chromium.base.PathUtils;
import org.chromium.native_test.NativeBrowserTestApplication;
import org.chromium.ui.base.ResourceBundle;

/**
 * A basic content_public.browser.tests {@link android.app.Application}.
 */
public class ComponentsBrowserTestsApplication extends NativeBrowserTestApplication {
    static final String PRIVATE_DATA_DIRECTORY_SUFFIX = "components_shell";

    @Override
    protected void attachBaseContext(Context base) {
        super.attachBaseContext(base);

        if (isBrowserProcess()) {
            // Test-only stuff, see also NativeUnitTest.java.
            ResourceBundle.setNoAvailableLocalePaks();
            PathUtils.setPrivateDataDirectorySuffix(PRIVATE_DATA_DIRECTORY_SUFFIX);
        }
    }
}
