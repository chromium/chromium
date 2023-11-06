// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.components.stylus_handwriting;

import static org.junit.Assert.assertEquals;
import static org.junit.Assert.assertNull;
import static org.mockito.ArgumentMatchers.any;
import static org.mockito.ArgumentMatchers.anyBoolean;
import static org.mockito.Mockito.doCallRealMethod;
import static org.mockito.Mockito.doReturn;
import static org.mockito.Mockito.spy;
import static org.mockito.Mockito.times;
import static org.mockito.Mockito.verify;
import static org.mockito.MockitoAnnotations.openMocks;

import android.content.Context;
import android.os.Build;
import android.view.PointerIcon;

import androidx.annotation.RequiresApi;
import androidx.test.platform.app.InstrumentationRegistry;

import org.junit.Before;
import org.junit.Test;
import org.junit.runner.RunWith;
import org.mockito.Mock;

import org.chromium.base.test.BaseRobolectricTestRunner;
import org.chromium.base.test.util.Batch;
import org.chromium.base.test.util.Feature;
import org.chromium.content_public.browser.WebContents;
import org.chromium.ui.base.ViewAndroidDelegate;

/**
 * Tests for StylusWritingController. Specifically how it handles whether to show a stylus hover
 * icon or not.
 */
@Batch(Batch.PER_CLASS)
@RunWith(BaseRobolectricTestRunner.class)
@RequiresApi(api = Build.VERSION_CODES.TIRAMISU)
public class StylusWritingControllerTest {
    private final Context mContext =
            InstrumentationRegistry.getInstrumentation().getTargetContext();
    private StylusWritingController mStylusWritingController;
    private ViewAndroidDelegate mViewAndroidDelegate;
    private PointerIcon mPointerIcon;
    @Mock private WebContents mWebContents;

    @Before
    public void setUp() {
        openMocks(this);
        // Use a pointer type different to the default and different to what's used in production.
        mPointerIcon = PointerIcon.getSystemIcon(mContext, PointerIcon.TYPE_GRAB);
        mStylusWritingController =
                StylusWritingController.createControllerForTests(mContext, mPointerIcon);

        mViewAndroidDelegate = ViewAndroidDelegate.createBasicDelegate(null);
        mViewAndroidDelegate = spy(mViewAndroidDelegate);
        doCallRealMethod().when(mViewAndroidDelegate).notifyHoverActionStylusWritable(anyBoolean());
        doCallRealMethod().when(mViewAndroidDelegate).setShouldShowStylusHoverIconCallback(any());

        doReturn(mViewAndroidDelegate).when(mWebContents).getViewAndroidDelegate();
        mStylusWritingController.onWebContentsChanged(mWebContents);
        verify(mViewAndroidDelegate).setShouldShowStylusHoverIconCallback(any());
    }

    @Test
    @Feature({"Stylus Handwriting"})
    public void testStylusWritableUpdatesIcon() {
        mViewAndroidDelegate.notifyHoverActionStylusWritable(true);
        assertEquals(mPointerIcon, mStylusWritingController.resolvePointerIcon());
        mViewAndroidDelegate.notifyHoverActionStylusWritable(false);
        assertNull(mStylusWritingController.resolvePointerIcon());
    }

    @Test
    @Feature({"Stylus Handwriting"})
    public void testWindowFocusChangeUpdatesCallback() {
        mStylusWritingController.onWindowFocusChanged(true);
        verify(mViewAndroidDelegate, times(2)).setShouldShowStylusHoverIconCallback(any());
    }
}
