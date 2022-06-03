// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.components.download;

import static org.mockito.Mockito.when;

import androidx.test.filters.SmallTest;

import org.junit.Assert;
import org.junit.Before;
import org.junit.Rule;
import org.junit.Test;
import org.junit.runner.RunWith;
import org.mockito.Mock;
import org.mockito.MockitoAnnotations;

import org.chromium.base.ThreadUtils;
import org.chromium.base.test.BaseJUnit4ClassRunner;
import org.chromium.base.test.util.CriteriaHelper;
import org.chromium.base.test.util.JniMocker;
import org.chromium.net.ConnectionType;
import org.chromium.net.NetworkChangeNotifierAutoDetect;

/**
 * Unit test for {@link NetworkStatusListenerAndroid} and {@link BackgroundNetworkStatusListener}.
 */
@RunWith(BaseJUnit4ClassRunner.class)
public class NetworkStatusListenerAndroidTest {
    private static final int NATIVE_PTR = 1;

    @Rule
    public JniMocker mJniMocker = new JniMocker();
    @Mock
    private NetworkChangeNotifierAutoDetect mAutoDetect;
    @Mock
    NetworkChangeNotifierAutoDetect.NetworkState mNetworkState;
    @Mock
    private NetworkStatusListenerAndroid.Natives mNativeMock;

    private NetworkStatusListenerAndroid mListener;

    private static class TestAutoDetectFactory
            extends BackgroundNetworkStatusListener.AutoDetectFactory {
        private NetworkChangeNotifierAutoDetect mAutoDetect;
        TestAutoDetectFactory(NetworkChangeNotifierAutoDetect autoDetect) {
            mAutoDetect = autoDetect;
        }

        // AutoDetectFactory overrides.
        @Override
        public NetworkChangeNotifierAutoDetect create(
                NetworkChangeNotifierAutoDetect.Observer observer,
                NetworkChangeNotifierAutoDetect.RegistrationPolicy policy) {
            return mAutoDetect;
        }
    }

    @Before
    public void setUp() {
        MockitoAnnotations.initMocks(this);
        BackgroundNetworkStatusListener.setAutoDetectFactory(
                new TestAutoDetectFactory(mAutoDetect));
        mJniMocker.mock(NetworkStatusListenerAndroidJni.TEST_HOOKS, mNativeMock);
    }

    private void initWithConnectionType(@ConnectionType int connectionType) {
        when(mAutoDetect.getCurrentNetworkState()).thenReturn(mNetworkState);
        when(mNetworkState.getConnectionType()).thenReturn(connectionType);
        ThreadUtils.runOnUiThreadBlocking(
                () -> { mListener = NetworkStatusListenerAndroid.create(NATIVE_PTR); });
    }

    @Test
    @SmallTest
    public void testGetCurrentConnectionType() {
        initWithConnectionType(ConnectionType.CONNECTION_3G);

        // The background thread should set the connection type correctly and update the main
        // thread.
        CriteriaHelper.pollUiThread(
                () -> mListener.getCurrentConnectionType() == ConnectionType.CONNECTION_3G);
    }

    @Test
    @SmallTest
    public void testOnConnectionTypeChanged() {
        initWithConnectionType(ConnectionType.CONNECTION_3G);
        CriteriaHelper.pollUiThread(
                () -> mListener.getCurrentConnectionType() == ConnectionType.CONNECTION_3G);

        // Change the connection type on main thread, the connection type should be updated.
        ThreadUtils.runOnUiThreadBlocking(() -> {
            NetworkStatusListenerAndroid.getHelperForTesting().onConnectionTypeChanged(
                    ConnectionType.CONNECTION_5G);
            Assert.assertEquals(ConnectionType.CONNECTION_5G, mListener.getCurrentConnectionType());
        });
    }
}
