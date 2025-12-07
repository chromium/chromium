// Copyright 2015 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.components_browsertests_apk;

import android.os.Bundle;

import org.chromium.base.PathUtils;
import org.chromium.build.annotations.Nullable;
import org.chromium.content_shell.browsertests.ContentShellBrowserTestActivity;

import java.io.File;

/** Android activity for running components browser tests */
public class ComponentsBrowserTestsActivity extends ContentShellBrowserTestActivity {
    @Override
    public void onCreate(@Nullable Bundle savedInstanceState) {
        super.onCreate(savedInstanceState);
        appendCommandLineFlags(
                "--remote-debugging-socket-name components_browsertests_devtools_remote");
    }

    @Override
    protected File getPrivateDataDirectory() {
        return new File(
                PathUtils.getDataDirectory(),
                ComponentsBrowserTestsApplication.PRIVATE_DATA_DIRECTORY_SUFFIX);
    }

    @Override
    protected int getTestActivityViewId() {
        return R.layout.test_activity;
    }

    @Override
    protected int getShellManagerViewId() {
        return R.id.shell_container;
    }
}
