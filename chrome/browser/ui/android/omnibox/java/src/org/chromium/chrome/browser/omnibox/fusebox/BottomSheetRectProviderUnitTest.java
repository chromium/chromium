// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.omnibox.fusebox;

import static org.junit.Assert.assertEquals;
import static org.mockito.Mockito.verify;

import android.app.Activity;
import android.graphics.Rect;
import android.view.View;

import org.junit.Before;
import org.junit.Rule;
import org.junit.Test;
import org.junit.runner.RunWith;
import org.mockito.Mock;
import org.mockito.junit.MockitoJUnit;
import org.mockito.junit.MockitoRule;
import org.robolectric.Robolectric;
import org.robolectric.annotation.Config;

import org.chromium.base.test.BaseRobolectricTestRunner;
import org.chromium.ui.base.TestActivity;

/** Unit tests for {@link BottomSheetRectProvider}. */
@RunWith(BaseRobolectricTestRunner.class)
@Config(manifest = Config.NONE)
public class BottomSheetRectProviderUnitTest {
    public @Rule MockitoRule mockitoRule = MockitoJUnit.rule();

    private @Mock View mAnchorView;
    private Activity mActivity;
    private BottomSheetRectProvider mProvider;

    @Before
    public void setUp() {
        mActivity = Robolectric.setupActivity(TestActivity.class);
        mProvider = new BottomSheetRectProvider(mActivity, mAnchorView);
    }

    @Test
    public void testConstructor_registersLayoutChangeListener() {
        verify(mAnchorView).addOnLayoutChangeListener(mProvider);
    }

    @Test
    public void testUpdateRect_anchorsToBottom() {
        // The rect should be anchored to the bottom, meaning top == bottom.
        Rect r = mProvider.getRect();
        assertEquals("Rect top should equal bottom", r.top, r.bottom);
    }

    @Test
    public void testOnLayoutChange_updatesRect() {
        mProvider.onLayoutChange(mAnchorView, 0, 0, 100, 100, 0, 0, 0, 0);
        Rect r = mProvider.getRect();
        assertEquals("Rect top should equal bottom", r.top, r.bottom);
    }

    @Test
    public void testDestroy_removesListener() {
        mProvider.destroy();
        verify(mAnchorView).removeOnLayoutChangeListener(mProvider);
    }
}
