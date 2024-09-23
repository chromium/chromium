// Copyright 2015 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.net;

import static com.google.common.truth.Truth.assertThat;
import static com.google.common.truth.Truth.assertWithMessage;

import static org.junit.Assert.assertThrows;
import static org.junit.Assert.fail;

import static org.chromium.net.Http2TestServer.SERVER_CERT_PEM;
import static org.chromium.net.truth.UrlResponseInfoSubject.assertThat;

import android.os.Build;

import androidx.test.ext.junit.runners.AndroidJUnit4;
import androidx.test.filters.SmallTest;

import org.json.JSONObject;
import org.junit.After;
import org.junit.Before;
import org.junit.Rule;
import org.junit.Test;
import org.junit.runner.RunWith;

import org.chromium.base.test.util.DoNotBatch;
import org.chromium.net.CronetTestRule.CronetImplementation;
import org.chromium.net.CronetTestRule.CronetTestFramework;
import org.chromium.net.CronetTestRule.IgnoreFor;
import org.chromium.net.test.util.CertTestUtil;

import java.io.ByteArrayInputStream;
import java.security.cert.CertificateFactory;
import java.security.cert.X509Certificate;
import java.util.Arrays;
import java.util.Calendar;
import java.util.Date;
import java.util.HashSet;
import java.util.Set;

/** Public-Key-Pinning tests of Cronet Java API. */
@DoNotBatch(reason = "crbug/1459563")
@RunWith(AndroidJUnit4.class)
@IgnoreFor(
        implementations = {CronetImplementation.FALLBACK},
        reason = "The fallback implementation does not support public key pinning")
public class PkpTest {
    private static final int DISTANT_FUTURE = Integer.MAX_VALUE;
    private static final boolean INCLUDE_SUBDOMAINS = true;
    private static final boolean EXCLUDE_SUBDOMAINS = false;
    private static final boolean ENABLE_PINNING_BYPASS_FOR_LOCAL_ANCHORS = true;
    private static final boolean DISABLE_PINNING_BYPASS_FOR_LOCAL_ANCHORS = false;

    @Rule public final CronetTestRule mTestRule = CronetTestRule.withManualEngineStartup();

    @Before
    public void setUp() throws Exception {
        assertThat(Http2TestServer.startHttp2TestServer(mTestRule.getTestFramework().getContext()))
                .isTrue();
    }

    @After
    public void tearDown() throws Exception {
        assertThat(Http2TestServer.shutdownHttp2TestServer()).isTrue();
    }

    /**
     * Tests the case when the pin hash does not match. The client is expected to receive the error
     * response.
     */
    @Test
    @SmallTest
    public void testErrorCodeIfPinDoesNotMatch() throws Exception {
        applyCronetEngineBuilderConfigurationPatch(
                mTestRule.getTestFramework(), DISABLE_PINNING_BYPASS_FOR_LOCAL_ANCHORS);
        byte[] nonMatchingHash = generateSomeSha256();
        applyPkpSha256Patch(
                mTestRule.getTestFramework(),
                Http2TestServer.getServerHost(),
                nonMatchingHash,
                EXCLUDE_SUBDOMAINS,
                DISTANT_FUTURE);
        ExperimentalCronetEngine engine = mTestRule.getTestFramework().startEngine();
        TestUrlRequestCallback callback = new TestUrlRequestCallback();
        UrlRequest.Builder requestBuilder =
                engine.newUrlRequestBuilder(
                        Http2TestServer.getServerUrl(), callback, callback.getExecutor());
        requestBuilder.build().start();
        callback.blockForDone();

        assertErrorResponse(callback);
    }

    /**
     * Tests the case when the pin hash matches. The client is expected to receive the successful
     * response with the response code 200.
     */
    @Test
    @SmallTest
    public void testSuccessIfPinMatches() throws Exception {
        applyCronetEngineBuilderConfigurationPatch(
                mTestRule.getTestFramework(), DISABLE_PINNING_BYPASS_FOR_LOCAL_ANCHORS);
        // Get PKP hash of the real certificate
        X509Certificate cert = readCertFromFileInPemFormat(SERVER_CERT_PEM);
        byte[] matchingHash = CertTestUtil.getPublicKeySha256(cert);

        applyPkpSha256Patch(
                mTestRule.getTestFramework(),
                Http2TestServer.getServerHost(),
                matchingHash,
                EXCLUDE_SUBDOMAINS,
                DISTANT_FUTURE);
        ExperimentalCronetEngine engine = mTestRule.getTestFramework().startEngine();
        TestUrlRequestCallback callback = new TestUrlRequestCallback();
        UrlRequest.Builder requestBuilder =
                engine.newUrlRequestBuilder(
                        Http2TestServer.getServerUrl(), callback, callback.getExecutor());
        requestBuilder.build().start();
        callback.blockForDone();

        assertSuccessfulResponse(callback);
    }

    /**
     * Tests the case when the pin hash does not match and the client accesses the subdomain of the
     * configured PKP host with includeSubdomains flag set to true. The client is expected to
     * receive the error response.
     */
    @Test
    @SmallTest
    @IgnoreFor(
            implementations = {CronetImplementation.AOSP_PLATFORM},
            reason =
                    "Requires the use of subdomains. This can currently only be done through"
                            + " HostResolverRules, which fakes hostname resultion."
                            + " TODO(crbug.com/40941277): Enable for HttpEngine once we have"
                            + " hostname resolution")
    public void testIncludeSubdomainsFlagEqualTrue() throws Exception {
        String fakeUrl = "https://test.example.com:8443";
        String fakeDomain = "example.com";

        applyCronetEngineBuilderConfigurationPatchWithMockCertVerifier(
                mTestRule.getTestFramework(), DISABLE_PINNING_BYPASS_FOR_LOCAL_ANCHORS);
        byte[] nonMatchingHash = generateSomeSha256();
        applyPkpSha256Patch(
                mTestRule.getTestFramework(),
                fakeDomain,
                nonMatchingHash,
                INCLUDE_SUBDOMAINS,
                DISTANT_FUTURE);
        ExperimentalCronetEngine engine = mTestRule.getTestFramework().startEngine();

        TestUrlRequestCallback callback = new TestUrlRequestCallback();

        UrlRequest.Builder requestBuilder =
                engine.newUrlRequestBuilder(fakeUrl, callback, callback.getExecutor());
        requestBuilder.build().start();
        callback.blockForDone();

        assertErrorResponse(callback);
    }

    /**
     * Tests the case when the pin hash does not match and the client accesses the subdomain of the
     * configured PKP host with includeSubdomains flag set to false. The client is expected to
     * receive the successful response with the response code 200.
     */
    @Test
    @SmallTest
    @IgnoreFor(
            implementations = {CronetImplementation.AOSP_PLATFORM},
            reason =
                    "Requires the use of subdomains. This can currently only be done through"
                            + " HostResolverRules, which fakes hostname resultion."
                            + " TODO(crbug.com/40941277): Enable for HttpEngine once we have"
                            + " hostname resolution")
    public void testIncludeSubdomainsFlagEqualFalse() throws Exception {
        String fakeUrl = "https://test.example.com:8443";
        String fakeDomain = "example.com";

        applyCronetEngineBuilderConfigurationPatchWithMockCertVerifier(
                mTestRule.getTestFramework(), DISABLE_PINNING_BYPASS_FOR_LOCAL_ANCHORS);
        byte[] nonMatchingHash = generateSomeSha256();
        applyPkpSha256Patch(
                mTestRule.getTestFramework(),
                fakeDomain,
                nonMatchingHash,
                EXCLUDE_SUBDOMAINS,
                DISTANT_FUTURE);
        ExperimentalCronetEngine engine = mTestRule.getTestFramework().startEngine();

        TestUrlRequestCallback callback = new TestUrlRequestCallback();

        UrlRequest.Builder requestBuilder =
                engine.newUrlRequestBuilder(fakeUrl, callback, callback.getExecutor());
        requestBuilder.build().start();
        callback.blockForDone();

        assertSuccessfulResponse(callback);
    }

    /**
     * Tests the case when the mismatching pin is set for some host that is different from the one
     * the client wants to access. In that case the other host pinning policy should not be applied
     * and the client is expected to receive the successful response with the response code 200.
     */
    @Test
    @SmallTest
    public void testSuccessIfNoPinSpecified() throws Exception {
        applyCronetEngineBuilderConfigurationPatch(
                mTestRule.getTestFramework(), DISABLE_PINNING_BYPASS_FOR_LOCAL_ANCHORS);
        byte[] nonMatchingHash = generateSomeSha256();
        applyPkpSha256Patch(
                mTestRule.getTestFramework(),
                "otherhost.com",
                nonMatchingHash,
                INCLUDE_SUBDOMAINS,
                DISTANT_FUTURE);
        ExperimentalCronetEngine engine = mTestRule.getTestFramework().startEngine();
        TestUrlRequestCallback callback = new TestUrlRequestCallback();
        UrlRequest.Builder requestBuilder =
                engine.newUrlRequestBuilder(
                        Http2TestServer.getServerUrl(), callback, callback.getExecutor());
        requestBuilder.build().start();
        callback.blockForDone();

        assertSuccessfulResponse(callback);
    }

    /**
     * Tests mismatching pins that will expire in 10 seconds. The pins should be still valid and
     * enforced during the request; thus returning PIN mismatch error.
     */
    @Test
    @SmallTest
    public void testSoonExpiringPin() throws Exception {
        applyCronetEngineBuilderConfigurationPatch(
                mTestRule.getTestFramework(), DISABLE_PINNING_BYPASS_FOR_LOCAL_ANCHORS);
        final int tenSecondsAhead = 10;
        byte[] nonMatchingHash = generateSomeSha256();
        applyPkpSha256Patch(
                mTestRule.getTestFramework(),
                Http2TestServer.getServerHost(),
                nonMatchingHash,
                EXCLUDE_SUBDOMAINS,
                tenSecondsAhead);
        ExperimentalCronetEngine engine = mTestRule.getTestFramework().startEngine();
        TestUrlRequestCallback callback = new TestUrlRequestCallback();
        UrlRequest.Builder requestBuilder =
                engine.newUrlRequestBuilder(
                        Http2TestServer.getServerUrl(), callback, callback.getExecutor());
        requestBuilder.build().start();
        callback.blockForDone();

        assertErrorResponse(callback);
    }

    /**
     * Tests mismatching pins that expired 1 second ago. Since the pins have expired, they should
     * not be enforced during the request; thus a successful response is expected.
     */
    @Test
    @SmallTest
    public void testRecentlyExpiredPin() throws Exception {
        applyCronetEngineBuilderConfigurationPatch(
                mTestRule.getTestFramework(), DISABLE_PINNING_BYPASS_FOR_LOCAL_ANCHORS);
        final int oneSecondAgo = -1;
        byte[] nonMatchingHash = generateSomeSha256();
        applyPkpSha256Patch(
                mTestRule.getTestFramework(),
                Http2TestServer.getServerHost(),
                nonMatchingHash,
                EXCLUDE_SUBDOMAINS,
                oneSecondAgo);
        ExperimentalCronetEngine engine = mTestRule.getTestFramework().startEngine();
        TestUrlRequestCallback callback = new TestUrlRequestCallback();
        UrlRequest.Builder requestBuilder =
                engine.newUrlRequestBuilder(
                        Http2TestServer.getServerUrl(), callback, callback.getExecutor());
        requestBuilder.build().start();
        callback.blockForDone();

        assertSuccessfulResponse(callback);
    }

    /**
     * Tests that the pinning of local trust anchors is enforced when pinning bypass for local trust
     * anchors is disabled.
     */
    @Test
    @SmallTest
    public void testLocalTrustAnchorPinningEnforced() throws Exception {
        applyCronetEngineBuilderConfigurationPatch(
                mTestRule.getTestFramework(), DISABLE_PINNING_BYPASS_FOR_LOCAL_ANCHORS);
        byte[] nonMatchingHash = generateSomeSha256();
        applyPkpSha256Patch(
                mTestRule.getTestFramework(),
                Http2TestServer.getServerHost(),
                nonMatchingHash,
                EXCLUDE_SUBDOMAINS,
                DISTANT_FUTURE);
        ExperimentalCronetEngine engine = mTestRule.getTestFramework().startEngine();
        TestUrlRequestCallback callback = new TestUrlRequestCallback();
        UrlRequest.Builder requestBuilder =
                engine.newUrlRequestBuilder(
                        Http2TestServer.getServerUrl(), callback, callback.getExecutor());
        requestBuilder.build().start();
        callback.blockForDone();

        assertErrorResponse(callback);
    }

    /**
     * Tests that the pinning of local trust anchors is not enforced when pinning bypass for local
     * trust anchors is enabled.
     */
    @Test
    @SmallTest
    public void testLocalTrustAnchorPinningNotEnforced() throws Exception {
        applyCronetEngineBuilderConfigurationPatch(
                mTestRule.getTestFramework(), ENABLE_PINNING_BYPASS_FOR_LOCAL_ANCHORS);
        byte[] nonMatchingHash = generateSomeSha256();
        applyPkpSha256Patch(
                mTestRule.getTestFramework(),
                Http2TestServer.getServerHost(),
                nonMatchingHash,
                EXCLUDE_SUBDOMAINS,
                DISTANT_FUTURE);
        ExperimentalCronetEngine engine = mTestRule.getTestFramework().startEngine();
        TestUrlRequestCallback callback = new TestUrlRequestCallback();
        UrlRequest.Builder requestBuilder =
                engine.newUrlRequestBuilder(
                        Http2TestServer.getServerUrl(), callback, callback.getExecutor());
        requestBuilder.build().start();
        callback.blockForDone();

        assertSuccessfulResponse(callback);
    }

    /** Tests that host pinning is not persisted between multiple CronetEngine instances. */
    @Test
    @SmallTest
    public void testPinsAreNotPersisted() throws Exception {
        applyCronetEngineBuilderConfigurationPatch(
                mTestRule.getTestFramework(), DISABLE_PINNING_BYPASS_FOR_LOCAL_ANCHORS);
        byte[] nonMatchingHash = generateSomeSha256();
        applyPkpSha256Patch(
                mTestRule.getTestFramework(),
                Http2TestServer.getServerHost(),
                nonMatchingHash,
                EXCLUDE_SUBDOMAINS,
                DISTANT_FUTURE);
        ExperimentalCronetEngine engine = mTestRule.getTestFramework().startEngine();
        TestUrlRequestCallback callback = new TestUrlRequestCallback();
        UrlRequest.Builder requestBuilder =
                engine.newUrlRequestBuilder(
                        Http2TestServer.getServerUrl(), callback, callback.getExecutor());
        requestBuilder.build().start();
        callback.blockForDone();
        assertErrorResponse(callback);

        // Restart Cronet engine and try the same request again. Since the pins are not persisted,
        // a successful response is expected.
        engine.shutdown();
        ExperimentalCronetEngine.Builder builder =
                mTestRule
                        .getTestFramework()
                        .createNewSecondaryBuilder(mTestRule.getTestFramework().getContext());

        configureCronetEngineBuilder(builder, DISABLE_PINNING_BYPASS_FOR_LOCAL_ANCHORS);
        engine = builder.build();
        callback = new TestUrlRequestCallback();
        requestBuilder =
                engine.newUrlRequestBuilder(
                        Http2TestServer.getServerUrl(), callback, callback.getExecutor());
        requestBuilder.build().start();
        callback.blockForDone();
        assertSuccessfulResponse(callback);
    }

    /**
     * Tests that the client receives {@code InvalidArgumentException} when the pinned host name is
     * invalid.
     */
    @Test
    @SmallTest
    public void testHostNameArgumentValidation() throws Exception {
        applyCronetEngineBuilderConfigurationPatch(
                mTestRule.getTestFramework(), DISABLE_PINNING_BYPASS_FOR_LOCAL_ANCHORS);
        final String label63 = "123456789-123456789-123456789-123456789-123456789-123456789-123";
        final String host255 = label63 + "." + label63 + "." + label63 + "." + label63;
        // Valid host names.
        assertNoExceptionWhenHostNameIsValid(mTestRule.getTestFramework(), "domain.com");
        assertNoExceptionWhenHostNameIsValid(mTestRule.getTestFramework(), "my-domain.com");
        assertNoExceptionWhenHostNameIsValid(mTestRule.getTestFramework(), "section4.domain.info");
        assertNoExceptionWhenHostNameIsValid(mTestRule.getTestFramework(), "44.domain44.info");
        assertNoExceptionWhenHostNameIsValid(
                mTestRule.getTestFramework(), "very.long.long.long.long.long.long.long.domain.com");
        assertNoExceptionWhenHostNameIsValid(mTestRule.getTestFramework(), "host");
        assertNoExceptionWhenHostNameIsValid(mTestRule.getTestFramework(), "новости.ру");
        assertNoExceptionWhenHostNameIsValid(
                mTestRule.getTestFramework(), "самые-последние.новости.рус");
        assertNoExceptionWhenHostNameIsValid(mTestRule.getTestFramework(), "最新消息.中国");
        // Checks max size of the host label (63 characters)
        assertNoExceptionWhenHostNameIsValid(mTestRule.getTestFramework(), label63 + ".com");
        // Checks max size of the host name (255 characters)
        assertNoExceptionWhenHostNameIsValid(mTestRule.getTestFramework(), host255);
        assertNoExceptionWhenHostNameIsValid(mTestRule.getTestFramework(), "127.0.0.z");

        // Invalid host names.
        assertExceptionWhenHostNameIsInvalid(mTestRule.getTestFramework(), "domain.com:300");
        assertExceptionWhenHostNameIsInvalid(mTestRule.getTestFramework(), "-domain.com");
        assertExceptionWhenHostNameIsInvalid(mTestRule.getTestFramework(), "domain-.com");
        assertExceptionWhenHostNameIsInvalid(mTestRule.getTestFramework(), "http://domain.com");
        assertExceptionWhenHostNameIsInvalid(mTestRule.getTestFramework(), "domain.com:");
        assertExceptionWhenHostNameIsInvalid(mTestRule.getTestFramework(), "domain.com/");
        assertExceptionWhenHostNameIsInvalid(mTestRule.getTestFramework(), "новости.ру:");
        assertExceptionWhenHostNameIsInvalid(mTestRule.getTestFramework(), "новости.ру/");
        assertExceptionWhenHostNameIsInvalid(
                mTestRule.getTestFramework(), "_http.sctp.www.example.com");
        assertExceptionWhenHostNameIsInvalid(
                mTestRule.getTestFramework(), "http.sctp._www.example.com");
        // Checks a host that exceeds max allowed length of the host label (63 characters)
        assertExceptionWhenHostNameIsInvalid(mTestRule.getTestFramework(), label63 + "4.com");
        // Checks a host that exceeds max allowed length of hostname (255 characters)
        assertExceptionWhenHostNameIsInvalid(
                mTestRule.getTestFramework(), host255.substring(3) + ".com");
        assertExceptionWhenHostNameIsInvalid(
                mTestRule.getTestFramework(), "FE80:0000:0000:0000:0202:B3FF:FE1E:8329");
        assertExceptionWhenHostNameIsInvalid(mTestRule.getTestFramework(), "[2001:db8:0:1]:80");

        // Invalid host names for PKP that contain IPv4 addresses
        // or names with digits and dots only.
        assertExceptionWhenHostNameIsInvalid(mTestRule.getTestFramework(), "127.0.0.1");
        assertExceptionWhenHostNameIsInvalid(mTestRule.getTestFramework(), "68.44.222.12");
        assertExceptionWhenHostNameIsInvalid(mTestRule.getTestFramework(), "256.0.0.1");
        assertExceptionWhenHostNameIsInvalid(mTestRule.getTestFramework(), "127.0.0.1.1");
        assertExceptionWhenHostNameIsInvalid(mTestRule.getTestFramework(), "127.0.0");
        assertExceptionWhenHostNameIsInvalid(mTestRule.getTestFramework(), "127.0.0.");
        assertExceptionWhenHostNameIsInvalid(mTestRule.getTestFramework(), "127.0.0.299");
    }

    /**
     * Tests that NullPointerException is thrown if the host name or the collection of pins or the
     * expiration date is null.
     */
    @Test
    @SmallTest
    public void testNullArguments() throws Exception {
        applyCronetEngineBuilderConfigurationPatch(
                mTestRule.getTestFramework(), DISABLE_PINNING_BYPASS_FOR_LOCAL_ANCHORS);
        verifyExceptionWhenAddPkpArgumentIsNull(mTestRule.getTestFramework(), true, false, false);
        verifyExceptionWhenAddPkpArgumentIsNull(mTestRule.getTestFramework(), false, true, false);
        verifyExceptionWhenAddPkpArgumentIsNull(mTestRule.getTestFramework(), false, false, true);
        verifyExceptionWhenAddPkpArgumentIsNull(mTestRule.getTestFramework(), false, false, false);
    }

    /** Tests that IllegalArgumentException is thrown if SHA1 is passed as the value of a pin. */
    @Test
    @SmallTest
    public void testIllegalArgumentExceptionWhenPinValueIsSHA1() throws Exception {
        applyCronetEngineBuilderConfigurationPatch(
                mTestRule.getTestFramework(), DISABLE_PINNING_BYPASS_FOR_LOCAL_ANCHORS);
        byte[] sha1 = new byte[20];
        assertThrows(
                "Pin value was: " + Arrays.toString(sha1),
                IllegalArgumentException.class,
                () ->
                        applyPkpSha256Patch(
                                mTestRule.getTestFramework(),
                                Http2TestServer.getServerHost(),
                                sha1,
                                EXCLUDE_SUBDOMAINS,
                                DISTANT_FUTURE));
    }

    /** Asserts that the response from the server contains an PKP error. */
    private void assertErrorResponse(TestUrlRequestCallback callback) {
        assertThat(callback.mError).isNotNull();
        // NetworkException#getCronetInternalErrorCode is exposed only by the native implementation.
        if (mTestRule.implementationUnderTest() != CronetImplementation.STATICALLY_LINKED) {
            return;
        }

        int errorCode = ((NetworkException) callback.mError).getCronetInternalErrorCode();
        Set<Integer> expectedErrors = new HashSet<>();
        expectedErrors.add(NetError.ERR_CONNECTION_REFUSED);
        expectedErrors.add(NetError.ERR_SSL_PINNED_KEY_NOT_IN_CERT_CHAIN);
        assertWithMessage(
                        String.format(
                                "Incorrect error code. Expected one of %s but received %s",
                                expectedErrors, errorCode))
                .that(expectedErrors)
                .contains(errorCode);
    }

    /** Asserts a successful response with response code 200. */
    private void assertSuccessfulResponse(TestUrlRequestCallback callback) {
        if (callback.mError != null) {
            fail(
                    "Did not expect an error but got error code "
                            + ((NetworkException) callback.mError).getCronetInternalErrorCode());
        }
        assertWithMessage("Expected non-null response from the server")
                .that(callback.getResponseInfoWithChecks())
                .isNotNull();
        assertThat(callback.getResponseInfoWithChecks()).hasHttpStatusCodeThat().isEqualTo(200);
    }

    private static void applyCronetEngineBuilderConfigurationPatch(
            CronetTestFramework testFramework, boolean bypassPinningForLocalAnchors)
            throws Exception {
        testFramework.applyEngineBuilderPatch(
                (builder) -> configureCronetEngineBuilder(builder, bypassPinningForLocalAnchors));
    }

    private static void applyCronetEngineBuilderConfigurationPatchWithMockCertVerifier(
            CronetTestFramework testFramework, boolean bypassPinningForLocalAnchors)
            throws Exception {
        testFramework.applyEngineBuilderPatch(
                (builder) ->
                        configureCronetEngineBuilderWithMockCertVerifier(
                                builder, bypassPinningForLocalAnchors));
    }

    private static void enableMockCertVerifier(ExperimentalCronetEngine.Builder builder)
            throws Exception {
        final String[] server_certs = {SERVER_CERT_PEM};
        // knownRoot maps to net::CertVerifyResult.is_issued_by_known_root. There is no test which
        // depends on that value as the only thing it affects is certificate verification for QUIC
        // (where we never trust non web PKI certs, regardless of app/user config). Hence, always
        // set this to false to maintain consistency with the non-MockCertVerifier case, where we
        // use a non-trusted self signed certificate.
        CronetTestUtil.setMockCertVerifierForTesting(
                builder,
                MockCertVerifier.createMockCertVerifier(server_certs, /* knownRoot= */ false));
        // MockCertVerifier uses certificates with hostname != localhost. So, setup fake
        // hostname resolution.
        JSONObject hostResolverParams = CronetTestUtil.generateHostResolverRules();
        JSONObject experimentalOptions =
                new JSONObject().put("HostResolverRules", hostResolverParams);
        builder.setExperimentalOptions(experimentalOptions.toString());
    }

    private static void internalConfigureCronetEngineBuilder(
            ExperimentalCronetEngine.Builder builder,
            boolean bypassPinningForLocalAnchors,
            boolean useMockCertVerifier)
            throws Exception {
        builder.enablePublicKeyPinningBypassForLocalTrustAnchors(bypassPinningForLocalAnchors);
        // TODO(crbug.com/40284777): When not explicitly enabled, fall back to MockCertVerifier if
        // custom CAs are not supported.
        if (useMockCertVerifier || Build.VERSION.SDK_INT <= Build.VERSION_CODES.M) {
            enableMockCertVerifier(builder);
        }
    }

    private static void configureCronetEngineBuilder(
            ExperimentalCronetEngine.Builder builder, boolean bypassPinningForLocalAnchors)
            throws Exception {
        internalConfigureCronetEngineBuilder(
                builder, bypassPinningForLocalAnchors, /* useMockCertVerifier= */ false);
    }

    private static void configureCronetEngineBuilderWithMockCertVerifier(
            ExperimentalCronetEngine.Builder builder, boolean bypassPinningForLocalAnchors)
            throws Exception {
        internalConfigureCronetEngineBuilder(
                builder, bypassPinningForLocalAnchors, /* useMockCertVerifier= */ true);
    }

    private static byte[] generateSomeSha256() {
        byte[] sha256 = new byte[32];
        Arrays.fill(sha256, (byte) 58);
        return sha256;
    }

    @SuppressWarnings("ArrayAsKeyOfSetOrMap")
    private static void applyPkpSha256Patch(
            CronetTestFramework testFramework,
            String host,
            byte[] pinHashValue,
            boolean includeSubdomain,
            int maxAgeInSec) {
        Set<byte[]> hashes = new HashSet<>();
        hashes.add(pinHashValue);
        testFramework.applyEngineBuilderPatch(
                (builder) ->
                        builder.addPublicKeyPins(
                                host, hashes, includeSubdomain, dateInFuture(maxAgeInSec)));
    }

    private static X509Certificate readCertFromFileInPemFormat(String certFileName)
            throws Exception {
        byte[] certDer = CertTestUtil.pemToDer(CertTestUtil.CERTS_DIRECTORY + certFileName);
        CertificateFactory certFactory = CertificateFactory.getInstance("X.509");
        return (X509Certificate) certFactory.generateCertificate(new ByteArrayInputStream(certDer));
    }

    private static Date dateInFuture(int secondsIntoFuture) {
        Calendar cal = Calendar.getInstance();
        cal.add(Calendar.SECOND, secondsIntoFuture);
        return cal.getTime();
    }

    private static void assertNoExceptionWhenHostNameIsValid(
            CronetTestFramework testFramework, String hostName) {
        try {
            applyPkpSha256Patch(
                    testFramework,
                    hostName,
                    generateSomeSha256(),
                    INCLUDE_SUBDOMAINS,
                    DISTANT_FUTURE);
        } catch (IllegalArgumentException ex) {
            fail(
                    "Host name "
                            + hostName
                            + " should be valid but the exception was thrown: "
                            + ex.toString());
        }
    }

    private static void assertExceptionWhenHostNameIsInvalid(
            CronetTestFramework testFramework, String hostName) {
        assertThrows(
                "Hostname was " + hostName,
                IllegalArgumentException.class,
                () ->
                        applyPkpSha256Patch(
                                testFramework,
                                hostName,
                                generateSomeSha256(),
                                INCLUDE_SUBDOMAINS,
                                DISTANT_FUTURE));
    }

    @SuppressWarnings("ArrayAsKeyOfSetOrMap")
    private static void verifyExceptionWhenAddPkpArgumentIsNull(
            CronetTestFramework testFramework,
            boolean hostNameIsNull,
            boolean pinsAreNull,
            boolean expirationDataIsNull) {
        String hostName = hostNameIsNull ? null : "some-host.com";
        Set<byte[]> pins = pinsAreNull ? null : new HashSet<byte[]>();
        Date expirationDate = expirationDataIsNull ? null : new Date();

        boolean shouldThrowNpe = hostNameIsNull || pinsAreNull || expirationDataIsNull;
        if (shouldThrowNpe) {
            testFramework.applyEngineBuilderPatch(
                    (builder) ->
                            assertThrows(
                                    NullPointerException.class,
                                    () ->
                                            builder.addPublicKeyPins(
                                                    hostName,
                                                    pins,
                                                    INCLUDE_SUBDOMAINS,
                                                    expirationDate)));
        } else {
            testFramework.applyEngineBuilderPatch(
                    (builder) ->
                            builder.addPublicKeyPins(
                                    hostName, pins, INCLUDE_SUBDOMAINS, expirationDate));
        }
    }
}
