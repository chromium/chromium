// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.test;

import org.junit.rules.TestRule;
import org.junit.runner.Description;
import org.junit.runners.model.Statement;

import org.chromium.base.Log;
import org.chromium.base.metrics.RecordHistogram;
import org.chromium.base.metrics.RecordUserAction;

/**
 * {@link TestRule} to disable native access for testing.
 */
public final class DisableNativeTestRule implements TestRule {
    private static final String TAG = "DisableNative";

    @Override
    public Statement apply(final Statement statement, final Description description) {
        return new Statement() {
            @Override
            public void evaluate() throws Throwable {
                LoadNative loadNative = description.getAnnotation(LoadNative.class);
                if (loadNative == null) {
                    Log.i(TAG, "Disable RecordHistogram and RecordUserAction for testing");
                    RecordHistogram.setDisabledForTests(true);
                    RecordUserAction.setDisabledForTests(true);
                } else {
                    Log.i(TAG, "Test will run with native libraries");
                }
                try {
                    statement.evaluate();
                } finally {
                    if (loadNative == null) {
                        Log.i(TAG, "Re-enable RecordHistogram and RecordUserAction after test");
                        RecordHistogram.setDisabledForTests(false);
                        RecordUserAction.setDisabledForTests(false);
                    }
                }
            }
        };
    }
}
