// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.components.content_capture;

import static org.junit.Assert.assertFalse;
import static org.junit.Assert.assertTrue;
import static org.mockito.Mockito.doReturn;

import android.content.ComponentName;
import android.content.Context;
import android.content.LocusId;
import android.os.Build;
import android.view.contentcapture.ContentCaptureCondition;
import android.view.contentcapture.ContentCaptureManager;

import androidx.annotation.RequiresApi;

import org.junit.Before;
import org.junit.Test;
import org.junit.runner.RunWith;
import org.mockito.Mockito;
import org.mockito.MockitoAnnotations;
import org.robolectric.annotation.Config;

import org.chromium.base.test.BaseRobolectricTestRunner;

import java.util.HashSet;

/** Unit test for PlatformContentCaptureController. */
@RunWith(BaseRobolectricTestRunner.class)
@Config(manifest = Config.NONE, sdk = Build.VERSION_CODES.Q)
@RequiresApi(Build.VERSION_CODES.Q)
public class PlatformContentCaptureControllerTest {
    private ContentCaptureManager mContentCaptureManager;
    private Context mContext;
    private ComponentName mComponentName;

    @Before
    public void setUp() {
        MockitoAnnotations.initMocks(this);
        mContext = Mockito.mock(Context.class);
        mContentCaptureManager = Mockito.mock(ContentCaptureManager.class);
        mComponentName = Mockito.mock(ComponentName.class);
        doReturn(mContentCaptureManager)
                .when(mContext)
                .getSystemService(ContentCaptureManager.class);
        doReturn(mComponentName).when(mContentCaptureManager).getServiceComponentName();
        doReturn(true).when(mContentCaptureManager).isContentCaptureEnabled();
        doReturn("com.google.android.as").when(mComponentName).getPackageName();
        doReturn(null).when(mContentCaptureManager).getContentCaptureConditions();
    }

    @Test
    public void testEverythingAllowed() throws Throwable {
        PlatformContentCaptureController controller =
                new PlatformContentCaptureController(mContext);
        assertTrue(controller.isAiai());
        assertTrue(controller.shouldStartCapture());
        assertTrue(controller.shouldCapture(new String[] {"http://www.chromium.org"}));
    }

    @Test
    public void testEverythingDisallowed() throws Throwable {
        doReturn(new HashSet<ContentCaptureCondition>())
                .when(mContentCaptureManager)
                .getContentCaptureConditions();
        PlatformContentCaptureController controller =
                new PlatformContentCaptureController(mContext);
        assertTrue(controller.isAiai());
        assertTrue(controller.shouldStartCapture());
        assertFalse(controller.shouldCapture(new String[] {"http://www.chromium.org"}));
    }

    @Test
    public void testContentCaptureConditions() throws Throwable {
        HashSet<ContentCaptureCondition> conditions = new HashSet<ContentCaptureCondition>();
        conditions.add(
                new ContentCaptureCondition(
                        new LocusId(".*chromium.org"), ContentCaptureCondition.FLAG_IS_REGEX));
        conditions.add(new ContentCaptureCondition(new LocusId("www.abc.org"), 0));
        doReturn(conditions).when(mContentCaptureManager).getContentCaptureConditions();
        PlatformContentCaptureController controller =
                new PlatformContentCaptureController(mContext);
        assertTrue(controller.isAiai());
        assertTrue(controller.shouldStartCapture());
        assertTrue(controller.shouldCapture(new String[] {"http://www.chromium.org"}));
        assertTrue(controller.shouldCapture(new String[] {"http://www.abc.org"}));
        assertFalse(controller.shouldCapture(new String[] {"http://abc.org"}));
    }

    @Test
    public void testNoAndroidAs() throws Throwable {
        doReturn("org.abc").when(mComponentName).getPackageName();
        PlatformContentCaptureController controller =
                new PlatformContentCaptureController(mContext);
        assertFalse(controller.isAiai());
    }

    @Test
    public void testShouldNotStartCapture() throws Throwable {
        doReturn(false).when(mContentCaptureManager).isContentCaptureEnabled();
        PlatformContentCaptureController controller =
                new PlatformContentCaptureController(mContext);
        assertFalse(controller.shouldStartCapture());
    }
}
