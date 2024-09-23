// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.components.data_sharing;

import static org.mockito.Mockito.times;
import static org.mockito.Mockito.verify;
import static org.robolectric.Shadows.shadowOf;

import android.os.Looper;

import org.junit.Before;
import org.junit.Rule;
import org.junit.Test;
import org.junit.runner.RunWith;
import org.mockito.Mock;
import org.mockito.MockitoAnnotations;
import org.robolectric.annotation.Config;

import org.chromium.base.ThreadUtils;
import org.chromium.base.test.BaseRobolectricTestRunner;
import org.chromium.base.test.util.JniMocker;
import org.chromium.url.GURL;

@RunWith(BaseRobolectricTestRunner.class)
@Config(manifest = Config.NONE)
public class DataSharingNetworkLoaderImplTest {
    @Rule public JniMocker mMocker = new JniMocker();
    @Mock private DataSharingNetworkLoaderImpl.Natives mDataSharingNetworkLoaderJniMock;
    private DataSharingNetworkLoaderImpl mDataSharingNetworkLoader;

    @Before
    public void setup() {
        ThreadUtils.setUiThread(Looper.getMainLooper());
        MockitoAnnotations.initMocks(this);
        mMocker.mock(DataSharingNetworkLoaderImplJni.TEST_HOOKS, mDataSharingNetworkLoaderJniMock);
        mDataSharingNetworkLoader = new DataSharingNetworkLoaderImpl(1);
    }

    @Test
    public void testLoadUrl() {
        mDataSharingNetworkLoader.loadUrl(
                GURL.emptyGURL(), null, null, DataSharingRequestType.CREATE_GROUP, null);
        shadowOf(Looper.getMainLooper()).idle();
        verify(mDataSharingNetworkLoaderJniMock, times(1))
                .loadUrl(
                        1,
                        GURL.emptyGURL(),
                        null,
                        null,
                        DataSharingNetworkUtils.getNetworkTrafficAnnotationTag(
                                        DataSharingRequestType.CREATE_GROUP)
                                .getHashCode(),
                        null);
    }
}
