// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.components.stylus_handwriting;

import static org.mockito.ArgumentMatchers.any;
import static org.mockito.ArgumentMatchers.contains;
import static org.mockito.ArgumentMatchers.eq;
import static org.mockito.Mockito.doReturn;
import static org.mockito.Mockito.never;
import static org.mockito.Mockito.verify;

import android.content.Context;
import android.os.RemoteException;
import android.widget.directwriting.IDirectWritingService;

import org.junit.Before;
import org.junit.Test;
import org.junit.runner.RunWith;
import org.mockito.Mock;
import org.mockito.MockitoAnnotations;
import org.mockito.Spy;
import org.robolectric.RuntimeEnvironment;
import org.robolectric.annotation.Config;

import org.chromium.base.test.BaseRobolectricTestRunner;
import org.chromium.base.test.util.Feature;

/** Unit tests for {@link DirectWritingServiceBinder}. */
@RunWith(BaseRobolectricTestRunner.class)
@Config(manifest = Config.NONE)
public class DirectWritingServiceBinderTest {
    @Spy private DirectWritingServiceBinder mDwServiceBinder;
    @Mock private DirectWritingServiceBinder.DirectWritingTriggerCallback mTriggerCallback;
    @Mock private IDirectWritingService mRemoteDwService;
    @Mock private DirectWritingServiceCallback mDwCallback;

    private Context mContext;

    @Before
    public void setUp() {
        MockitoAnnotations.initMocks(this);
        mContext = RuntimeEnvironment.application;
        doReturn(mDwCallback).when(mTriggerCallback).getServiceCallback();
    }

    @Test
    @Feature({"Stylus Handwriting"})
    public void testOnWindowFocusChanged() {
        // Test that callback is not unregistered when window focus is lost.
        mDwServiceBinder.onWindowFocusChanged(mContext, false);
        verify(mDwServiceBinder, never()).unregisterCallback();
        verify(mDwServiceBinder).handleWindowFocusLost(mContext);

        // Test that callback is registered when window focus is gained.
        mDwServiceBinder.onWindowFocusChanged(mContext, true);
        verify(mDwServiceBinder).registerCallback();
    }

    @Test
    @Feature({"Stylus Handwriting"})
    public void testRegisterCallback() throws RemoteException {
        mDwServiceBinder.setTriggerCallbackForTest(mTriggerCallback);
        mDwServiceBinder.registerCallback();
        verify(mRemoteDwService, never()).registerCallback(any(), any());

        mDwServiceBinder.setRemoteServiceForTest(mRemoteDwService);
        mDwServiceBinder.registerCallback();
        verify(mRemoteDwService)
                .registerCallback(
                        eq(mDwCallback),
                        contains(IDirectWritingService.VALUE_SERVICE_HOST_SOURCE_WEBVIEW));
    }

    @Test
    @Feature({"Stylus Handwriting"})
    public void testUnregisterCallback() throws RemoteException {
        mDwServiceBinder.setTriggerCallbackForTest(mTriggerCallback);
        mDwServiceBinder.unregisterCallback();
        verify(mRemoteDwService, never()).unregisterCallback(any());

        mDwServiceBinder.setRemoteServiceForTest(mRemoteDwService);
        mDwServiceBinder.unregisterCallback();
        verify(mRemoteDwService).unregisterCallback(mDwCallback);
    }
}
