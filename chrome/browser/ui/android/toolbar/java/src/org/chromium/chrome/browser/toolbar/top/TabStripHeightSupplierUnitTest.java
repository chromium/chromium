// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.toolbar.top;

import static org.junit.Assert.assertEquals;

import org.junit.Assert;
import org.junit.Rule;
import org.junit.Test;
import org.junit.runner.RunWith;

import org.chromium.base.test.BaseRobolectricTestRunner;
import org.chromium.base.test.util.Features;
import org.chromium.base.test.util.Features.DisableFeatures;
import org.chromium.base.test.util.Features.JUnitProcessor;
import org.chromium.base.test.util.PayloadCallbackHelper;
import org.chromium.chrome.browser.flags.ChromeFeatureList;

/** Unit test for {@link TabStripHeightSupplier}. */
@RunWith(BaseRobolectricTestRunner.class)
public class TabStripHeightSupplierUnitTest {
    // Random number for testing purpose.
    private static final int TEST_TAB_STRIP_HEIGHT_FROM_RES = 352;

    private TabStripHeightSupplier mTabStripHeightSupplier =
            new TabStripHeightSupplier(TEST_TAB_STRIP_HEIGHT_FROM_RES);
    private PayloadCallbackHelper<Integer> mObserver = new PayloadCallbackHelper<>();

    @Rule public Features.JUnitProcessor mProcessor = new JUnitProcessor();

    @Test
    @Features.EnableFeatures(ChromeFeatureList.DYNAMIC_TOP_CHROME)
    @DisableFeatures(ChromeFeatureList.TAB_STRIP_LAYOUT_OPTIMIZATION)
    public void enableFeatureNoAssert() {
        mTabStripHeightSupplier.addObserver(mObserver::notifyCalled);

        int newHeight = 10;
        mTabStripHeightSupplier.set(newHeight);
        assertEquals(
                "mTabStripHeightSupplier.get() is wrong.",
                newHeight,
                (int) mTabStripHeightSupplier.get());
        assertEquals(newHeight, (int) mObserver.getOnlyPayloadBlocking());
    }

    @Test
    @Features.DisableFeatures(ChromeFeatureList.DYNAMIC_TOP_CHROME)
    public void disableFeatureTestAssert() {
        testAssert(
                "#addObserver", () -> mTabStripHeightSupplier.addObserver(mObserver::notifyCalled));
        testAssert("#set", () -> mTabStripHeightSupplier.set(0));
    }

    private void testAssert(String msg, Runnable runnable) {
        try {
            runnable.run();
        } catch (AssertionError e) {
            return;
        }
        Assert.fail("Assertion not triggered for " + msg);
    }
}
