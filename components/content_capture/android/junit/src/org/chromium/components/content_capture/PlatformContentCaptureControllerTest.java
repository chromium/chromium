// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.components.content_capture;

import static org.junit.Assert.assertEquals;
import static org.junit.Assert.assertFalse;
import static org.junit.Assert.assertTrue;
import static org.mockito.ArgumentMatchers.any;
import static org.mockito.Mockito.doReturn;
import static org.mockito.Mockito.never;
import static org.mockito.Mockito.verify;

import android.content.ComponentName;
import android.content.Context;
import android.content.LocusId;
import android.os.Build;
import android.view.contentcapture.ContentCaptureCondition;
import android.view.contentcapture.ContentCaptureManager;
import android.view.contentcapture.DataRemovalRequest;

import androidx.annotation.RequiresApi;

import org.junit.After;
import org.junit.Before;
import org.junit.Rule;
import org.junit.Test;
import org.junit.runner.RunWith;
import org.mockito.ArgumentCaptor;
import org.mockito.Mockito;
import org.mockito.junit.MockitoJUnit;
import org.mockito.junit.MockitoRule;
import org.robolectric.annotation.Config;
import org.robolectric.util.ReflectionHelpers;

import org.chromium.base.test.BaseRobolectricTestRunner;

import java.util.HashSet;

/** Unit test for PlatformContentCaptureController. */
@RunWith(BaseRobolectricTestRunner.class)
@Config(sdk = BaseRobolectricTestRunner.MIN_SDK)
@RequiresApi(Build.VERSION_CODES.Q)
public class PlatformContentCaptureControllerTest {
    private static final String SENSITIVE_URL = "https://example.com/account?session_token=secret";

    @Rule public MockitoRule mMockitoRule = MockitoJUnit.rule();
    private ContentCaptureManager mContentCaptureManager;
    private Context mContext;
    private ComponentName mComponentName;
    private String mOriginalBuildType;

    @Before
    public void setUp() {
        // verifyService() lets a non-AiAi service through on debuggable builds; pin the build type
        // so that the tests below exercise the production behavior regardless of the build type
        // baked into the Robolectric android-all jar.
        mOriginalBuildType = Build.TYPE;
        ReflectionHelpers.setStaticField(Build.class, "TYPE", "user");

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

    @After
    public void tearDown() {
        ReflectionHelpers.setStaticField(Build.class, "TYPE", mOriginalBuildType);
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
        doReturn(new HashSet<>()).when(mContentCaptureManager).getContentCaptureConditions();
        PlatformContentCaptureController controller =
                new PlatformContentCaptureController(mContext);
        assertTrue(controller.isAiai());
        assertTrue(controller.shouldStartCapture());
        assertFalse(controller.shouldCapture(new String[] {"http://www.chromium.org"}));
    }

    @Test
    public void testContentCaptureConditions() throws Throwable {
        HashSet<ContentCaptureCondition> conditions = new HashSet<>();
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

    @Test
    public void testNoDataRemovalRequestSentToNonAiaiService() throws Throwable {
        doReturn("org.abc").when(mComponentName).getPackageName();
        PlatformContentCaptureController controller =
                new PlatformContentCaptureController(mContext);
        assertFalse(controller.isAiai());
        assertFalse(controller.shouldStartCapture());

        controller.clearContentCaptureDataForURLs(new String[] {SENSITIVE_URL});
        controller.clearAllContentCaptureData();

        // The service never received any content, so there is nothing for it to delete; sending it
        // the deleted URLs would only disclose the user's browsing history to it.
        verify(mContentCaptureManager, never()).removeData(any(DataRemovalRequest.class));
    }

    @Test
    public void testDataRemovalRequestSentToAiaiService() throws Throwable {
        PlatformContentCaptureController controller =
                new PlatformContentCaptureController(mContext);
        assertTrue(controller.isAiai());

        controller.clearContentCaptureDataForURLs(new String[] {SENSITIVE_URL});

        ArgumentCaptor<DataRemovalRequest> captor =
                ArgumentCaptor.forClass(DataRemovalRequest.class);
        verify(mContentCaptureManager).removeData(captor.capture());
        assertEquals(1, captor.getValue().getLocusIdRequests().size());
        assertEquals(
                SENSITIVE_URL, captor.getValue().getLocusIdRequests().get(0).getLocusId().getId());
    }

    @Test
    public void testDataRemovalRequestSentToAiaiServiceWhenCaptureDisabled() throws Throwable {
        // AiAi may still hold content captured before content capture was turned off, so deletions
        // must keep flowing even though no new content is being captured.
        doReturn(false).when(mContentCaptureManager).isContentCaptureEnabled();
        PlatformContentCaptureController controller =
                new PlatformContentCaptureController(mContext);
        assertTrue(controller.isAiai());
        assertFalse(controller.shouldStartCapture());

        controller.clearContentCaptureDataForURLs(new String[] {SENSITIVE_URL});
        controller.clearAllContentCaptureData();

        verify(mContentCaptureManager, Mockito.times(2)).removeData(any(DataRemovalRequest.class));
    }
}
