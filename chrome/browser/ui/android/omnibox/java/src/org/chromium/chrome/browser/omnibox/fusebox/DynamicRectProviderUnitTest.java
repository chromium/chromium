// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.omnibox.fusebox;

import static org.mockito.ArgumentMatchers.any;
import static org.mockito.Mockito.never;
import static org.mockito.Mockito.times;
import static org.mockito.Mockito.verify;

import android.content.res.Resources;

import org.junit.Before;
import org.junit.Rule;
import org.junit.Test;
import org.junit.runner.RunWith;
import org.mockito.Mock;
import org.mockito.junit.MockitoJUnit;
import org.mockito.junit.MockitoRule;
import org.robolectric.annotation.Config;

import org.chromium.base.test.BaseRobolectricTestRunner;
import org.chromium.ui.widget.RectProvider;

/** Unit tests for {@link DynamicRectProvider}. */
@RunWith(BaseRobolectricTestRunner.class)
@Config(manifest = Config.NONE)
public class DynamicRectProviderUnitTest {
    public @Rule MockitoRule mockitoRule = MockitoJUnit.rule();

    private @Mock RectProvider mFloatingDelegate;
    private @Mock RectProvider mBottomDelegate;
    private @Mock RectProvider.Observer mObserver;
    private @Mock Resources mResources;
    private DynamicRectProvider mDynamicRectProvider;

    @Before
    public void setUp() {
        mDynamicRectProvider = new DynamicRectProvider(mFloatingDelegate, mBottomDelegate);
    }

    @Test
    public void testSetPopupState_notObserved_doesNotProxy() {
        mDynamicRectProvider.setPopupState(FuseboxProperties.PopupState.FLOATING);
        verify(mFloatingDelegate, never()).startObserving(any());
    }

    @Test
    public void testStartObserving_proxiesToCurrentDelegate() {
        mDynamicRectProvider.setPopupState(FuseboxProperties.PopupState.FLOATING);
        mDynamicRectProvider.startObserving(mObserver);
        verify(mFloatingDelegate).startObserving(any());
    }

    @Test
    public void testSetPopupState_observed_switchesDelegates() {
        mDynamicRectProvider.startObserving(mObserver);
        mDynamicRectProvider.setPopupState(FuseboxProperties.PopupState.FLOATING);
        mDynamicRectProvider.setPopupState(FuseboxProperties.PopupState.BOTTOM);
        verify(mFloatingDelegate).stopObserving();
        verify(mBottomDelegate).startObserving(any());
    }

    @Test
    public void testStopObserving_stopsProxying() {
        mDynamicRectProvider.setPopupState(FuseboxProperties.PopupState.FLOATING);
        mDynamicRectProvider.startObserving(mObserver);
        mDynamicRectProvider.stopObserving();
        verify(mFloatingDelegate).stopObserving();
    }

    @Test
    public void testStartObserving_redundant_doesNotChurn() {
        mDynamicRectProvider.setPopupState(FuseboxProperties.PopupState.FLOATING);
        mDynamicRectProvider.startObserving(mObserver);
        mDynamicRectProvider.startObserving(mObserver);
        verify(mFloatingDelegate, times(1)).startObserving(any());
    }

    @Test
    public void testSetPopupState_redundant_doesNotChurn() {
        mDynamicRectProvider.startObserving(mObserver);
        mDynamicRectProvider.setPopupState(FuseboxProperties.PopupState.FLOATING);
        mDynamicRectProvider.setPopupState(FuseboxProperties.PopupState.FLOATING);
        verify(mFloatingDelegate, times(1)).startObserving(any());
    }

    @Test
    public void testStopObserving_redundant_doesNotChurn() {
        mDynamicRectProvider.setPopupState(FuseboxProperties.PopupState.FLOATING);
        mDynamicRectProvider.startObserving(mObserver);
        mDynamicRectProvider.stopObserving();
        mDynamicRectProvider.stopObserving();
        verify(mFloatingDelegate, times(1)).stopObserving();
    }

    @Test
    public void testGetPopupWidth_floating() {
        org.mockito.Mockito.when(
                        mResources.getDimensionPixelSize(
                                org.chromium.chrome.browser.omnibox.R.dimen.fusebox_popup_width))
                .thenReturn(500);
        int width =
                mDynamicRectProvider.getPopupWidth(
                        FuseboxProperties.PopupState.FLOATING, mResources);
        org.junit.Assert.assertEquals(500, width);
    }

    @Test
    public void testGetPopupWidth_bottom() {
        android.graphics.Rect rect = new android.graphics.Rect(0, 0, 300, 0);
        org.mockito.Mockito.when(mBottomDelegate.getRect()).thenReturn(rect);
        int width =
                mDynamicRectProvider.getPopupWidth(FuseboxProperties.PopupState.BOTTOM, mResources);
        org.junit.Assert.assertEquals(300, width);
    }
}
