// Copyright 2014 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.net;

import android.content.Context;

import org.chromium.base.test.util.UrlUtils;

/** Helper class to install test files. */
public final class TestFilesInstaller {
    // Name of the asset directory in which test files are stored.
    private static final String TEST_FILE_ASSET_PATH = "components/cronet/testing/test_server/data";

    /** Installs test files if files have not been installed. */
    public static void installIfNeeded(Context context) {
        // Do nothing.
        // NOTE(pauljensen): This hook is used (overridden) when tests are run in other
        // configurations, so it should not be removed.
    }

    /** Returns the installed path of the test files. */
    public static String getInstalledPath(Context context) {
        return UrlUtils.getIsolatedTestRoot() + "/" + TEST_FILE_ASSET_PATH;
    }
}
