// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.net.impl;

import static com.google.common.truth.Truth.assertThat;
import static com.google.common.truth.TruthJUnit.assume;

import static org.junit.Assert.assertThrows;
import static org.mockito.Mockito.mock;
import static org.mockito.Mockito.never;
import static org.mockito.Mockito.times;
import static org.mockito.Mockito.verify;

import android.os.Build;
import android.os.PersistableBundle;

import androidx.test.filters.SmallTest;

import org.junit.After;
import org.junit.Before;
import org.junit.Rule;
import org.junit.Test;
import org.junit.runner.RunWith;
import org.junit.runners.JUnit4;
import org.mockito.ArgumentCaptor;

import org.chromium.base.test.util.Batch;
import org.chromium.net.CronetTestFramework.CronetImplementation;
import org.chromium.net.CronetTestRule;
import org.chromium.net.CronetTestRule.IgnoreFor;
import org.chromium.net.NativeTestServer;
import org.chromium.net.TestUrlRequestCallback;
import org.chromium.net.UrlRequest;
import org.chromium.net.test.Type;

import java.util.List;

/** Tests for {@link CronetPccAuditLogger}. */
@Batch(Batch.UNIT_TESTS)
@RunWith(JUnit4.class)
public class CronetPccAuditLoggerTest {
    @Rule public final CronetTestRule mTestRule = CronetTestRule.withAutomaticEngineStartup();
    private CronetPccAuditLogger.PccSandboxManagerDelegate mMockDelegate;
    private NativeTestServer mServer;
    private String mUrl;
    private TestUrlRequestCallback mCallback;
    private UrlRequest.Builder mRequestBuilder;

    @Before
    public void setUp() {
        mMockDelegate = mock(CronetPccAuditLogger.PccSandboxManagerDelegate.class);
        CronetPccAuditLogger.setPccSandboxManagerDelegateForTesting(mMockDelegate);

        mServer = new NativeTestServer(mTestRule.getTestFramework().getContext(), Type.HTTP);
        mServer.start();
        mUrl = mServer.getEchoBodyURL();
        mCallback = new TestUrlRequestCallback();

        mRequestBuilder =
                mTestRule
                        .getTestFramework()
                        .getEngine()
                        .newUrlRequestBuilder(mUrl, mCallback, mCallback.getExecutor());
    }

    @After
    public void tearDown() {
        CronetPccAuditLogger.setIsPrivateComputeUidForTesting(null);
        mServer.close();
    }

    @Test
    @SmallTest
    public void testMaybeWrite_isPccUid_attemptsWrite() {
        assume().that(Build.VERSION.SDK_INT).isAtLeast(Build.VERSION_CODES.CINNAMON_BUN);

        CronetPccAuditLogger.setIsPrivateComputeUidForTesting(true);

        String testUrl = "https://example.com/path?param=value";
        CronetPccAuditLogger.maybeWrite(testUrl);

        ArgumentCaptor<PersistableBundle> bundleCaptor =
                ArgumentCaptor.forClass(PersistableBundle.class);
        verify(mMockDelegate, times(1)).writeToAuditLog(bundleCaptor.capture());

        PersistableBundle bundle = bundleCaptor.getValue();
        assertThat(bundle).isNotNull();
        assertThat(testUrl).isEqualTo(bundle.getString("url"));
        assertThat(bundle.getLong("timestamp")).isGreaterThan(0);
    }

    @Test
    @SmallTest
    public void testMaybeWrite_noIsPrivateComputeUid_throwsUninitialized() throws Exception {
        assume().that(Build.VERSION.SDK_INT).isAtLeast(Build.VERSION_CODES.CINNAMON_BUN);

        CronetPccAuditLogger.PccSandboxManagerDelegate mockDelegate =
                mock(CronetPccAuditLogger.PccSandboxManagerDelegate.class);
        CronetPccAuditLogger.setPccSandboxManagerDelegateForTesting(mockDelegate);
        CronetPccAuditLogger.setIsPrivateComputeUidForTesting(null);

        assertThrows(IllegalStateException.class, () -> CronetPccAuditLogger.maybeWrite(mUrl));
    }

    @Test
    @SmallTest
    @IgnoreFor(
            implementations = {CronetImplementation.FALLBACK, CronetImplementation.AOSP_PLATFORM},
            reason =
                    "Not testing against FALLBACK as it doesn't support PCC auditing. Not testing"
                        + " against AOSP_PLATFORM as the test has to change static state which is"
                        + " inaccessible when going through HttpEngine.")
    public void testStartRequest_isPrivateComputeUid_logsToAuditLog() throws Exception {
        assume().that(Build.VERSION.SDK_INT).isAtLeast(Build.VERSION_CODES.CINNAMON_BUN);

        CronetPccAuditLogger.setIsPrivateComputeUidForTesting(true);

        UrlRequest request = mRequestBuilder.build();
        request.start();
        mCallback.blockForDone();

        ArgumentCaptor<PersistableBundle> bundleCaptor =
                ArgumentCaptor.forClass(PersistableBundle.class);
        verify(mMockDelegate, times(1)).writeToAuditLog(bundleCaptor.capture());

        PersistableBundle bundle = bundleCaptor.getValue();
        assertThat(bundle).isNotNull();
        assertThat(mUrl).isEqualTo(bundle.getString("url"));
        assertThat(bundle.getLong("timestamp")).isGreaterThan(0);
    }

    @Test
    @SmallTest
    @IgnoreFor(
            implementations = {CronetImplementation.FALLBACK, CronetImplementation.AOSP_PLATFORM},
            reason =
                    "Not testing against FALLBACK as it doesn't support PCC auditing. Not testing"
                        + " against AOSP_PLATFORM as the test has to change static state which is"
                        + " inaccessible when going through HttpEngine.")
    public void testStartRequest_notAPrivateComputeUid_doesNotLogToAuditLog() throws Exception {
        assume().that(Build.VERSION.SDK_INT).isAtLeast(Build.VERSION_CODES.CINNAMON_BUN);

        CronetPccAuditLogger.setIsPrivateComputeUidForTesting(false);

        UrlRequest request = mRequestBuilder.build();
        request.start();
        mCallback.blockForDone();

        ArgumentCaptor<PersistableBundle> bundleCaptor =
                ArgumentCaptor.forClass(PersistableBundle.class);
        verify(mMockDelegate, never()).writeToAuditLog(bundleCaptor.capture());
    }

    @Test
    @SmallTest
    @IgnoreFor(
            implementations = {CronetImplementation.FALLBACK, CronetImplementation.AOSP_PLATFORM},
            reason =
                    "Not testing against FALLBACK as it doesn't support PCC auditing. Not testing"
                        + " against AOSP_PLATFORM as the test has to change static state which is"
                        + " inaccessible when going through HttpEngine.")
    public void testStartRequest_redirect_logsAllUrlsToAuditLog() throws Exception {
        assume().that(Build.VERSION.SDK_INT).isAtLeast(Build.VERSION_CODES.CINNAMON_BUN);

        CronetPccAuditLogger.setIsPrivateComputeUidForTesting(true);

        String redirectUrl = mServer.getRedirectURL();
        String successUrl = mServer.getSuccessURL();

        UrlRequest.Builder builder =
                mTestRule
                        .getTestFramework()
                        .getEngine()
                        .newUrlRequestBuilder(redirectUrl, mCallback, mCallback.getExecutor());
        UrlRequest request = builder.build();
        request.start();
        mCallback.blockForDone();

        ArgumentCaptor<PersistableBundle> bundleCaptor =
                ArgumentCaptor.forClass(PersistableBundle.class);
        verify(mMockDelegate, times(2)).writeToAuditLog(bundleCaptor.capture());

        List<PersistableBundle> bundles = bundleCaptor.getAllValues();
        assertThat(bundles).hasSize(2);

        assertThat(redirectUrl).isEqualTo(bundles.get(0).getString("url"));
        assertThat(bundles.get(0).getLong("timestamp")).isGreaterThan(0);

        assertThat(successUrl).isEqualTo(bundles.get(1).getString("url"));
        assertThat(bundles.get(1).getLong("timestamp")).isGreaterThan(0);
    }
}
