// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.net.impl;

import static com.google.common.truth.Truth.assertThat;

import static org.junit.Assume.assumeTrue;

import static org.chromium.net.truth.UrlResponseInfoSubject.assertThat;

import android.content.Context;
import android.content.ContextWrapper;
import android.os.Build;

import androidx.test.core.app.ApplicationProvider;
import androidx.test.ext.junit.runners.AndroidJUnit4;
import androidx.test.filters.SmallTest;

import org.junit.Test;
import org.junit.runner.RunWith;

import org.chromium.base.test.util.Batch;
import org.chromium.net.CronetEngine;
import org.chromium.net.CronetTestFramework.CronetImplementation;
import org.chromium.net.impl.CronetLogger.CronetSource;

/** Tests {@link UserAgent} */
@Batch(Batch.UNIT_TESTS)
@RunWith(AndroidJUnit4.class)
public class UserAgentTest {
    private Context getContextWithLegacyUserAgent(boolean value) {
        return new ContextWrapper(
                UserAgentTestUtil.getContextInterceptorWithLegacyUserAgent(value)
                        .interceptContext(ApplicationProvider.getApplicationContext())) {
            @Override
            public Context getApplicationContext() {
                // Ensure the code under test (in particular, CronetManifest) cannot use this method
                // to "escape" context interception.
                return this;
            }
        };
    }

    @Test
    @SmallTest
    public void testLegacyUserAgent_whenOnPlatformSourceAndMetadataIsFalse() throws Exception {
        assertThat(
                        UserAgent.from(
                                getContextWithLegacyUserAgent(false),
                                CronetSource.CRONET_SOURCE_PLATFORM,
                                "123.0.234.345"))
                .startsWith("AndroidHttpClient");
    }

    @Test
    @SmallTest
    public void testLegacyUserAgent_whenOnPlatformSourceAndMetadataIsTrue() throws Exception {
        assertThat(
                        UserAgent.from(
                                getContextWithLegacyUserAgent(true),
                                CronetSource.CRONET_SOURCE_PLATFORM,
                                "123.0.234.345"))
                .doesNotContain("AndroidHttpClient");
    }

    @Test
    @SmallTest
    public void testLegacyUserAgent_whenNotOnPlatformSourceAndMetadataIsFalse() throws Exception {
        for (CronetSource source : CronetSource.values()) {
            if (source != CronetSource.CRONET_SOURCE_PLATFORM) {
                assertThat(
                                UserAgent.from(
                                        getContextWithLegacyUserAgent(false),
                                        source,
                                        "123.0.234.345"))
                        .doesNotContain("AndroidHttpClient");
            }
        }
    }

    @Test
    @SmallTest
    public void testLegacyUserAgent_whenNotOnPlatformSourceAndMetadataIsTrue() throws Exception {
        assertThat(
                        UserAgent.from(
                                getContextWithLegacyUserAgent(true),
                                CronetSource.CRONET_SOURCE_PLATFORM,
                                "123.0.234.345"))
                .doesNotContain("AndroidHttpClient");
    }

    @Test
    @SmallTest
    public void testLegacyUserAgent_whenOnPlatformProviderAndMetadataIsFalse() throws Exception {
        assumeTrue(
                "This test is only available on U+ devices",
                Build.VERSION.SDK_INT >= Build.VERSION_CODES.UPSIDE_DOWN_CAKE);
        CronetEngine.Builder httpEngineBuilderViaWrapper =
                new HttpEngineNativeProvider(getContextWithLegacyUserAgent(false))
                        .createBuilder();

        assertThat(httpEngineBuilderViaWrapper.getDefaultUserAgent())
                .isEqualTo(
                        UserAgentTestUtil.getUserAgentWithAndroidHttpClient(
                                CronetImplementation.AOSP_PLATFORM));
    }

    @Test
    @SmallTest
    public void testLegacyUserAgent_whenOnNativeProviderAndMetadataIsFalse() throws Exception {
        CronetEngine.Builder engineBuilder =
                new NativeCronetProvider(getContextWithLegacyUserAgent(false)).createBuilder();
        assertThat(engineBuilder.getDefaultUserAgent())
                .isEqualTo(
                        UserAgentTestUtil.getUserAgentWithPackageName(
                                CronetImplementation.STATICALLY_LINKED));
    }

    @Test
    @SmallTest
    public void testLegacyUserAgent_whenOnPlatformProviderAndMetadataIsTrue() throws Exception {
        assumeTrue(
                "This test is only available on U+ devices",
                Build.VERSION.SDK_INT >= Build.VERSION_CODES.UPSIDE_DOWN_CAKE);
        CronetEngine.Builder httpEngineBuilderViaWrapper =
                new HttpEngineNativeProvider(getContextWithLegacyUserAgent(true))
                        .createBuilder();
        assertThat(httpEngineBuilderViaWrapper.getDefaultUserAgent())
                .isEqualTo(
                        UserAgentTestUtil.getUserAgentWithPackageName(
                                CronetImplementation.AOSP_PLATFORM));
    }

    @Test
    @SmallTest
    public void testLegacyUserAgent_whenOnNativeProviderAndMetadataIsTrue() throws Exception {
        CronetEngine.Builder engineBuilder =
                new NativeCronetProvider(getContextWithLegacyUserAgent(true)).createBuilder();
        assertThat(engineBuilder.getDefaultUserAgent())
                .isEqualTo(
                        UserAgentTestUtil.getUserAgentWithPackageName(
                                CronetImplementation.STATICALLY_LINKED));
    }
}
