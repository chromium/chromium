// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.test;

import org.junit.Assert;
import org.junit.rules.TestRule;
import org.junit.runner.Description;
import org.junit.runners.model.Statement;

import org.chromium.base.library_loader.LibraryProcessType;
import org.chromium.base.task.PostTask;
import org.chromium.chrome.browser.init.BrowserParts;
import org.chromium.chrome.browser.init.ChromeBrowserInitializer;
import org.chromium.chrome.browser.init.EmptyBrowserParts;
import org.chromium.content_public.browser.BrowserStartupController;
import org.chromium.content_public.browser.UiThreadTaskTraits;
import org.chromium.content_public.browser.test.util.Criteria;
import org.chromium.content_public.browser.test.util.CriteriaHelper;
import org.chromium.content_public.browser.test.util.TestThreadUtils;

import java.util.concurrent.atomic.AtomicBoolean;

/**
 * Custom  {@link TestRule} for test using native in reduced mode. This also enables the flags
 * required for reduced mode to work.
 */
public class ReducedModeNativeTestRule implements TestRule {
    private final AtomicBoolean mNativeLoaded = new AtomicBoolean();
    private final boolean mAutoLoadNative;

    public ReducedModeNativeTestRule() {
        this(true /*autoLoadNative*/);
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
        final BrowserParts parts = new EmptyBrowserParts() {
            @Override
            public void finishNativeInitialization() {
                mNativeLoaded.set(true);
            }

            @Override
            public boolean startServiceManagerOnly() {
                return true;
            }
        };
        PostTask.postTask(UiThreadTaskTraits.DEFAULT, () -> {
            ChromeBrowserInitializer.getInstance().handlePreNativeStartup(parts);
            ChromeBrowserInitializer.getInstance().handlePostNativeStartup(true, parts);
        });
        waitForNativeLoaded();
    }

    private void waitForNativeLoaded() {
        CriteriaHelper.pollUiThread(
                new Criteria("Failed while waiting for starting Service Manager.") {
                    @Override
                    public boolean isSatisfied() {
                        return mNativeLoaded.get();
                    }
                });
    }

    public void assertOnlyServiceManagerStarted() {
        TestThreadUtils.runOnUiThreadBlocking(() -> {
            Assert.assertTrue("Native has not been started.",
                    BrowserStartupController.get(LibraryProcessType.PROCESS_BROWSER)
                            .isNativeStarted());
            Assert.assertFalse("The full browser is started instead of ServiceManager only.",
                    BrowserStartupController.get(LibraryProcessType.PROCESS_BROWSER)
                            .isFullBrowserStarted());
        });
    }
}
