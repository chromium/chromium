// Copyright 2015 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.net.urlconnection;

import static com.google.common.truth.Truth.assertThat;

import static org.junit.Assert.assertThrows;

import android.os.Build;

import androidx.test.ext.junit.runners.AndroidJUnit4;
import androidx.test.filters.SmallTest;

import org.junit.After;
import org.junit.Rule;
import org.junit.Test;
import org.junit.runner.RunWith;

import org.chromium.base.test.util.DoNotBatch;
import org.chromium.net.CronetTestRule;
import org.chromium.net.CronetTestRule.CronetImplementation;
import org.chromium.net.CronetTestRule.IgnoreFor;
import org.chromium.net.CronetTestRule.RequiresMinAndroidApi;
import org.chromium.net.NativeTestServer;

import java.net.HttpURLConnection;
import java.net.URL;

/** Test for CronetURLStreamHandlerFactory. */
@DoNotBatch(
        reason = "URL#setURLStreamHandlerFactory can be called at most once during JVM lifetime")
@IgnoreFor(
        implementations = {CronetImplementation.FALLBACK},
        reason = "See crrev.com/c/4590329")
@RunWith(AndroidJUnit4.class)
public class CronetURLStreamHandlerFactoryTest {
    @Rule public final CronetTestRule mTestRule = CronetTestRule.withAutomaticEngineStartup();

    private HttpURLConnection mUrlConnection;

    @After
    public void tearDown() {
        if (mUrlConnection != null) {
            mUrlConnection.disconnect();
        }
        NativeTestServer.shutdownNativeTestServer();
    }

    @Test
    @SmallTest
    public void testRequireConfig() throws Exception {
        NullPointerException e =
                assertThrows(
                        NullPointerException.class, () -> new CronetURLStreamHandlerFactory(null));
        assertThat(e).hasMessageThat().isEqualTo("CronetEngine is null.");
    }

    public void internalSetUrlStreamFactoryUsesCronet() throws Exception {
        assertThat(
                        NativeTestServer.startNativeTestServer(
                                mTestRule.getTestFramework().getContext()))
                .isTrue();

        URL.setURLStreamHandlerFactory(
                mTestRule.getTestFramework().getEngine().createURLStreamHandlerFactory());
        URL url = new URL(NativeTestServer.getEchoMethodURL());
        mUrlConnection = (HttpURLConnection) url.openConnection();
        assertThat(mUrlConnection.getResponseCode()).isEqualTo(200);
        assertThat(mUrlConnection.getResponseMessage()).isEqualTo("OK");
        assertThat(TestUtil.getResponseAsString(mUrlConnection)).isEqualTo("GET");
    }

    @Test
    @SmallTest
    @IgnoreFor(
            implementations = {CronetImplementation.AOSP_PLATFORM},
            reason =
                    "URL#setURLStreamHandlerFactory can be called at most once during JVM lifetime."
                            + " Running against both impls through CronetTestRule would violate that."
                            + " Instead duplicate the test targets")
    public void testSetUrlStreamFactoryUsesCronetForNative() throws Exception {
        internalSetUrlStreamFactoryUsesCronet();
    }

    @Test
    @SmallTest
    @IgnoreFor(
            implementations = {CronetImplementation.STATICALLY_LINKED},
            reason =
                    "URL#setURLStreamHandlerFactory can be called at most once during JVM lifetime."
                            + " Running against both impls through CronetTestRule would violate that."
                            + " Instead duplicate the test targets")
    @RequiresMinAndroidApi(Build.VERSION_CODES.UPSIDE_DOWN_CAKE)
    public void testSetUrlStreamFactoryUsesCronetForHttpEngine() throws Exception {
        internalSetUrlStreamFactoryUsesCronet();
    }
}
