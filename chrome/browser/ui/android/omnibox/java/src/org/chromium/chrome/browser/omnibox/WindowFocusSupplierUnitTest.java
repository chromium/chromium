// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.omnibox;

import static org.junit.Assert.assertFalse;
import static org.junit.Assert.assertTrue;
import static org.mockito.Mockito.verify;

import org.junit.After;
import org.junit.Before;
import org.junit.Rule;
import org.junit.Test;
import org.junit.runner.RunWith;
import org.mockito.Mock;
import org.mockito.junit.MockitoJUnit;
import org.mockito.junit.MockitoRule;
import org.robolectric.Robolectric;
import org.robolectric.android.controller.ActivityController;

import org.chromium.base.test.BaseRobolectricTestRunner;
import org.chromium.chrome.browser.lifecycle.ActivityLifecycleDispatcher;
import org.chromium.ui.base.TestActivity;
import org.chromium.ui.base.WindowAndroid;

/** Unit tests for {@link WindowFocusSupplier}. */
@RunWith(BaseRobolectricTestRunner.class)
public class WindowFocusSupplierUnitTest {
    @Rule public MockitoRule mMockitoRule = MockitoJUnit.rule();

    @Mock private ActivityLifecycleDispatcher mActivityLifecycleDispatcher;
    @Mock private WindowAndroid mWindowAndroid;

    private ActivityController<TestActivity> mActivityController;
    private WindowFocusSupplier mWindowFocusSupplier;

    @Before
    public void setUp() {
        mActivityController = Robolectric.buildActivity(TestActivity.class).setup();
        mWindowFocusSupplier =
                new WindowFocusSupplier(mActivityLifecycleDispatcher, mWindowAndroid);
    }

    @After
    public void tearDown() {
        mActivityController.close();
    }

    @Test
    public void testRegistersObserverOnCreation() {
        verify(mActivityLifecycleDispatcher).register(mWindowFocusSupplier);
    }

    @Test
    public void testOnWindowFocusChanged_updatesValue() {
        mWindowFocusSupplier.onWindowFocusChanged(false);
        assertFalse(mWindowFocusSupplier.get());

        mWindowFocusSupplier.onWindowFocusChanged(true);
        assertTrue(mWindowFocusSupplier.get());
    }

    @Test
    public void testDestroy_unregistersObserver() {
        mWindowFocusSupplier.destroy();
        verify(mActivityLifecycleDispatcher).unregister(mWindowFocusSupplier);
    }
}
