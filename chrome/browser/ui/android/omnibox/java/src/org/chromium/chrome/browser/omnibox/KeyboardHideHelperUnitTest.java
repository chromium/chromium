// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.omnibox;

import static org.mockito.Mockito.doReturn;
import static org.mockito.Mockito.spy;
import static org.mockito.Mockito.times;
import static org.mockito.Mockito.verify;

import android.graphics.Rect;
import android.view.View;

import org.junit.Assert;
import org.junit.Before;
import org.junit.Test;
import org.junit.runner.RunWith;
import org.mockito.Mock;
import org.mockito.Mockito;
import org.mockito.MockitoAnnotations;
import org.mockito.Spy;
import org.mockito.invocation.InvocationOnMock;
import org.mockito.stubbing.Answer;
import org.robolectric.RuntimeEnvironment;
import org.robolectric.annotation.Config;
import org.robolectric.annotation.LooperMode;
import org.robolectric.shadows.ShadowLooper;

import org.chromium.base.test.BaseRobolectricTestRunner;
import org.chromium.ui.base.WindowDelegate;

import java.util.concurrent.atomic.AtomicInteger;

@RunWith(BaseRobolectricTestRunner.class)
@Config(manifest = Config.NONE)
@LooperMode(LooperMode.Mode.LEGACY)
public class KeyboardHideHelperUnitTest {
    @Mock private Runnable mKeyboardHiddenCallback;
    @Mock private View mRootView;
    @Mock private WindowDelegate mWindowDelegate;

    @Spy private View mView;

    private KeyboardHideHelper mKeyboardHideHelper;

    @Before
    public void setUp() {
        MockitoAnnotations.initMocks(this);

        mView = spy(new View(RuntimeEnvironment.application));
        mKeyboardHideHelper = new KeyboardHideHelper(mView, mKeyboardHiddenCallback);
    }

    @Test
    public void testHideNotifiedOnSizeDecrease_WithoutWindowDelegate() {
        doReturn(mRootView).when(mView).getRootView();
        doReturn(300).when(mRootView).getHeight();
        mKeyboardHideHelper.monitorForKeyboardHidden();
        Assert.assertTrue(mKeyboardHideHelper.isMonitoringForLayoutChanges());

        doReturn(500).when(mRootView).getHeight();
        mKeyboardHideHelper.onGlobalLayout();

        verify(mKeyboardHiddenCallback, times(1)).run();
        Assert.assertFalse(mKeyboardHideHelper.isMonitoringForLayoutChanges());
    }

    @Test
    public void testHideNotifiedOnSizeDecrease_WithWindowDelegate() {
        mKeyboardHideHelper.setWindowDelegate(mWindowDelegate);
        final AtomicInteger height = new AtomicInteger(300);
        Answer<Void> windowVisibleDisplayFrameAnswer =
                new Answer<Void>() {
                    @Override
                    public Void answer(InvocationOnMock invocation) {
                        ((Rect) invocation.getArgument(0)).set(0, 0, 100, height.get());
                        return null;
                    }
                };
        Mockito.doAnswer(windowVisibleDisplayFrameAnswer)
                .when(mWindowDelegate)
                .getWindowVisibleDisplayFrame(Mockito.any(Rect.class));
        doReturn(500).when(mWindowDelegate).getDecorViewHeight();

        mKeyboardHideHelper.monitorForKeyboardHidden();
        Assert.assertTrue(mKeyboardHideHelper.isMonitoringForLayoutChanges());

        height.set(500);
        mKeyboardHideHelper.onGlobalLayout();

        verify(mKeyboardHiddenCallback, times(1)).run();
        Assert.assertFalse(mKeyboardHideHelper.isMonitoringForLayoutChanges());
    }

    @Test
    public void testMonitorTimeElapsed() {
        mKeyboardHideHelper.monitorForKeyboardHidden();
        Assert.assertTrue(mKeyboardHideHelper.isMonitoringForLayoutChanges());
        ShadowLooper.runUiThreadTasksIncludingDelayedTasks();
        Assert.assertFalse(mKeyboardHideHelper.isMonitoringForLayoutChanges());
    }
}
