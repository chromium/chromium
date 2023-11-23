// Copyright 2021 The Chromium Authors
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
import org.robolectric.Shadows;
import org.robolectric.annotation.Config;
import org.robolectric.shadows.ShadowLooper;

import org.chromium.base.ThreadUtils;
import org.chromium.base.test.BaseRobolectricTestRunner;
import org.chromium.base.test.util.JniMocker;
import org.chromium.net.ConnectionType;
import org.chromium.net.NetworkChangeNotifierAutoDetect;

/** Unit test for {@link NetworkStatusListenerAndroid} and {@link BackgroundNetworkStatusListener}. */
@RunWith(BaseRobolectricTestRunner.class)
@Config(manifest = Config.NONE)
public class NetworkStatusListenerAndroidTest {
    private static final int NATIVE_PTR = 1;

    @Rule public JniMocker mJniMocker = new JniMocker();
    @Mock private NetworkChangeNotifierAutoDetect mAutoDetect;
    @Mock NetworkChangeNotifierAutoDetect.NetworkState mNetworkState;
    @Mock private NetworkStatusListenerAndroid.Natives mNativeMock;

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

    private void runBackgroundThread() {
        // Flush any UI thread tasks first.
        ShadowLooper.runUiThreadTasks();

        // Run the background thread.
        ShadowLooper shadowLooper =
                Shadows.shadowOf(
                        NetworkStatusListenerAndroid.getHelperForTesting()
                                .getHandlerForTesting()
                                .getLooper());
        shadowLooper.runToEndOfTasks();

        // Flush any UI thread tasks created by the background thread.
        ShadowLooper.runUiThreadTasks();
    }

    private void initWithConnectionType(@ConnectionType int connectionType) {
        when(mAutoDetect.getCurrentNetworkState()).thenReturn(mNetworkState);
        when(mNetworkState.getConnectionType()).thenReturn(connectionType);
        ThreadUtils.runOnUiThreadBlocking(
                () -> {
                    mListener = NetworkStatusListenerAndroid.create(NATIVE_PTR);
                });
    }

    @Test
    @SmallTest
    public void testGetCurrentConnectionType() {
        initWithConnectionType(ConnectionType.CONNECTION_3G);

        runBackgroundThread();

        // The background thread should set the connection type correctly and update the main
        // thread.
        Assert.assertEquals(ConnectionType.CONNECTION_3G, mListener.getCurrentConnectionType());
    }

    @Test
    @SmallTest
    public void testOnConnectionTypeChanged() {
        initWithConnectionType(ConnectionType.CONNECTION_3G);

        runBackgroundThread();

        Assert.assertEquals(ConnectionType.CONNECTION_3G, mListener.getCurrentConnectionType());

        // Change the connection type on main thread, the connection type should be updated.
        ThreadUtils.runOnUiThreadBlocking(
                () -> {
                    NetworkStatusListenerAndroid.getHelperForTesting()
                            .onConnectionTypeChanged(ConnectionType.CONNECTION_5G);
                });

        runBackgroundThread();

        Assert.assertEquals(ConnectionType.CONNECTION_5G, mListener.getCurrentConnectionType());
    }
}
