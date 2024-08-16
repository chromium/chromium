// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.net;

import static com.google.common.truth.Truth.assertThat;

import static org.junit.Assert.assertThrows;

import static org.chromium.net.truth.UrlResponseInfoSubject.assertThat;

import android.os.Build;

import androidx.annotation.OptIn;
import androidx.test.ext.junit.runners.AndroidJUnit4;
import androidx.test.filters.SmallTest;

import org.junit.After;
import org.junit.Before;
import org.junit.Rule;
import org.junit.Test;
import org.junit.runner.RunWith;

import org.chromium.base.test.util.Batch;
import org.chromium.net.CronetTestRule.CronetImplementation;
import org.chromium.net.CronetTestRule.IgnoreFor;
import org.chromium.net.CronetTestRule.RequiresMinApi;

import java.nio.ByteBuffer;
import java.security.MessageDigest;
import java.util.Arrays;

/** Tests for external compression dictionaries support. */
@RunWith(AndroidJUnit4.class)
@RequiresMinApi(34) // External compression dictionaries support was added in crrev.com/c/5756722.
@Batch(Batch.UNIT_TESTS)
@IgnoreFor(
        implementations = {CronetImplementation.FALLBACK, CronetImplementation.AOSP_PLATFORM},
        reason =
                "Fallback and AOSP implementations do not support external compression"
                        + " dictionaries")
public class SharedDictionaryTest {
    @Rule public final CronetTestRule mTestRule = CronetTestRule.withManualEngineStartup();

    // Byte contents of a compression dictionary whose contents is the string "A dictionary".
    // See {@link org.chromium.net.Http2TestHandler.ServeSharedBrotliResponder#onHeadersRead}.
    private static final byte[] COMPRESSION_DICTIONARY = {
        (byte) 0x41,
        (byte) 0x20,
        (byte) 0x64,
        (byte) 0x69,
        (byte) 0x63,
        (byte) 0x74,
        (byte) 0x69,
        (byte) 0x6f,
        (byte) 0x6e,
        (byte) 0x61,
        (byte) 0x72,
        (byte) 0x79
    };

    private CronetEngine mCronetEngine;

    @Before
    public void setUp() throws Exception {
        // TODO(crbug.com/40284777): Fallback to MockCertVerifier when custom CAs are not supported.
        if (Build.VERSION.SDK_INT <= Build.VERSION_CODES.M) {
            mTestRule
                    .getTestFramework()
                    .applyEngineBuilderPatch(
                            (builder) -> {
                                CronetTestUtil.setMockCertVerifierForTesting(
                                        builder, QuicTestServer.createMockCertVerifier());
                            });
        }

        mTestRule
                .getTestFramework()
                .applyEngineBuilderPatch(
                        (builder) -> {
                            builder.enableBrotli(true);
                        });
        assertThat(Http2TestServer.startHttp2TestServer(mTestRule.getTestFramework().getContext()))
                .isTrue();
    }

    @After
    public void tearDown() throws Exception {
        assertThat(Http2TestServer.shutdownHttp2TestServer()).isTrue();
    }

    @Test
    @SmallTest
    @OptIn(markerClass = org.chromium.net.UrlRequest.Experimental.class)
    public void testNullHashThrows() throws Exception {
        mCronetEngine = mTestRule.getTestFramework().startEngine();
        String url = Http2TestServer.getEchoAllHeadersUrl();
        TestUrlRequestCallback callback = new TestUrlRequestCallback();
        UrlRequest.Builder builder =
                mCronetEngine.newUrlRequestBuilder(url, callback, callback.getExecutor());
        ByteBuffer dictionary = ByteBuffer.allocateDirect(COMPRESSION_DICTIONARY.length);
        dictionary.put(COMPRESSION_DICTIONARY);
        dictionary.flip();

        Exception e =
                assertThrows(
                        NullPointerException.class,
                        () ->
                                builder.setRawCompressionDictionary(
                                        null, dictionary, /* dictionaryId= */ ""));
        assertThat(e).hasMessageThat().isEqualTo("Hash is required");
    }

    @Test
    @SmallTest
    @OptIn(markerClass = org.chromium.net.UrlRequest.Experimental.class)
    public void testNonConformantHashSizeThrows() throws Exception {
        mCronetEngine = mTestRule.getTestFramework().startEngine();
        String url = Http2TestServer.getEchoAllHeadersUrl();
        TestUrlRequestCallback callback = new TestUrlRequestCallback();
        UrlRequest.Builder builder =
                mCronetEngine.newUrlRequestBuilder(url, callback, callback.getExecutor());
        ByteBuffer dictionary = ByteBuffer.allocateDirect(COMPRESSION_DICTIONARY.length);
        dictionary.put(COMPRESSION_DICTIONARY);
        dictionary.flip();

        Exception e =
                assertThrows(
                        IllegalArgumentException.class,
                        () ->
                                builder.setRawCompressionDictionary(
                                        new byte[] {1, 2, 3}, dictionary, /* dictionaryId= */ ""));
        assertThat(e).hasMessageThat().isEqualTo("SHA-256 hashes are supposed to be 32 bytes");
    }

    @Test
    @SmallTest
    @OptIn(markerClass = org.chromium.net.UrlRequest.Experimental.class)
    public void testNullDictionaryThrows() throws Exception {
        mCronetEngine = mTestRule.getTestFramework().startEngine();
        String url = Http2TestServer.getEchoAllHeadersUrl();
        TestUrlRequestCallback callback = new TestUrlRequestCallback();
        UrlRequest.Builder builder =
                mCronetEngine.newUrlRequestBuilder(url, callback, callback.getExecutor());
        ByteBuffer dictionary = ByteBuffer.allocateDirect(COMPRESSION_DICTIONARY.length);
        dictionary.put(COMPRESSION_DICTIONARY);
        dictionary.flip();
        MessageDigest digest = MessageDigest.getInstance("SHA-256");
        digest.update(dictionary.duplicate());
        byte[] hash = digest.digest();

        Exception e =
                assertThrows(
                        NullPointerException.class,
                        () ->
                                builder.setRawCompressionDictionary(
                                        hash, null, /* dictionaryId= */ ""));
        assertThat(e).hasMessageThat().isEqualTo("Dictionary is required");
    }

    @Test
    @SmallTest
    @OptIn(markerClass = org.chromium.net.UrlRequest.Experimental.class)
    public void testNullDictionaryIdThrows() throws Exception {
        mCronetEngine = mTestRule.getTestFramework().startEngine();
        String url = Http2TestServer.getEchoAllHeadersUrl();
        TestUrlRequestCallback callback = new TestUrlRequestCallback();
        UrlRequest.Builder builder =
                mCronetEngine.newUrlRequestBuilder(url, callback, callback.getExecutor());
        ByteBuffer dictionary = ByteBuffer.allocateDirect(COMPRESSION_DICTIONARY.length);
        dictionary.put(COMPRESSION_DICTIONARY);
        dictionary.flip();
        MessageDigest digest = MessageDigest.getInstance("SHA-256");
        digest.update(dictionary.duplicate());
        byte[] hash = digest.digest();

        Exception e =
                assertThrows(
                        NullPointerException.class,
                        () ->
                                builder.setRawCompressionDictionary(
                                        hash, dictionary, /* dictionaryId= */ null));
        assertThat(e)
                .hasMessageThat()
                .isEqualTo("Dictionary ID cannot be null. If missing, pass an empty string");
    }

    @Test
    @SmallTest
    @OptIn(markerClass = org.chromium.net.UrlRequest.Experimental.class)
    public void testNonDirectByteBufferAsDictionaryThrows() throws Exception {
        mCronetEngine = mTestRule.getTestFramework().startEngine();
        String url = Http2TestServer.getEchoAllHeadersUrl();
        TestUrlRequestCallback callback = new TestUrlRequestCallback();
        UrlRequest.Builder builder =
                mCronetEngine.newUrlRequestBuilder(url, callback, callback.getExecutor());
        ByteBuffer dictionary = ByteBuffer.allocate(COMPRESSION_DICTIONARY.length);
        dictionary.put(COMPRESSION_DICTIONARY);
        dictionary.flip();
        MessageDigest digest = MessageDigest.getInstance("SHA-256");
        digest.update(dictionary.duplicate());
        byte[] hash = digest.digest();

        Exception e =
                assertThrows(
                        IllegalArgumentException.class,
                        () ->
                                builder.setRawCompressionDictionary(
                                        hash, dictionary, /* dictionaryId= */ ""));
        assertThat(e).hasMessageThat().isEqualTo("byteBuffer must be a direct ByteBuffer.");
    }

    @Test
    @SmallTest
    @OptIn(markerClass = org.chromium.net.UrlRequest.Experimental.class)
    public void testDictionaryIsAdvertised() throws Exception {
        mCronetEngine = mTestRule.getTestFramework().startEngine();
        String url = Http2TestServer.getEchoAllHeadersUrl();
        TestUrlRequestCallback callback = new TestUrlRequestCallback();
        UrlRequest.Builder builder =
                mCronetEngine.newUrlRequestBuilder(url, callback, callback.getExecutor());

        ByteBuffer dictionary = ByteBuffer.allocateDirect(COMPRESSION_DICTIONARY.length);
        dictionary.put(COMPRESSION_DICTIONARY);
        dictionary.flip();
        MessageDigest digest = MessageDigest.getInstance("SHA-256");
        digest.update(dictionary.duplicate());
        byte[] hash = digest.digest();

        builder.setRawCompressionDictionary(hash, dictionary, /* dictionaryId= */ "");
        UrlRequest request = builder.build();
        request.start();
        callback.blockForDone();
        assertThat(callback.getResponseInfoWithChecks()).hasHttpStatusCodeThat().isEqualTo(200);
        assertThat(callback.mResponseAsString)
                .contains("\r\naccept-encoding: gzip, deflate, br, dcb\r\n");
        assertThat(callback.mResponseAsString).doesNotContain("dictionary-id");
        // Base64 encoding (delimited by colons, as required by ietf-httpbis-sfbis-06) of the hash
        // of the dictionary.
        assertThat(callback.mResponseAsString)
                .contains(
                        "\r\n"
                                + "available-dictionary:"
                                + " :CqNpAU9/qzcL6UB0aYVFx7uTLsRhJSePN780qwKjWuw=:\r\n");
    }

    @Test
    @SmallTest
    @OptIn(markerClass = org.chromium.net.UrlRequest.Experimental.class)
    public void testDictionaryIDIsAdvertised() throws Exception {
        mCronetEngine = mTestRule.getTestFramework().startEngine();
        String url = Http2TestServer.getEchoAllHeadersUrl();
        TestUrlRequestCallback callback = new TestUrlRequestCallback();
        UrlRequest.Builder builder =
                mCronetEngine.newUrlRequestBuilder(url, callback, callback.getExecutor());

        ByteBuffer dictionary = ByteBuffer.allocateDirect(COMPRESSION_DICTIONARY.length);
        dictionary.put(COMPRESSION_DICTIONARY);
        dictionary.flip();
        MessageDigest digest = MessageDigest.getInstance("SHA-256");
        digest.update(dictionary.duplicate());
        byte[] hash = digest.digest();

        builder.setRawCompressionDictionary(hash, dictionary, /* dictionaryId= */ "MyID");
        UrlRequest request = builder.build();
        request.start();
        callback.blockForDone();
        assertThat(callback.getResponseInfoWithChecks()).hasHttpStatusCodeThat().isEqualTo(200);
        assertThat(callback.mResponseAsString).contains("\r\ndictionary-id: \"MyID\"\r\n");
        assertThat(callback.mResponseAsString)
                .contains("\r\naccept-encoding: gzip, deflate, br, dcb\r\n");
        // Base64 encoding (delimited by colons, as required by ietf-httpbis-sfbis-06) of the hash
        // of the dictionary.
        assertThat(callback.mResponseAsString)
                .contains(
                        "\r\n"
                                + "available-dictionary:"
                                + " :CqNpAU9/qzcL6UB0aYVFx7uTLsRhJSePN780qwKjWuw=:\r\n");
    }

    @Test
    @SmallTest
    public void testDefaultNoDictionaryIsAdvertised() throws Exception {
        mCronetEngine = mTestRule.getTestFramework().startEngine();
        String url = Http2TestServer.getEchoAllHeadersUrl();
        TestUrlRequestCallback callback = new TestUrlRequestCallback();
        UrlRequest.Builder builder =
                mCronetEngine.newUrlRequestBuilder(url, callback, callback.getExecutor());
        UrlRequest request = builder.build();
        request.start();
        callback.blockForDone();
        assertThat(callback.getResponseInfoWithChecks()).hasHttpStatusCodeThat().isEqualTo(200);
        assertThat(callback.mResponseAsString).doesNotContain("dcb");
        assertThat(callback.mResponseAsString).doesNotContain("dictionary-id");
    }

    @Test
    @SmallTest
    @OptIn(markerClass = org.chromium.net.UrlRequest.Experimental.class)
    public void testsetRawCompressionDictionaryWithDisabledBrotliSilentlyFails() throws Exception {
        mTestRule
                .getTestFramework()
                .applyEngineBuilderPatch(
                        (builder) -> {
                            builder.enableBrotli(false);
                        });
        mCronetEngine = mTestRule.getTestFramework().startEngine();
        String url = Http2TestServer.getEchoAllHeadersUrl();
        TestUrlRequestCallback callback = new TestUrlRequestCallback();
        UrlRequest.Builder builder =
                mCronetEngine.newUrlRequestBuilder(url, callback, callback.getExecutor());

        ByteBuffer dictionary = ByteBuffer.allocateDirect(COMPRESSION_DICTIONARY.length);
        dictionary.put(COMPRESSION_DICTIONARY);
        dictionary.flip();
        MessageDigest digest = MessageDigest.getInstance("SHA-256");
        digest.update(dictionary.duplicate());
        byte[] hash = digest.digest();

        builder.setRawCompressionDictionary(hash, dictionary, /* dictionaryId= */ "");
        UrlRequest request = builder.build();
        request.start();
        callback.blockForDone();
        assertThat(callback.getResponseInfoWithChecks()).hasHttpStatusCodeThat().isEqualTo(200);
        assertThat(callback.mResponseAsString).doesNotContain("dcb");
        assertThat(callback.mResponseAsString).doesNotContain("dictionary-id");
    }

    @Test
    @SmallTest
    @OptIn(markerClass = org.chromium.net.UrlRequest.Experimental.class)
    public void testSharedDictionaryDecoded() throws Exception {
        mCronetEngine = mTestRule.getTestFramework().startEngine();
        String url = Http2TestServer.getServeSharedBrotliResponse();
        TestUrlRequestCallback callback = new TestUrlRequestCallback();
        UrlRequest.Builder builder =
                mCronetEngine.newUrlRequestBuilder(url, callback, callback.getExecutor());

        ByteBuffer dictionary = ByteBuffer.allocateDirect(COMPRESSION_DICTIONARY.length);
        dictionary.put(COMPRESSION_DICTIONARY);
        dictionary.flip();
        MessageDigest digest = MessageDigest.getInstance("SHA-256");
        digest.update(dictionary.duplicate());
        byte[] hash = digest.digest();

        builder.setRawCompressionDictionary(hash, dictionary, /* dictionaryId= */ "");
        UrlRequest request = builder.build();
        request.start();
        callback.blockForDone();
        assertThat(callback.getResponseInfoWithChecks()).hasHttpStatusCodeThat().isEqualTo(200);
        String expectedResponse = "This is compressed test data using a test dictionary";
        assertThat(callback.mResponseAsString).isEqualTo(expectedResponse);
        assertThat(callback.getResponseInfoWithChecks())
                .hasHeadersThat()
                .containsEntry("content-encoding", Arrays.asList("dcb"));
    }
}
