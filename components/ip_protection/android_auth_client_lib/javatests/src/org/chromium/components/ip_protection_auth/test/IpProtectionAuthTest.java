// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.components.ip_protection_auth.test;

import static com.google.common.truth.Truth.assertThat;

import static org.junit.Assert.fail;
import static org.junit.Assume.assumeTrue;

import android.os.Build;
import android.os.ConditionVariable;

import androidx.annotation.NonNull;
import androidx.test.filters.MediumTest;

import com.google.privacy.ppn.proto.AuthAndSignRequest;
import com.google.privacy.ppn.proto.AuthAndSignResponse;
import com.google.privacy.ppn.proto.GetInitialDataRequest;
import com.google.privacy.ppn.proto.GetInitialDataResponse;

import org.junit.Before;
import org.junit.Test;
import org.junit.runner.RunWith;

import org.chromium.base.ThreadUtils;
import org.chromium.base.library_loader.LibraryLoader;
import org.chromium.base.test.BaseJUnit4ClassRunner;
import org.chromium.base.test.util.Batch;
import org.chromium.components.ip_protection_auth.IpProtectionAuthClient;
import org.chromium.components.ip_protection_auth.IpProtectionAuthServiceCallback;
import org.chromium.components.ip_protection_auth.IpProtectionByteArrayCallback;

/**
 * Tests for IpProtectionAuthClient and associated classes.
 *
 * <p>These tests mostly call into native code (ip_protection_auth_test_natives.cc) and interact
 * with "mock" services hosted in a secondary APK.
 *
 * <p>The usage of native test code for Java-hosted tests along with using native functionality like
 * RunLoop and CHECK has the potential to make any test failures more confusing, including native
 * crashes rather than Java AssertionErrors and global task state contamination across unrelated
 * test suites. As such, these tests are batched PER_CLASS to isolate such failures.
 */
@MediumTest
@RunWith(BaseJUnit4ClassRunner.class)
@Batch(Batch.PER_CLASS)
public final class IpProtectionAuthTest {
    private static final String ACTION = "android.net.http.IpProtectionAuthService";
    private static final String MOCK_PACKAGE_NAME = "org.chromium.components.ip_protection_auth";

    private static final String MOCK_CLASS_NAME_FOR_DEFAULT =
            MOCK_PACKAGE_NAME + ".mock_service.IpProtectionAuthServiceMock";
    private static final String MOCK_CLASS_NAME_FOR_NONEXISTANT =
            MOCK_PACKAGE_NAME + ".mock_service.IntentionallyNonexistantClass";
    private static final String MOCK_CLASS_NAME_FOR_NULL_BINDING =
            MOCK_PACKAGE_NAME + ".mock_service.NullBindingService";
    private static final String MOCK_CLASS_NAME_FOR_DISABLED =
            MOCK_PACKAGE_NAME + ".mock_service.NullBindingService$DisabledService";
    private static final String MOCK_CLASS_NAME_FOR_RESTRICTED =
            MOCK_PACKAGE_NAME + ".mock_service.NullBindingService$RestrictedService";

    private static final int TIMEOUT_MS = 10000;

    private class TestServiceCallback implements IpProtectionAuthServiceCallback {
        private final ConditionVariable mConditionVariable = new ConditionVariable();
        private IpProtectionAuthClient mClient;
        private String mError;

        @Override
        public void onResult(IpProtectionAuthClient client) {
            ThreadUtils.assertOnUiThread();
            assertThat(client).isNotNull();
            assertFirstTime();
            mClient = client;
            mConditionVariable.open();
        }

        @Override
        public void onError(String error) {
            ThreadUtils.assertOnUiThread();
            assertThat(error).isNotNull();
            assertFirstTime();
            mError = error;
            mConditionVariable.open();
        }

        private void assertFirstTime() {
            if (mClient != null || mError != null) {
                fail("service callback called multiple times");
            }
        }

        /**
         * Await and expect for a client with a short timeout.
         *
         * <p>TestServiceCallback never closes its contained auth client, so this must be done by
         * the test case.
         */
        @NonNull
        public IpProtectionAuthClient awaitResult() {
            assertThat(mConditionVariable.block(TIMEOUT_MS)).isTrue();
            assertThat(mError).isNull();
            assertThat(mClient).isNotNull();
            return mClient;
        }

        /** Await and expect an error with a short timeout. */
        @NonNull
        public String awaitError() {
            assertThat(mConditionVariable.block(TIMEOUT_MS)).isTrue();
            assertThat(mClient).isNull();
            assertThat(mError).isNotNull();
            return mError;
        }
    }

    private class TestByteArrayCallback implements IpProtectionByteArrayCallback {
        private final ConditionVariable mConditionVariable = new ConditionVariable();
        private byte[] mResult;
        private Integer mError;

        @Override
        public void onResult(byte[] result) {
            assertThat(result).isNotNull();
            assertFirstTime();
            mResult = result;
            mConditionVariable.open();
        }

        @Override
        public void onError(int error) {
            assertFirstTime();
            mError = new Integer(error);
            mConditionVariable.open();
        }

        private void assertFirstTime() {
            assertThat(mResult).isNull();
            assertThat(mError).isNull();
        }

        /** Await and expect a result with a short timeout. */
        @NonNull
        public byte[] awaitResult() {
            assertThat(mConditionVariable.block(TIMEOUT_MS)).isTrue();
            assertThat(mError).isNull();
            assertThat(mResult).isNotNull();
            return mResult;
        }

        /** Await and expect for an error with a short timeout. */
        @NonNull
        public int awaitError() {
            assertThat(mConditionVariable.block(TIMEOUT_MS)).isTrue();
            assertThat(mResult).isNull();
            assertThat(mError).isNotNull();
            return mError.intValue();
        }
    }

    /**
     * Get an IpProtectionAuthClient which binds to the default variant of the mock service.
     *
     * @return The auth client
     */
    private IpProtectionAuthClient getClient() {
        return getClientForMock(MOCK_CLASS_NAME_FOR_DEFAULT);
    }

    /**
     * Get an IpProtectionAuthClient which binds to a specific variant of the mock service.
     *
     * @return The auth client
     */
    @NonNull
    private IpProtectionAuthClient getClientForMock(String mockServiceClassName) {
        TestServiceCallback callback = new TestServiceCallback();
        IpProtectionAuthClient.createConnectedInstanceForTesting(
                MOCK_PACKAGE_NAME, mockServiceClassName, callback);
        return callback.awaitResult();
    }

    /**
     * Attempt to create and bind an IpProtectionAuthClient to a specific mock service, asserting
     * the bind fails.
     *
     * @return Error description
     */
    @NonNull
    private String getErrorForMock(String mockServiceClassName) {
        TestServiceCallback callback = new TestServiceCallback();
        IpProtectionAuthClient.createConnectedInstanceForTesting(
                MOCK_PACKAGE_NAME, mockServiceClassName, callback);
        return callback.awaitError();
    }

    @Before
    public void setUp() throws Exception {
        LibraryLoader.getInstance().ensureInitialized();
    }

    // Tests a normal case where the service is available.
    @Test
    public void testBindableService_GetsClient() throws Exception {
        try (IpProtectionAuthClient client = getClient()) {}
    }

    // Tests a normal case where the service isn't installed.
    @Test
    public void testNonexistantService_GetsError() throws Exception {
        String error = getErrorForMock(MOCK_CLASS_NAME_FOR_NONEXISTANT);

        assertThat(error)
                .isEqualTo(
                        "Unable to locate the IP Protection authentication provider package ("
                                + ACTION
                                + " action). This is expected if the host system is not set up to"
                                + " provide IP Protection services.");
    }

    // Tests a normal case where the service is installed but rejects bindings, for example, due to
    // a feature flag.
    @Test
    public void testNullBindingService_GetsError() throws Exception {
        // API levels < 28 (Pie) do not support null bindings
        assumeTrue(Build.VERSION.SDK_INT >= Build.VERSION_CODES.P);

        String error = getErrorForMock(MOCK_CLASS_NAME_FOR_NULL_BINDING);

        assertThat(error).isEqualTo("Service returned null from onBind()");
    }

    // Tests an abnormal case where bindService would return false.
    @Test
    public void testDisabledService_GetsError() throws Exception {
        String error = getErrorForMock(MOCK_CLASS_NAME_FOR_DISABLED);

        assertThat(error).isEqualTo("bindService() failed: returned false");
    }

    // Tests an abnormal case where bindService fails with a SecurityException. Note that the only
    // permission which would apply in production is the INTERNET permission. IpProtectionAuthClient
    // code shouldn't be reached if the app doesn't have INTERNET permission. This test case instead
    // tests for resiliance against an unusual service implementation with unexpected restrictions.
    @Test
    public void testNoPermission_GetsError() throws Exception {
        String error = getErrorForMock(MOCK_CLASS_NAME_FOR_RESTRICTED);

        // The full error message is constructed largely in AOSP source code, so just check for the
        // parts which we expect Chromium code to produce.
        assertThat(error).contains("Failed to bind service: java.lang.SecurityException: ");
    }

    @Test
    public void getInitialDataTest() throws Exception {
        try (IpProtectionAuthClient client = getClient()) {
            TestByteArrayCallback getInitialDataCallback = new TestByteArrayCallback();
            // TODO(b/344853279): This request is missing required fields. It should be updated.
            GetInitialDataRequest request =
                    GetInitialDataRequest.newBuilder().setServiceType("webviewipblinding").build();

            client.getInitialData(request.toByteArray(), getInitialDataCallback);

            byte[] response = getInitialDataCallback.awaitResult();
            // Parse should succeed and not throw
            GetInitialDataResponse.parseFrom(response);
        }
    }

    @Test
    public void authAndSignTest() throws Exception {
        try (IpProtectionAuthClient client = getClient()) {
            TestByteArrayCallback authAndSignCallback = new TestByteArrayCallback();
            // TODO(b/344853279): This request has deprecated OAuth field and is missing required
            // fields. It should be updated.
            AuthAndSignRequest request =
                    AuthAndSignRequest.newBuilder().setOauthToken("test").build();

            client.authAndSign(request.toByteArray(), authAndSignCallback);

            byte[] response = authAndSignCallback.awaitResult();
            // Parse should succeed and not throw
            AuthAndSignResponse.parseFrom(response);
        }
    }

    @Test
    public void nativeCreateConnectedInstanceTest() throws Exception {
        IpProtectionAuthTestNatives.createConnectedInstanceForTesting();
    }

    @Test
    public void nativeGetInitialDataTest() throws Exception {
        IpProtectionAuthTestNatives.testGetInitialData();
    }

    @Test
    public void nativeAuthAndSignTest() throws Exception {
        IpProtectionAuthTestNatives.testAuthAndSign();
    }

    @Test
    public void nativeTransientErrorTest() throws Exception {
        IpProtectionAuthTestNatives.testTransientError();
    }

    @Test
    public void nativePersistentErrorTest() throws Exception {
        IpProtectionAuthTestNatives.testPersistentError();
    }

    @Test
    public void nativeIllegalErrorCodeTest() throws Exception {
        IpProtectionAuthTestNatives.testIllegalErrorCode();
    }

    @Test
    public void nativeNullResponseTest() throws Exception {
        IpProtectionAuthTestNatives.testNullResponse();
    }

    @Test
    public void nativeUnparsableResponseTest() throws Exception {
        IpProtectionAuthTestNatives.testUnparsableResponse();
    }

    @Test
    public void nativeSynchronousErrorTest() throws Exception {
        IpProtectionAuthTestNatives.testSynchronousError();
    }

    @Test
    public void nativeUnresolvedWhenClosedTest() throws Exception {
        IpProtectionAuthTestNatives.testUnresolvedWhenClosed();
    }

    @Test
    public void nativeCrashOnRequestSyncWithoutResponse() throws Exception {
        IpProtectionAuthTestNatives.testCrashOnRequestSyncWithoutResponse();
    }

    @Test
    public void nativeCrashOnRequestAsyncWithoutResponse() throws Exception {
        IpProtectionAuthTestNatives.testCrashOnRequestAsyncWithoutResponse();
    }

    @Test
    public void nativeCrashOnRequestSyncWithResponse() throws Exception {
        IpProtectionAuthTestNatives.testCrashOnRequestSyncWithResponse();
    }

    @Test
    public void nativeUnresolvedCallbacksRejectedAfterCrash() throws Exception {
        IpProtectionAuthTestNatives.testUnresolvedCallbacksRejectedAfterCrash();
    }
}
