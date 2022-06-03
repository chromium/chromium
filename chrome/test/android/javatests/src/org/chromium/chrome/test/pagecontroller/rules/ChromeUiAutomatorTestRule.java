// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.test.pagecontroller.rules;

import android.content.Context;
import android.graphics.Bitmap;
import android.graphics.BitmapFactory;

import org.junit.rules.TestWatcher;
import org.junit.runner.Description;

import org.chromium.base.Log;
import org.chromium.chrome.test.pagecontroller.utils.UiAutomatorUtils;

import java.io.File;
import java.io.IOException;

/**
 * Custom Rule that logs useful information for debugging UiAutomator
 * related issues in the event of a test failure.
 */
public class ChromeUiAutomatorTestRule extends TestWatcher {
    private static final String TAG = "ChromeUiAutomatorTR";

    @Override
    protected void failed(Throwable e, Description description) {
        super.failed(e, description);
        Log.e(TAG, description.toString() + " failed", e);
        UiAutomatorUtils utils = UiAutomatorUtils.getInstance();
        utils.printWindowHierarchy("UI hierarchy when " + description.toString() + " failed");
    }

    /**
     * Creates a Bitmap from the UiAutomatorUtils (use this for RenderTest).
     */
    public Bitmap takeScreenshot(Context context) throws IOException {
        UiAutomatorUtils utils = UiAutomatorUtils.getInstance();

        File outputDir = context.getCacheDir();
        File outputFile = File.createTempFile("screenshot", "png", outputDir);
        outputFile.deleteOnExit();

        utils.takeScreenShot(outputFile);
        return BitmapFactory.decodeFile(outputFile.getAbsolutePath());
    }
}
