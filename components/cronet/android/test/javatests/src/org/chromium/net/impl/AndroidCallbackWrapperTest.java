// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.net.impl;

import static com.google.common.truth.Truth.assertThat;
import static com.google.common.truth.TruthJUnit.assume;

import android.os.Build;

import androidx.test.core.app.ApplicationProvider;
import androidx.test.ext.junit.runners.AndroidJUnit4;
import androidx.test.filters.SmallTest;

import org.junit.Before;
import org.junit.Test;
import org.junit.runner.RunWith;

import org.chromium.base.test.util.Batch;
import org.chromium.net.CronetEngine;
import org.chromium.net.NativeTestServer;
import org.chromium.net.TestUrlRequestCallback;
import org.chromium.net.UrlRequest;

@Batch(Batch.UNIT_TESTS)
@RunWith(AndroidJUnit4.class)
public class AndroidCallbackWrapperTest {
    private NativeTestServer mNativeTestServer;

    @Before
    public void setUp() throws Exception {
        assume().that(Build.VERSION.SDK_INT).isAtLeast(Build.VERSION_CODES.UPSIDE_DOWN_CAKE);
    }

    @Test
    @SmallTest
    public void testHttpEngineWrapperCorrectlyPropagatesTrafficStats() throws Exception {
        TestUrlRequestCallback callback = new TestUrlRequestCallback();
        CronetEngine engine =
                new HttpEngineNativeProvider(ApplicationProvider.getApplicationContext())
                        .createBuilder()
                        .build();
        // Create request.
        UrlRequest.Builder builder =
                engine.newUrlRequestBuilder("localhost", callback, callback.getExecutor());
        AndroidUrlRequestWrapper urlRequest =
                (AndroidUrlRequestWrapper)
                        builder.setTrafficStatsUid(1000).setTrafficStatsTag(5000).build();
        assertThat(urlRequest.getTrafficStatsUid()).isEqualTo(1000);
        assertThat(urlRequest.getTrafficStatsTag()).isEqualTo(5000);
    }
}
