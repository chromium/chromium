// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.test;

import org.junit.Assert;
import org.junit.rules.TestRule;
import org.junit.runner.Description;
import org.junit.runners.model.Statement;

import org.chromium.base.ThreadUtils;
import org.chromium.base.task.PostTask;
import org.chromium.base.task.TaskTraits;
import org.chromium.base.test.util.CriteriaHelper;
import org.chromium.chrome.browser.init.BrowserParts;
import org.chromium.chrome.browser.init.ChromeBrowserInitializer;
import org.chromium.chrome.browser.init.EmptyBrowserParts;
import org.chromium.content_public.browser.BrowserStartupController;

import java.util.concurrent.atomic.AtomicBoolean;

/**
 * Custom  {@link TestRule} for test using native in reduced mode. This also enables the flags
 * required for reduced mode to work.
 */
public class ReducedModeNativeTestRule implements TestRule {
    private final AtomicBoolean mNativeLoaded = new AtomicBoolean();
    private final boolean mAutoLoadNative;

    public ReducedModeNativeTestRule() {
        this(/* autoLoadNative= */ true);
    }

    public ReducedModeNativeTestRule(boolean autoLoadNative) {
        mAutoLoadNative = autoLoadNative;
    }

    @Override
    public Statement apply(final Statement base, final Description description) {
        return new Statement() {
            @Override
            public void evaluate() throws Throwable {
                if (mAutoLoadNative) {
                    loadNative();
                }
                base.evaluate();
            }
        };
    }

    public void loadNative() {
        final BrowserParts parts =
                new EmptyBrowserParts() {
                    @Override
                    public void finishNativeInitialization() {
                        mNativeLoaded.set(true);
                    }

                    @Override
                    public boolean startMinimalBrowser() {
                        return true;
                    }
                };
        PostTask.postTask(
                TaskTraits.UI_DEFAULT,
                () -> {
                    ChromeBrowserInitializer.getInstance()
                            .handlePreNativeStartupAndLoadLibraries(parts);
                    ChromeBrowserInitializer.getInstance().handlePostNativeStartup(true, parts);
                });
        waitForNativeLoaded();
    }

    private void waitForNativeLoaded() {
        CriteriaHelper.pollUiThread(
                mNativeLoaded::get, "Failed while waiting for starting minimal browser.");
    }

    public void assertMinimalBrowserStarted() {
        ThreadUtils.runOnUiThreadBlocking(
                () -> {
                    Assert.assertTrue(
                            "Native has not been started.",
                            BrowserStartupController.getInstance().isNativeStarted());
                    Assert.assertFalse(
                            "The full browser is started instead of minimal browser.",
                            BrowserStartupController.getInstance().isFullBrowserStarted());
                });
    }
}
