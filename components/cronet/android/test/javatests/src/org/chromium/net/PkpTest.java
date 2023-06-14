// Copyright 2015 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.net;

import static com.google.common.truth.Truth.assertThat;
import static com.google.common.truth.Truth.assertWithMessage;

import static org.junit.Assert.assertThrows;
import static org.junit.Assert.fail;

import static org.chromium.net.CronetTestRule.getTestStorage;
import static org.chromium.net.Http2TestServer.SERVER_CERT_PEM;

import androidx.test.ext.junit.runners.AndroidJUnit4;
import androidx.test.filters.SmallTest;

import org.json.JSONObject;
import org.junit.After;
import org.junit.Before;
import org.junit.Rule;
import org.junit.Test;
import org.junit.runner.RunWith;

import org.chromium.net.CronetTestRule.OnlyRunNativeCronet;
import org.chromium.net.test.util.CertTestUtil;

import java.io.ByteArrayInputStream;
import java.security.cert.CertificateFactory;
import java.security.cert.X509Certificate;
import java.util.Arrays;
import java.util.Calendar;
import java.util.Date;
import java.util.HashSet;
import java.util.Set;

/**
 * Public-Key-Pinning tests of Cronet Java API.
 */
@RunWith(AndroidJUnit4.class)
public class PkpTest {
    private static final int DISTANT_FUTURE = Integer.MAX_VALUE;
    private static final boolean INCLUDE_SUBDOMAINS = true;
    private static final boolean EXCLUDE_SUBDOMAINS = false;
    private static final boolean KNOWN_ROOT = true;
    private static final boolean UNKNOWN_ROOT = false;
    private static final boolean ENABLE_PINNING_BYPASS_FOR_LOCAL_ANCHORS = true;
    private static final boolean DISABLE_PINNING_BYPASS_FOR_LOCAL_ANCHORS = false;

    @Rule
    public final CronetTestRule mTestRule = CronetTestRule.withManualEngineStartup();

    private CronetEngine mCronetEngine;
    private ExperimentalCronetEngine.Builder mBuilder;
    private TestUrlRequestCallback mListener;
    private String mServerUrl; // https://test.example.com:8443
    private String mServerHost; // test.example.com
    private String mDomain; // example.com

    @Before
    public void setUp() throws Exception {
        if (mTestRule.testingJavaImpl()) {
            return;
        }
        System.loadLibrary("cronet_tests");
        assertThat(Http2TestServer.startHttp2TestServer(mTestRule.getTestFramework().getContext()))
                .isTrue();
        mServerHost = "test.example.com";
        mServerUrl = "https://" + mServerHost + ":" + Http2TestServer.getServerPort();
        mDomain = mServerHost.substring(mServerHost.indexOf('.') + 1, mServerHost.length());
    }

    @After
    public void tearDown() throws Exception {
        Http2TestServer.shutdownHttp2TestServer();
        shutdownCronetEngine();
    }

    /**
     * Tests the case when the pin hash does not match. The client is expected to
     * receive the error response.
     *
     * @throws Exception
     */
    @Test
    @SmallTest
    @OnlyRunNativeCronet
    public void testErrorCodeIfPinDoesNotMatch() throws Exception {
        createCronetEngineBuilder(ENABLE_PINNING_BYPASS_FOR_LOCAL_ANCHORS, KNOWN_ROOT);
        byte[] nonMatchingHash = generateSomeSha256();
        addPkpSha256(mServerHost, nonMatchingHash, EXCLUDE_SUBDOMAINS, DISTANT_FUTURE);
        startCronetEngine();
        sendRequestAndWaitForResult();

        assertErrorResponse();
    }

    /**
     * Tests the case when the pin hash matches. The client is expected to
     * receive the successful response with the response code 200.
     *
     * @throws Exception
     */
    @Test
    @SmallTest
    @OnlyRunNativeCronet
    public void testSuccessIfPinMatches() throws Exception {
        createCronetEngineBuilder(ENABLE_PINNING_BYPASS_FOR_LOCAL_ANCHORS, KNOWN_ROOT);
        // Get PKP hash of the real certificate
        X509Certificate cert = readCertFromFileInPemFormat(SERVER_CERT_PEM);
        byte[] matchingHash = CertTestUtil.getPublicKeySha256(cert);

        addPkpSha256(mServerHost, matchingHash, EXCLUDE_SUBDOMAINS, DISTANT_FUTURE);
        startCronetEngine();
        sendRequestAndWaitForResult();

        assertSuccessfulResponse();
    }

    /**
     * Tests the case when the pin hash does not match and the client accesses the subdomain of
     * the configured PKP host with includeSubdomains flag set to true. The client is
     * expected to receive the error response.
     *
     * @throws Exception
     */
    @Test
    @SmallTest
    @OnlyRunNativeCronet
    public void testIncludeSubdomainsFlagEqualTrue() throws Exception {
        createCronetEngineBuilder(ENABLE_PINNING_BYPASS_FOR_LOCAL_ANCHORS, KNOWN_ROOT);
        byte[] nonMatchingHash = generateSomeSha256();
        addPkpSha256(mDomain, nonMatchingHash, INCLUDE_SUBDOMAINS, DISTANT_FUTURE);
        startCronetEngine();
        sendRequestAndWaitForResult();

        assertErrorResponse();
    }

    /**
     * Tests the case when the pin hash does not match and the client accesses the subdomain of
     * the configured PKP host with includeSubdomains flag set to false. The client is expected to
     * receive the successful response with the response code 200.
     *
     * @throws Exception
     */
    @Test
    @SmallTest
    @OnlyRunNativeCronet
    public void testIncludeSubdomainsFlagEqualFalse() throws Exception {
        createCronetEngineBuilder(ENABLE_PINNING_BYPASS_FOR_LOCAL_ANCHORS, KNOWN_ROOT);
        byte[] nonMatchingHash = generateSomeSha256();
        addPkpSha256(mDomain, nonMatchingHash, EXCLUDE_SUBDOMAINS, DISTANT_FUTURE);
        startCronetEngine();
        sendRequestAndWaitForResult();

        assertSuccessfulResponse();
    }

    /**
     * Tests the case when the mismatching pin is set for some host that is different from the one
     * the client wants to access. In that case the other host pinning policy should not be applied
     * and the client is expected to receive the successful response with the response code 200.
     *
     * @throws Exception
     */
    @Test
    @SmallTest
    @OnlyRunNativeCronet
    public void testSuccessIfNoPinSpecified() throws Exception {
        createCronetEngineBuilder(ENABLE_PINNING_BYPASS_FOR_LOCAL_ANCHORS, KNOWN_ROOT);
        byte[] nonMatchingHash = generateSomeSha256();
        addPkpSha256("otherhost.com", nonMatchingHash, INCLUDE_SUBDOMAINS, DISTANT_FUTURE);
        startCronetEngine();
        sendRequestAndWaitForResult();

        assertSuccessfulResponse();
    }

    /**
     * Tests mismatching pins that will expire in 10 seconds. The pins should be still valid and
     * enforced during the request; thus returning PIN mismatch error.
     *
     * @throws Exception
     */
    @Test
    @SmallTest
    @OnlyRunNativeCronet
    public void testSoonExpiringPin() throws Exception {
        createCronetEngineBuilder(ENABLE_PINNING_BYPASS_FOR_LOCAL_ANCHORS, KNOWN_ROOT);
        final int tenSecondsAhead = 10;
        byte[] nonMatchingHash = generateSomeSha256();
        addPkpSha256(mServerHost, nonMatchingHash, EXCLUDE_SUBDOMAINS, tenSecondsAhead);
        startCronetEngine();
        sendRequestAndWaitForResult();

        assertErrorResponse();
    }

    /**
     * Tests mismatching pins that expired 1 second ago. Since the pins have expired, they
     * should not be enforced during the request; thus a successful response is expected.
     *
     * @throws Exception
     */
    @Test
    @SmallTest
    @OnlyRunNativeCronet
    public void testRecentlyExpiredPin() throws Exception {
        createCronetEngineBuilder(ENABLE_PINNING_BYPASS_FOR_LOCAL_ANCHORS, KNOWN_ROOT);
        final int oneSecondAgo = -1;
        byte[] nonMatchingHash = generateSomeSha256();
        addPkpSha256(mServerHost, nonMatchingHash, EXCLUDE_SUBDOMAINS, oneSecondAgo);
        startCronetEngine();
        sendRequestAndWaitForResult();

        assertSuccessfulResponse();
    }

    /**
     * Tests that the pinning of local trust anchors is enforced when pinning bypass for local
     * trust anchors is disabled.
     *
     * @throws Exception
     */
    @Test
    @SmallTest
    @OnlyRunNativeCronet
    public void testLocalTrustAnchorPinningEnforced() throws Exception {
        createCronetEngineBuilder(DISABLE_PINNING_BYPASS_FOR_LOCAL_ANCHORS, UNKNOWN_ROOT);
        byte[] nonMatchingHash = generateSomeSha256();
        addPkpSha256(mServerHost, nonMatchingHash, EXCLUDE_SUBDOMAINS, DISTANT_FUTURE);
        startCronetEngine();
        sendRequestAndWaitForResult();

        assertErrorResponse();
        shutdownCronetEngine();
    }

    /**
     * Tests that the pinning of local trust anchors is not enforced when pinning bypass for local
     * trust anchors is enabled.
     *
     * @throws Exception
     */
    @Test
    @SmallTest
    @OnlyRunNativeCronet
    public void testLocalTrustAnchorPinningNotEnforced() throws Exception {
        createCronetEngineBuilder(ENABLE_PINNING_BYPASS_FOR_LOCAL_ANCHORS, UNKNOWN_ROOT);
        byte[] nonMatchingHash = generateSomeSha256();
        addPkpSha256(mServerHost, nonMatchingHash, EXCLUDE_SUBDOMAINS, DISTANT_FUTURE);
        startCronetEngine();
        sendRequestAndWaitForResult();

        assertSuccessfulResponse();
        shutdownCronetEngine();
    }

    /**
     * Tests that host pinning is not persisted between multiple CronetEngine instances.
     *
     * @throws Exception
     */
    @Test
    @SmallTest
    @OnlyRunNativeCronet
    public void testPinsAreNotPersisted() throws Exception {
        createCronetEngineBuilder(ENABLE_PINNING_BYPASS_FOR_LOCAL_ANCHORS, KNOWN_ROOT);
        byte[] nonMatchingHash = generateSomeSha256();
        addPkpSha256(mServerHost, nonMatchingHash, EXCLUDE_SUBDOMAINS, DISTANT_FUTURE);
        startCronetEngine();
        sendRequestAndWaitForResult();
        assertErrorResponse();
        shutdownCronetEngine();

        // Restart Cronet engine and try the same request again. Since the pins are not persisted,
        // a successful response is expected.
        createCronetEngineBuilder(ENABLE_PINNING_BYPASS_FOR_LOCAL_ANCHORS, KNOWN_ROOT);
        startCronetEngine();
        sendRequestAndWaitForResult();
        assertSuccessfulResponse();
    }

    /**
     * Tests that the client receives {@code InvalidArgumentException} when the pinned host name
     * is invalid.
     *
     * @throws Exception
     */
    @Test
    @SmallTest
    public void testHostNameArgumentValidation() throws Exception {
        createCronetEngineBuilder(ENABLE_PINNING_BYPASS_FOR_LOCAL_ANCHORS, KNOWN_ROOT);
        final String label63 = "123456789-123456789-123456789-123456789-123456789-123456789-123";
        final String host255 = label63 + "." + label63 + "." + label63 + "." + label63;
        // Valid host names.
        assertNoExceptionWhenHostNameIsValid("domain.com");
        assertNoExceptionWhenHostNameIsValid("my-domain.com");
        assertNoExceptionWhenHostNameIsValid("section4.domain.info");
        assertNoExceptionWhenHostNameIsValid("44.domain44.info");
        assertNoExceptionWhenHostNameIsValid("very.long.long.long.long.long.long.long.domain.com");
        assertNoExceptionWhenHostNameIsValid("host");
        assertNoExceptionWhenHostNameIsValid("новости.ру");
        assertNoExceptionWhenHostNameIsValid("самые-последние.новости.рус");
        assertNoExceptionWhenHostNameIsValid("最新消息.中国");
        // Checks max size of the host label (63 characters)
        assertNoExceptionWhenHostNameIsValid(label63 + ".com");
        // Checks max size of the host name (255 characters)
        assertNoExceptionWhenHostNameIsValid(host255);
        assertNoExceptionWhenHostNameIsValid("127.0.0.z");

        // Invalid host names.
        assertExceptionWhenHostNameIsInvalid("domain.com:300");
        assertExceptionWhenHostNameIsInvalid("-domain.com");
        assertExceptionWhenHostNameIsInvalid("domain-.com");
        assertExceptionWhenHostNameIsInvalid("http://domain.com");
        assertExceptionWhenHostNameIsInvalid("domain.com:");
        assertExceptionWhenHostNameIsInvalid("domain.com/");
        assertExceptionWhenHostNameIsInvalid("новости.ру:");
        assertExceptionWhenHostNameIsInvalid("новости.ру/");
        assertExceptionWhenHostNameIsInvalid("_http.sctp.www.example.com");
        assertExceptionWhenHostNameIsInvalid("http.sctp._www.example.com");
        // Checks a host that exceeds max allowed length of the host label (63 characters)
        assertExceptionWhenHostNameIsInvalid(label63 + "4.com");
        // Checks a host that exceeds max allowed length of hostname (255 characters)
        assertExceptionWhenHostNameIsInvalid(host255.substring(3) + ".com");
        assertExceptionWhenHostNameIsInvalid("FE80:0000:0000:0000:0202:B3FF:FE1E:8329");
        assertExceptionWhenHostNameIsInvalid("[2001:db8:0:1]:80");

        // Invalid host names for PKP that contain IPv4 addresses
        // or names with digits and dots only.
        assertExceptionWhenHostNameIsInvalid("127.0.0.1");
        assertExceptionWhenHostNameIsInvalid("68.44.222.12");
        assertExceptionWhenHostNameIsInvalid("256.0.0.1");
        assertExceptionWhenHostNameIsInvalid("127.0.0.1.1");
        assertExceptionWhenHostNameIsInvalid("127.0.0");
        assertExceptionWhenHostNameIsInvalid("127.0.0.");
        assertExceptionWhenHostNameIsInvalid("127.0.0.299");
    }

    /**
     * Tests that NullPointerException is thrown if the host name or the collection of pins or
     * the expiration date is null.
     *
     * @throws Exception
     */
    @Test
    @SmallTest
    public void testNullArguments() throws Exception {
        createCronetEngineBuilder(ENABLE_PINNING_BYPASS_FOR_LOCAL_ANCHORS, KNOWN_ROOT);
        verifyExceptionWhenAddPkpArgumentIsNull(true, false, false);
        verifyExceptionWhenAddPkpArgumentIsNull(false, true, false);
        verifyExceptionWhenAddPkpArgumentIsNull(false, false, true);
        verifyExceptionWhenAddPkpArgumentIsNull(false, false, false);
    }

    /**
     * Tests that IllegalArgumentException is thrown if SHA1 is passed as the value of a pin.
     *
     * @throws Exception
     */
    @Test
    @SmallTest
    public void testIllegalArgumentExceptionWhenPinValueIsSHA1() throws Exception {
        createCronetEngineBuilder(ENABLE_PINNING_BYPASS_FOR_LOCAL_ANCHORS, KNOWN_ROOT);
        byte[] sha1 = new byte[20];
        assertThrows("Pin value was: " + Arrays.toString(sha1), IllegalArgumentException.class,
                () -> addPkpSha256(mServerHost, sha1, EXCLUDE_SUBDOMAINS, DISTANT_FUTURE));
    }

    /**
     * Asserts that the response from the server contains an PKP error.
     */
    private void assertErrorResponse() {
        assertThat(mListener.mError).isNotNull();
        int errorCode = ((NetworkException) mListener.mError).getCronetInternalErrorCode();
        Set<Integer> expectedErrors = new HashSet<>();
        expectedErrors.add(NetError.ERR_CONNECTION_REFUSED);
        expectedErrors.add(NetError.ERR_SSL_PINNED_KEY_NOT_IN_CERT_CHAIN);
        assertWithMessage(String.format("Incorrect error code. Expected one of %s but received %s",
                                  expectedErrors, errorCode))
                .that(expectedErrors)
                .contains(errorCode);
    }

    /**
     * Asserts a successful response with response code 200.
     */
    private void assertSuccessfulResponse() {
        if (mListener.mError != null) {
            fail("Did not expect an error but got error code "
                    + ((NetworkException) mListener.mError).getCronetInternalErrorCode());
        }
        assertWithMessage("Expected non-null response from the server")
                .that(mListener.mResponseInfo)
                .isNotNull();
        assertThat(mListener.mResponseInfo.getHttpStatusCode()).isEqualTo(200);
    }

    private void createCronetEngineBuilder(boolean bypassPinningForLocalAnchors, boolean knownRoot)
            throws Exception {
        // Set common CronetEngine parameters
        mBuilder = mTestRule.getTestFramework().createNewSecondaryBuilder(
                mTestRule.getTestFramework().getContext());
        mBuilder.enablePublicKeyPinningBypassForLocalTrustAnchors(bypassPinningForLocalAnchors);
        JSONObject hostResolverParams = CronetTestUtil.generateHostResolverRules();
        JSONObject experimentalOptions = new JSONObject()
                                                 .put("HostResolverRules", hostResolverParams);
        mBuilder.setExperimentalOptions(experimentalOptions.toString());
        mBuilder.setStoragePath(getTestStorage(mTestRule.getTestFramework().getContext()));
        mBuilder.enableHttpCache(CronetEngine.Builder.HTTP_CACHE_DISK_NO_HTTP, 1000 * 1024);
        final String[] server_certs = {SERVER_CERT_PEM};
        CronetTestUtil.setMockCertVerifierForTesting(
                mBuilder, MockCertVerifier.createMockCertVerifier(server_certs, knownRoot));
    }

    private void startCronetEngine() {
        mCronetEngine = mBuilder.build();
    }

    private void shutdownCronetEngine() {
        if (mCronetEngine != null) {
            mCronetEngine.shutdown();
            mCronetEngine = null;
        }
    }

    private byte[] generateSomeSha256() {
        byte[] sha256 = new byte[32];
        Arrays.fill(sha256, (byte) 58);
        return sha256;
    }

    @SuppressWarnings("ArrayAsKeyOfSetOrMap")
    private void addPkpSha256(
            String host, byte[] pinHashValue, boolean includeSubdomain, int maxAgeInSec) {
        Set<byte[]> hashes = new HashSet<>();
        hashes.add(pinHashValue);
        mBuilder.addPublicKeyPins(host, hashes, includeSubdomain, dateInFuture(maxAgeInSec));
    }

    private void sendRequestAndWaitForResult() {
        mListener = new TestUrlRequestCallback();

        String httpURL = mServerUrl + "/simple.txt";
        UrlRequest.Builder requestBuilder =
                mCronetEngine.newUrlRequestBuilder(httpURL, mListener, mListener.getExecutor());
        requestBuilder.build().start();
        mListener.blockForDone();
    }

    private X509Certificate readCertFromFileInPemFormat(String certFileName) throws Exception {
        byte[] certDer = CertTestUtil.pemToDer(CertTestUtil.CERTS_DIRECTORY + certFileName);
        CertificateFactory certFactory = CertificateFactory.getInstance("X.509");
        return (X509Certificate) certFactory.generateCertificate(new ByteArrayInputStream(certDer));
    }

    private Date dateInFuture(int secondsIntoFuture) {
        Calendar cal = Calendar.getInstance();
        cal.add(Calendar.SECOND, secondsIntoFuture);
        return cal.getTime();
    }

    private void assertNoExceptionWhenHostNameIsValid(String hostName) {
        try {
            addPkpSha256(hostName, generateSomeSha256(), INCLUDE_SUBDOMAINS, DISTANT_FUTURE);
        } catch (IllegalArgumentException ex) {
            fail("Host name " + hostName + " should be valid but the exception was thrown: "
                    + ex.toString());
        }
    }

    private void assertExceptionWhenHostNameIsInvalid(String hostName) {
        assertThrows("Hostname was " + hostName, IllegalArgumentException.class,
                ()
                        -> addPkpSha256(hostName, generateSomeSha256(), INCLUDE_SUBDOMAINS,
                                DISTANT_FUTURE));
    }

    @SuppressWarnings("ArrayAsKeyOfSetOrMap")
    private void verifyExceptionWhenAddPkpArgumentIsNull(
            boolean hostNameIsNull, boolean pinsAreNull, boolean expirationDataIsNull) {
        String hostName = hostNameIsNull ? null : "some-host.com";
        Set<byte[]> pins = pinsAreNull ? null : new HashSet<byte[]>();
        Date expirationDate = expirationDataIsNull ? null : new Date();

        boolean shouldThrowNpe = hostNameIsNull || pinsAreNull || expirationDataIsNull;
        if (shouldThrowNpe) {
            assertThrows(NullPointerException.class,
                    ()
                            -> mBuilder.addPublicKeyPins(
                                    hostName, pins, INCLUDE_SUBDOMAINS, expirationDate));
        } else {
            mBuilder.addPublicKeyPins(hostName, pins, INCLUDE_SUBDOMAINS, expirationDate);
        }
    }
}
