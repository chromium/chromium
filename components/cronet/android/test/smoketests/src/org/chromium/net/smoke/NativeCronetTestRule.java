// Copyright 2016 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.net.smoke;

import androidx.test.core.app.ApplicationProvider;

import org.junit.runner.Description;
import org.junit.runners.model.Statement;

import org.chromium.base.ContextUtils;
import org.chromium.base.PathUtils;

import java.io.File;

/**
 * Test base class for testing native Engine implementation. This class can import classes from the
 * org.chromium.base package.
 */
public class NativeCronetTestRule extends CronetSmokeTestRule {
    private static final String PRIVATE_DATA_DIRECTORY_SUFFIX = "cronet_test";
    private static final String LOGFILE_NAME = "cronet-netlog.json";

    @Override
    public Statement apply(final Statement base, Description desc) {
        return super.apply(
                new Statement() {
                    @Override
                    public void evaluate() throws Throwable {
                        ruleSetUp();
                        base.evaluate();
                        ruleTearDown();
                    }
                },
                desc);
    }

    @Override
    public void initCronetEngine() {
        super.initCronetEngine();
        assertNativeEngine(mCronetEngine);
        startNetLog();
    }

    private void ruleSetUp() throws Exception {
        ContextUtils.initApplicationContext(ApplicationProvider.getApplicationContext());
        PathUtils.setPrivateDataDirectorySuffix(PRIVATE_DATA_DIRECTORY_SUFFIX);
        getTestSupport().loadTestNativeLibrary();
    }

    private void ruleTearDown() throws Exception {
        stopAndSaveNetLog();
    }

    private void startNetLog() {
        if (mCronetEngine != null) {
            mCronetEngine.startNetLogToFile(
                    PathUtils.getDataDirectory() + "/" + LOGFILE_NAME, false);
        }
    }

    private void stopAndSaveNetLog() {
        if (mCronetEngine == null) return;
        mCronetEngine.stopNetLog();
        File netLogFile = new File(PathUtils.getDataDirectory(), LOGFILE_NAME);
        if (!netLogFile.exists()) return;
        getTestSupport().processNetLog(ApplicationProvider.getApplicationContext(), netLogFile);
    }

    @Override
    protected TestSupport initTestSupport() {
        return new ChromiumNativeTestSupport();
    }
}
