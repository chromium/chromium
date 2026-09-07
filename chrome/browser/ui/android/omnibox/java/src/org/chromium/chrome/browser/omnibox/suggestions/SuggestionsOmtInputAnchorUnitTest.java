// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.omnibox.suggestions;

import static org.junit.Assert.assertFalse;
import static org.junit.Assert.assertTrue;
import static org.mockito.ArgumentMatchers.any;
import static org.mockito.Mockito.doReturn;
import static org.mockito.Mockito.never;
import static org.mockito.Mockito.verify;

import android.content.Context;
import android.os.Build;
import android.view.WindowManager;
import android.window.InputTransferToken;

import org.junit.Before;
import org.junit.Rule;
import org.junit.Test;
import org.junit.runner.RunWith;
import org.mockito.Mock;
import org.mockito.junit.MockitoJUnit;
import org.mockito.junit.MockitoRule;
import org.mockito.quality.Strictness;
import org.robolectric.annotation.Config;

import org.chromium.base.ContextUtils;
import org.chromium.base.test.BaseRobolectricTestRunner;
import org.chromium.base.test.util.HistogramWatcher;

/** Unit tests for {@link SuggestionsOmtInputAnchor}. */
@RunWith(BaseRobolectricTestRunner.class)
public class SuggestionsOmtInputAnchorUnitTest {
    @Rule
    public final MockitoRule mMockitoRule = MockitoJUnit.rule().strictness(Strictness.STRICT_STUBS);

    @Mock private WindowManager mWindowManager;
    @Mock private InputTransferToken mFromToken;
    @Mock private InputTransferToken mToToken;

    @Before
    public void setUp() {
        Context context = ContextUtils.getApplicationContext();
        ContextUtils.initApplicationContextForTests(
                new android.content.ContextWrapper(context) {
                    @Override
                    public Object getSystemService(String name) {
                        if (Context.WINDOW_SERVICE.equals(name)) {
                            return mWindowManager;
                        }
                        return super.getSystemService(name);
                    }
                });
    }

    @Test
    @Config(sdk = Build.VERSION_CODES.VANILLA_ICE_CREAM)
    public void transferTouch_disabledBelowBaklava() {
        var watcher =
                HistogramWatcher.newBuilder()
                        .expectNoRecords("Android.Omnibox.OMTPrefetch.TouchTransferSuccess")
                        .build();

        // Execute transfer touch on SDK level prior to Baklava.
        boolean result = SuggestionsOmtInputAnchor.transferTouch(mFromToken, mToToken);

        // Verify that execution returns false without calling WindowManager.
        assertFalse(result);
        verify(mWindowManager, never()).transferTouchGesture(any(), any());
        watcher.assertExpected();
    }

    @Test
    @Config(sdk = Build.VERSION_CODES.BAKLAVA)
    public void transferTouch_successOnBaklava() {
        var watcher =
                HistogramWatcher.newBuilder()
                        .expectBooleanRecord(
                                "Android.Omnibox.OMTPrefetch.TouchTransferSuccess", true)
                        .build();

        // Configure WindowManager mock to accept transfer gesture.
        doReturn(true).when(mWindowManager).transferTouchGesture(mFromToken, mToToken);

        // Execute transfer touch on Baklava.
        boolean result = SuggestionsOmtInputAnchor.transferTouch(mFromToken, mToToken);

        // Verify that execution succeeds and delegates to WindowManager.
        assertTrue(result);
        verify(mWindowManager).transferTouchGesture(mFromToken, mToToken);
        watcher.assertExpected();
    }

    @Test
    @Config(sdk = Build.VERSION_CODES.BAKLAVA)
    public void transferTouch_failureOnBaklava() {
        var watcher =
                HistogramWatcher.newBuilder()
                        .expectBooleanRecord(
                                "Android.Omnibox.OMTPrefetch.TouchTransferSuccess", false)
                        .build();

        // Configure WindowManager mock to fail transfer gesture.
        doReturn(false).when(mWindowManager).transferTouchGesture(mFromToken, mToToken);

        // Execute transfer touch on Baklava.
        boolean result = SuggestionsOmtInputAnchor.transferTouch(mFromToken, mToToken);

        // Verify that failure is propagated back to caller.
        assertFalse(result);
        verify(mWindowManager).transferTouchGesture(mFromToken, mToToken);
        watcher.assertExpected();
    }

    @Test
    @Config(sdk = Build.VERSION_CODES.BAKLAVA)
    public void transferTouch_nullWindowManagerOnBaklava() {
        Context context = ContextUtils.getApplicationContext();
        ContextUtils.initApplicationContextForTests(
                new android.content.ContextWrapper(context) {
                    @Override
                    public Object getSystemService(String name) {
                        return null;
                    }
                });

        var watcher =
                HistogramWatcher.newBuilder()
                        .expectNoRecords("Android.Omnibox.OMTPrefetch.TouchTransferSuccess")
                        .build();

        // When WindowManager is unavailable on Baklava, transferTouch should return false cleanly.
        boolean result = SuggestionsOmtInputAnchor.transferTouch(mFromToken, mToToken);

        assertFalse(result);
        watcher.assertExpected();
    }
}
