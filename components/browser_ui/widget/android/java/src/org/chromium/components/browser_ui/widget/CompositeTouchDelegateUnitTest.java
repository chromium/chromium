// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.components.browser_ui.widget;

import android.view.MotionEvent;
import android.view.TouchDelegate;
import android.view.View;

import org.junit.Assert;
import org.junit.Before;
import org.junit.Test;
import org.junit.runner.RunWith;
import org.mockito.Mock;
import org.mockito.Mockito;
import org.mockito.MockitoAnnotations;
import org.robolectric.annotation.Config;

import org.chromium.base.test.BaseRobolectricTestRunner;

/**
 * Tests for CompositeTouchDelegate.
 */
@RunWith(BaseRobolectricTestRunner.class)
@Config(manifest = Config.NONE)
public final class CompositeTouchDelegateUnitTest {
    CompositeTouchDelegate mCompositeTouchDelegate;

    @Mock
    View mMockAncestorView;

    @Mock
    TouchDelegate mMockTouchDelegate;

    @Mock
    TouchDelegate mMockOtherTouchDelegate;

    @Mock
    MotionEvent mMockMotionEvent;

    @Before
    public void setUp() {
        MockitoAnnotations.initMocks(this);
        mCompositeTouchDelegate = new CompositeTouchDelegate(mMockAncestorView);
        mCompositeTouchDelegate.addDelegateForDescendantView(mMockTouchDelegate);
        mCompositeTouchDelegate.addDelegateForDescendantView(mMockOtherTouchDelegate);
    }

    @Test
    public void addDelegateForDescendantView_onTouchEventCallsDelegates() {
        mCompositeTouchDelegate.onTouchEvent(mMockMotionEvent);
        Mockito.verify(mMockTouchDelegate).onTouchEvent(mMockMotionEvent);
        Mockito.verify(mMockOtherTouchDelegate).onTouchEvent(mMockMotionEvent);
    }

    @Test
    public void addDelegateForDescendantView_onTouchEventIsHandledByFirst() {
        // Signal that the event is handled.
        Mockito.when(mMockTouchDelegate.onTouchEvent(mMockMotionEvent)).thenReturn(true);
        Assert.assertTrue(mCompositeTouchDelegate.onTouchEvent(mMockMotionEvent));
        Mockito.verify(mMockTouchDelegate).onTouchEvent(mMockMotionEvent);
        // Event was handled already, so the event won't make it here.
        Mockito.verify(mMockOtherTouchDelegate, Mockito.never()).onTouchEvent(mMockMotionEvent);
    }

    @Test
    public void removeDelegateForDescendantView_onTouchEventCallsDelegates() {
        mCompositeTouchDelegate.removeDelegateForDescendantView(mMockOtherTouchDelegate);
        mCompositeTouchDelegate.onTouchEvent(mMockMotionEvent);
        Mockito.verify(mMockTouchDelegate).onTouchEvent(mMockMotionEvent);
        Mockito.verify(mMockOtherTouchDelegate, Mockito.never()).onTouchEvent(mMockMotionEvent);
    }
}
