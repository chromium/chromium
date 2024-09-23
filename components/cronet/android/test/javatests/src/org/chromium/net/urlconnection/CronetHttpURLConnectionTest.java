// Copyright 2014 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.net.urlconnection;

import static com.google.common.truth.Truth.assertThat;

import static org.junit.Assert.assertThrows;

import android.net.TrafficStats;
import android.os.Build;
import android.os.Process;

import androidx.test.ext.junit.runners.AndroidJUnit4;
import androidx.test.filters.SmallTest;

import org.junit.After;
import org.junit.Before;
import org.junit.Rule;
import org.junit.Test;
import org.junit.runner.RunWith;

import org.chromium.base.Log;
import org.chromium.base.test.util.DoNotBatch;
import org.chromium.net.CronetEngine;
import org.chromium.net.CronetException;
import org.chromium.net.CronetTestRule;
import org.chromium.net.CronetTestRule.CronetImplementation;
import org.chromium.net.CronetTestRule.IgnoreFor;
import org.chromium.net.CronetTestRule.RequiresMinAndroidApi;
import org.chromium.net.CronetTestRule.RequiresMinApi;
import org.chromium.net.CronetTestUtil;
import org.chromium.net.MockUrlRequestJobFactory;
import org.chromium.net.NativeTestServer;

import java.io.ByteArrayOutputStream;
import java.io.FileNotFoundException;
import java.io.IOException;
import java.io.InputStream;
import java.io.InterruptedIOException;
import java.io.OutputStream;
import java.net.HttpURLConnection;
import java.net.ServerSocket;
import java.net.Socket;
import java.net.SocketTimeoutException;
import java.net.URL;
import java.util.ArrayList;
import java.util.Arrays;
import java.util.List;
import java.util.Map;
import java.util.concurrent.Callable;
import java.util.concurrent.FutureTask;
import java.util.concurrent.atomic.AtomicBoolean;
import java.util.regex.Matcher;
import java.util.regex.Pattern;

/** Basic tests of Cronet's HttpURLConnection implementation. */
@DoNotBatch(
        reason =
                "crbug/1453571 testReadTimeout crashes because of MockUrlrequestJobFactory's"
                        + "interaction with a singleton")
@IgnoreFor(
        implementations = {CronetImplementation.FALLBACK},
        reason = "See crrev.com/c/4590329")
@RunWith(AndroidJUnit4.class)
public class CronetHttpURLConnectionTest {
    private static final String TAG = CronetHttpURLConnectionTest.class.getSimpleName();

    @Rule public final CronetTestRule mTestRule = CronetTestRule.withManualEngineStartup();

    private HttpURLConnection mUrlConnection;

    private CronetEngine mCronetEngine;

    @Before
    public void setUp() throws Exception {
        mTestRule
                .getTestFramework()
                .applyEngineBuilderPatch(
                        (builder) -> {
                            mTestRule.getTestFramework().enableDiskCache(builder);
                        });

        mCronetEngine = mTestRule.getTestFramework().startEngine();
        assertThat(
                        NativeTestServer.startNativeTestServer(
                                mTestRule.getTestFramework().getContext()))
                .isTrue();
    }

    @After
    public void tearDown() throws Exception {
        if (mUrlConnection != null) {
            mUrlConnection.disconnect();
        }
        NativeTestServer.shutdownNativeTestServer();
    }

    @Test
    @SmallTest
    public void testBasicGet() throws Exception {
        URL url = new URL(NativeTestServer.getEchoMethodURL());
        mUrlConnection = (HttpURLConnection) mCronetEngine.openConnection(url);
        assertThat(mUrlConnection.getResponseCode()).isEqualTo(200);
        assertThat(mUrlConnection.getResponseMessage()).isEqualTo("OK");
        assertThat(TestUtil.getResponseAsString(mUrlConnection)).isEqualTo("GET");
    }

    @Test
    @SmallTest
    // Regression test for crbug.com/561678.
    public void testSetRequestMethod() throws Exception {
        URL url = new URL(NativeTestServer.getEchoMethodURL());
        mUrlConnection = (HttpURLConnection) mCronetEngine.openConnection(url);
        mUrlConnection.setDoOutput(true);
        mUrlConnection.setRequestMethod("PUT");
        OutputStream out = mUrlConnection.getOutputStream();
        out.write("sample data".getBytes());
        assertThat(mUrlConnection.getResponseCode()).isEqualTo(200);
        assertThat(mUrlConnection.getResponseMessage()).isEqualTo("OK");
        assertThat(TestUtil.getResponseAsString(mUrlConnection)).isEqualTo("PUT");
    }

    @Test
    @SmallTest
    public void testConnectTimeout() throws Exception {
        URL url = new URL(NativeTestServer.getEchoMethodURL());
        mUrlConnection = (HttpURLConnection) mCronetEngine.openConnection(url);
        // This should not throw an exception.
        mUrlConnection.setConnectTimeout(1000);
        assertThat(mUrlConnection.getResponseCode()).isEqualTo(200);
        assertThat(mUrlConnection.getResponseMessage()).isEqualTo("OK");
        assertThat(TestUtil.getResponseAsString(mUrlConnection)).isEqualTo("GET");
    }

    @Test
    @SmallTest
    @IgnoreFor(
            implementations = {CronetImplementation.AOSP_PLATFORM},
            reason = "crbug.com/1495309: Enable once we drop MockUrlRequestJobFactory")
    public void testReadTimeout() throws Exception {
        // Add url interceptors.
        MockUrlRequestJobFactory mockUrlRequestJobFactory =
                new MockUrlRequestJobFactory(mTestRule.getTestFramework().getEngine());
        URL url = new URL(MockUrlRequestJobFactory.getMockUrlForHangingRead());
        mUrlConnection = (HttpURLConnection) mCronetEngine.openConnection(url);
        mUrlConnection.setReadTimeout(1000);
        assertThat(mUrlConnection.getResponseCode()).isEqualTo(200);
        InputStream in = mUrlConnection.getInputStream();
        assertThrows(SocketTimeoutException.class, in::read);

        mUrlConnection.disconnect();
        mockUrlRequestJobFactory.shutdown();
    }

    @Test
    @SmallTest
    // Regression test for crbug.com/571436.
    public void testDefaultToPostWhenDoOutput() throws Exception {
        URL url = new URL(NativeTestServer.getEchoMethodURL());
        mUrlConnection = (HttpURLConnection) mCronetEngine.openConnection(url);
        mUrlConnection.setDoOutput(true);
        OutputStream out = mUrlConnection.getOutputStream();
        out.write("sample data".getBytes());
        assertThat(mUrlConnection.getResponseCode()).isEqualTo(200);
        assertThat(mUrlConnection.getResponseMessage()).isEqualTo("OK");
        assertThat(TestUtil.getResponseAsString(mUrlConnection)).isEqualTo("POST");
    }

    /**
     * Tests that calling {@link HttpURLConnection#connect} will also initialize {@code
     * OutputStream} if necessary in the case where {@code setFixedLengthStreamingMode} is called.
     * Regression test for crbug.com/582975.
     */
    @Test
    @SmallTest
    public void testInitOutputStreamInConnect() throws Exception {
        URL url = new URL(NativeTestServer.getEchoBodyURL());
        mUrlConnection = (HttpURLConnection) mCronetEngine.openConnection(url);
        mUrlConnection.setDoOutput(true);
        String dataString = "some very important data";
        byte[] data = dataString.getBytes();
        mUrlConnection.setFixedLengthStreamingMode(data.length);
        mUrlConnection.connect();
        OutputStream out = mUrlConnection.getOutputStream();
        out.write(data);
        assertThat(mUrlConnection.getResponseCode()).isEqualTo(200);
        assertThat(mUrlConnection.getResponseMessage()).isEqualTo("OK");
        assertThat(TestUtil.getResponseAsString(mUrlConnection)).isEqualTo(dataString);
    }

    /**
     * Tests that calling {@link HttpURLConnection#connect} will also initialize {@code
     * OutputStream} if necessary in the case where {@code setChunkedStreamingMode} is called.
     * Regression test for crbug.com/582975.
     */
    @Test
    @SmallTest
    public void testInitChunkedOutputStreamInConnect() throws Exception {
        URL url = new URL(NativeTestServer.getEchoBodyURL());
        mUrlConnection = (HttpURLConnection) mCronetEngine.openConnection(url);
        mUrlConnection.setDoOutput(true);
        String dataString = "some very important chunked data";
        byte[] data = dataString.getBytes();
        mUrlConnection.setChunkedStreamingMode(0);
        mUrlConnection.connect();
        OutputStream out = mUrlConnection.getOutputStream();
        out.write(data);
        assertThat(mUrlConnection.getResponseCode()).isEqualTo(200);
        assertThat(mUrlConnection.getResponseMessage()).isEqualTo("OK");
        assertThat(TestUtil.getResponseAsString(mUrlConnection)).isEqualTo(dataString);
    }

    @Test
    @SmallTest
    public void testSetFixedLengthStreamingModeLong() throws Exception {
        URL url = new URL(NativeTestServer.getEchoBodyURL());
        mUrlConnection = (HttpURLConnection) mCronetEngine.openConnection(url);
        mUrlConnection.setDoOutput(true);
        mUrlConnection.setRequestMethod("POST");
        String dataString = "some very important data";
        byte[] data = dataString.getBytes();
        mUrlConnection.setFixedLengthStreamingMode((long) data.length);
        OutputStream out = mUrlConnection.getOutputStream();
        out.write(data);
        assertThat(mUrlConnection.getResponseCode()).isEqualTo(200);
        assertThat(mUrlConnection.getResponseMessage()).isEqualTo("OK");
        assertThat(TestUtil.getResponseAsString(mUrlConnection)).isEqualTo(dataString);
    }

    @Test
    @SmallTest
    public void testSetFixedLengthStreamingModeInt() throws Exception {
        URL url = new URL(NativeTestServer.getEchoBodyURL());
        mUrlConnection = (HttpURLConnection) mCronetEngine.openConnection(url);
        mUrlConnection.setDoOutput(true);
        mUrlConnection.setRequestMethod("POST");
        String dataString = "some very important data";
        byte[] data = dataString.getBytes();
        mUrlConnection.setFixedLengthStreamingMode((int) data.length);
        OutputStream out = mUrlConnection.getOutputStream();
        out.write(data);
        assertThat(mUrlConnection.getResponseCode()).isEqualTo(200);
        assertThat(mUrlConnection.getResponseMessage()).isEqualTo("OK");
        assertThat(TestUtil.getResponseAsString(mUrlConnection)).isEqualTo(dataString);
    }

    @Test
    @SmallTest
    public void testNotFoundURLRequest() throws Exception {
        URL url = new URL(NativeTestServer.getFileURL("/notfound.html"));
        mUrlConnection = (HttpURLConnection) mCronetEngine.openConnection(url);
        assertThat(mUrlConnection.getResponseCode()).isEqualTo(404);
        assertThat(mUrlConnection.getResponseMessage()).isEqualTo("Not Found");
        assertThrows(FileNotFoundException.class, mUrlConnection::getInputStream);

        InputStream errorStream = mUrlConnection.getErrorStream();
        ByteArrayOutputStream out = new ByteArrayOutputStream();
        int byteRead;
        while ((byteRead = errorStream.read()) != -1) {
            out.write(byteRead);
        }
        assertThat(out.toString())
                .isEqualTo(
                        "<!DOCTYPE html>\n<html>\n<head>\n"
                                + "<title>Not found</title>\n<p>Test page loaded.</p>\n"
                                + "</head>\n</html>\n");
    }

    @Test
    @SmallTest
    public void testServerNotAvailable() throws Exception {
        URL url = new URL(NativeTestServer.getFileURL("/success.txt"));
        mUrlConnection = (HttpURLConnection) mCronetEngine.openConnection(url);
        assertThat(TestUtil.getResponseAsString(mUrlConnection)).isEqualTo("this is a text file\n");
        // After shutting down the server, the server should not be handling
        // new requests.
        NativeTestServer.shutdownNativeTestServer();
        HttpURLConnection secondConnection = (HttpURLConnection) mCronetEngine.openConnection(url);
        // Cronet's wrapper only receives the error in its listener
        // callback when message loop is running, thus only knows
        // about the error when it starts to read response.
        IOException e = assertThrows(IOException.class, secondConnection::getResponseCode);
        // TODO(crbug.com/40286644): Consider whether we should be checking this in the first place.
        if (mTestRule.implementationUnderTest().equals(CronetImplementation.STATICALLY_LINKED)) {
            assertThat(e).isInstanceOf(CronetException.class);
        }
        assertThat(e)
                .hasMessageThat()
                .containsMatch(
                        Pattern.compile(
                                "ECONNREFUSED|Connection refused|net::ERR_CONNECTION_REFUSED|Failed"
                                        + " to connect"));
        checkExceptionsAreThrown(secondConnection);
        // Starts the server to avoid crashing on shutdown in tearDown().
        assertThat(
                        NativeTestServer.startNativeTestServer(
                                mTestRule.getTestFramework().getContext()))
                .isTrue();
    }

    @Test
    @SmallTest
    public void testBadIP() throws Exception {
        URL url = new URL("http://0.0.0.0/");
        mUrlConnection = (HttpURLConnection) mCronetEngine.openConnection(url);
        // Cronet's wrapper only receives the error in its listener
        // callback when message loop is running, thus only knows
        // about the error when it starts to read response.
        IOException e = assertThrows(IOException.class, mUrlConnection::getResponseCode);
        // TODO(crbug.com/40286644): Consider whether we should be checking this in the first place.
        if (mTestRule.implementationUnderTest().equals(CronetImplementation.STATICALLY_LINKED)) {
            assertThat(e).isInstanceOf(CronetException.class);
        }
        assertThat(e)
                .hasMessageThat()
                .containsMatch(
                        Pattern.compile(
                                "ECONNREFUSED|Connection refused|net::ERR_CONNECTION_REFUSED|Failed"
                                        + " to connect"));
        checkExceptionsAreThrown(mUrlConnection);
    }

    @Test
    @SmallTest
    public void testBadHostname() throws Exception {
        URL url = new URL("http://this-weird-host-name-does-not-exist/");
        mUrlConnection = (HttpURLConnection) mCronetEngine.openConnection(url);
        // Cronet's wrapper only receives the error in its listener
        // callback when message loop is running, thus only knows
        // about the error when it starts to read response.
        IOException e = assertThrows(IOException.class, mUrlConnection::getResponseCode);
        // TODO(crbug.com/40286644): Consider whether we should be checking this in the first place.
        if (mTestRule.implementationUnderTest().equals(CronetImplementation.STATICALLY_LINKED)) {
            assertThat(e).isInstanceOf(CronetException.class);
        }
        checkExceptionsAreThrown(mUrlConnection);
    }

    @Test
    @SmallTest
    public void testDisconnectBeforeConnectionIsMade() throws Exception {
        URL url = new URL(NativeTestServer.getEchoMethodURL());
        mUrlConnection = (HttpURLConnection) mCronetEngine.openConnection(url);
        // Closing connection before connection is made has no effect.
        mUrlConnection.disconnect();
        assertThat(mUrlConnection.getResponseCode()).isEqualTo(200);
        assertThat(mUrlConnection.getResponseMessage()).isEqualTo("OK");
        assertThat(TestUtil.getResponseAsString(mUrlConnection)).isEqualTo("GET");
    }

    @Test
    @SmallTest
    // TODO(xunjieli): Currently the wrapper does not throw an exception.
    // Need to change the behavior.
    public void testDisconnectAfterConnectionIsMade() throws Exception {
        URL url = new URL(NativeTestServer.getEchoMethodURL());
        mUrlConnection = (HttpURLConnection) mCronetEngine.openConnection(url);
        // Close mUrlConnection before mUrlConnection is made has no effect.
        mUrlConnection.connect();
        mUrlConnection.disconnect();
        assertThrows(IOException.class, mUrlConnection::getResponseCode);
        assertThrows(IOException.class, mUrlConnection::getInputStream);
    }

    @Test
    @SmallTest
    public void testMultipleDisconnect() throws Exception {
        URL url = new URL(NativeTestServer.getEchoMethodURL());
        mUrlConnection = (HttpURLConnection) mCronetEngine.openConnection(url);
        assertThat(mUrlConnection.getResponseCode()).isEqualTo(200);
        assertThat(mUrlConnection.getResponseMessage()).isEqualTo("OK");
        assertThat(TestUtil.getResponseAsString(mUrlConnection)).isEqualTo("GET");
        // Disconnect multiple times should be fine.
        for (int i = 0; i < 10; i++) {
            mUrlConnection.disconnect();
        }
    }

    @Test
    @SmallTest
    public void testAddRequestProperty() throws Exception {
        URL url = new URL(NativeTestServer.getEchoAllHeadersURL());
        mUrlConnection = (HttpURLConnection) mCronetEngine.openConnection(url);
        mUrlConnection.addRequestProperty("foo-header", "foo");
        mUrlConnection.addRequestProperty("bar-header", "bar");

        // Before connection is made, check request headers are set.
        Map<String, List<String>> requestHeadersMap = mUrlConnection.getRequestProperties();
        List<String> fooValues = requestHeadersMap.get("foo-header");
        assertThat(fooValues).containsExactly("foo");
        assertThat(mUrlConnection.getRequestProperty("foo-header")).isEqualTo("foo");
        List<String> barValues = requestHeadersMap.get("bar-header");
        assertThat(barValues).containsExactly("bar");
        assertThat(mUrlConnection.getRequestProperty("bar-header")).isEqualTo("bar");

        // Check the request headers echoed back by the server.
        assertThat(mUrlConnection.getResponseCode()).isEqualTo(200);
        assertThat(mUrlConnection.getResponseMessage()).isEqualTo("OK");
        String headers = TestUtil.getResponseAsString(mUrlConnection);
        List<String> fooHeaderValues = getRequestHeaderValues(headers, "foo-header");
        List<String> barHeaderValues = getRequestHeaderValues(headers, "bar-header");
        assertThat(fooHeaderValues).containsExactly("foo");
        assertThat(barHeaderValues).containsExactly("bar");
    }

    @Test
    @SmallTest
    public void testAddRequestPropertyWithSameKey() throws Exception {
        URL url = new URL(NativeTestServer.getEchoAllHeadersURL());
        mUrlConnection = (HttpURLConnection) mCronetEngine.openConnection(url);
        mUrlConnection.addRequestProperty("header-name", "value1");
        UnsupportedOperationException e =
                assertThrows(
                        UnsupportedOperationException.class,
                        () -> mUrlConnection.addRequestProperty("header-Name", "value2"));
        assertThat(e)
                .hasMessageThat()
                .isEqualTo(
                        "Cannot add multiple headers of the same key, header-Name. "
                                + "crbug.com/432719.");
    }

    @Test
    @SmallTest
    public void testSetRequestPropertyWithSameKey() throws Exception {
        URL url = new URL(NativeTestServer.getEchoAllHeadersURL());
        HttpURLConnection conn = (HttpURLConnection) mCronetEngine.openConnection(url);
        // The test always sets and retrieves one header with the same
        // capitalization, and the other header with slightly different
        // capitalization.
        conn.setRequestProperty("same-capitalization", "yo");
        conn.setRequestProperty("diFFerent-cApitalization", "foo");
        Map<String, List<String>> headersMap = conn.getRequestProperties();
        List<String> values1 = headersMap.get("same-capitalization");
        assertThat(values1).containsExactly("yo");
        assertThat(conn.getRequestProperty("same-capitalization")).isEqualTo("yo");

        List<String> values2 = headersMap.get("different-capitalization");
        assertThat(values2).containsExactly("foo");
        assertThat(conn.getRequestProperty("Different-capitalization")).isEqualTo("foo");

        // Check request header is updated.
        conn.setRequestProperty("same-capitalization", "hi");
        conn.setRequestProperty("different-Capitalization", "bar");
        Map<String, List<String>> newHeadersMap = conn.getRequestProperties();
        List<String> newValues1 = newHeadersMap.get("same-capitalization");
        assertThat(newValues1).containsExactly("hi");
        assertThat(conn.getRequestProperty("same-capitalization")).isEqualTo("hi");

        List<String> newValues2 = newHeadersMap.get("differENT-capitalization");
        assertThat(newValues2).containsExactly("bar");
        assertThat(conn.getRequestProperty("different-capitalization")).isEqualTo("bar");

        // Check the request headers echoed back by the server.
        assertThat(conn.getResponseCode()).isEqualTo(200);
        assertThat(conn.getResponseMessage()).isEqualTo("OK");
        String headers = TestUtil.getResponseAsString(conn);

        List<String> actualValues1 = getRequestHeaderValues(headers, "same-capitalization");
        assertThat(actualValues1).containsExactly("hi");
        List<String> actualValues2 = getRequestHeaderValues(headers, "different-Capitalization");
        assertThat(actualValues2).containsExactly("bar");
        conn.disconnect();
    }

    @Test
    @SmallTest
    public void testAddAndSetRequestPropertyWithSameKey() throws Exception {
        URL url = new URL(NativeTestServer.getEchoAllHeadersURL());
        mUrlConnection = (HttpURLConnection) mCronetEngine.openConnection(url);
        mUrlConnection.addRequestProperty("header-name", "value1");
        mUrlConnection.setRequestProperty("Header-nAme", "value2");

        // Before connection is made, check request headers are set.
        assertThat(mUrlConnection.getRequestProperty("header-namE")).isEqualTo("value2");
        Map<String, List<String>> requestHeadersMap = mUrlConnection.getRequestProperties();
        assertThat(requestHeadersMap).containsEntry("HeAder-name", Arrays.asList("value2"));

        // Check the request headers echoed back by the server.
        assertThat(mUrlConnection.getResponseCode()).isEqualTo(200);
        assertThat(mUrlConnection.getResponseMessage()).isEqualTo("OK");
        String headers = TestUtil.getResponseAsString(mUrlConnection);
        List<String> actualValues = getRequestHeaderValues(headers, "Header-nAme");
        assertThat(actualValues).containsExactly("value2");
    }

    @Test
    @SmallTest
    public void testAddSetRequestPropertyAfterConnected() throws Exception {
        URL url = new URL(NativeTestServer.getEchoAllHeadersURL());
        mUrlConnection = (HttpURLConnection) mCronetEngine.openConnection(url);
        mUrlConnection.addRequestProperty("header-name", "value");
        assertThat(mUrlConnection.getResponseCode()).isEqualTo(200);
        assertThat(mUrlConnection.getResponseMessage()).isEqualTo("OK");
        assertThrows(
                IllegalStateException.class, () -> mUrlConnection.setRequestProperty("foo", "bar"));
        assertThrows(
                IllegalStateException.class, () -> mUrlConnection.addRequestProperty("foo", "bar"));
    }

    @Test
    @SmallTest
    public void testGetRequestPropertyAfterConnected() throws Exception {
        URL url = new URL(NativeTestServer.getEchoAllHeadersURL());
        mUrlConnection = (HttpURLConnection) mCronetEngine.openConnection(url);
        mUrlConnection.addRequestProperty("header-name", "value");

        assertThat(mUrlConnection.getResponseCode()).isEqualTo(200);
        assertThat(mUrlConnection.getResponseMessage()).isEqualTo("OK");

        assertThrows(IllegalStateException.class, mUrlConnection::getRequestProperties);
        assertThat(mUrlConnection.getRequestProperty("header-name")).isEqualTo("value");
    }

    @Test
    @SmallTest
    public void testGetRequestPropertiesUnmodifiable() throws Exception {
        URL url = new URL(NativeTestServer.getEchoAllHeadersURL());
        mUrlConnection = (HttpURLConnection) mCronetEngine.openConnection(url);
        mUrlConnection.addRequestProperty("header-name", "value");
        Map<String, List<String>> headers = mUrlConnection.getRequestProperties();
        assertThrows(
                UnsupportedOperationException.class,
                () -> headers.put("foo", Arrays.asList("v1", "v2")));

        List<String> values = headers.get("header-name");
        assertThrows(UnsupportedOperationException.class, () -> values.add("v3"));
    }

    @Test
    @SmallTest
    public void testInputStreamBatchReadBoundaryConditions() throws Exception {
        String testInputString = "this is a very important header";
        URL url = new URL(NativeTestServer.getEchoHeaderURL("foo"));
        mUrlConnection = (HttpURLConnection) mCronetEngine.openConnection(url);
        mUrlConnection.addRequestProperty("foo", testInputString);
        assertThat(mUrlConnection.getResponseCode()).isEqualTo(200);
        assertThat(mUrlConnection.getResponseMessage()).isEqualTo("OK");
        InputStream in = mUrlConnection.getInputStream();

        assertThrows(IndexOutOfBoundsException.class, () -> in.read(new byte[10], -1, 1));
        // Negative byteCount.
        assertThrows(IndexOutOfBoundsException.class, () -> in.read(new byte[10], 1, -1));
        // Read more than what buffer can hold.
        assertThrows(IndexOutOfBoundsException.class, () -> in.read(new byte[10], 0, 11));
    }

    @Test
    @SmallTest
    public void testInputStreamReadOneByte() throws Exception {
        URL url = new URL(NativeTestServer.getEchoBodyURL());
        mUrlConnection = (HttpURLConnection) mCronetEngine.openConnection(url);
        // Make the server echo a large request body, so it exceeds the internal
        // read buffer.
        mUrlConnection.setDoOutput(true);
        mUrlConnection.setRequestMethod("POST");
        byte[] largeData = TestUtil.getLargeData();
        mUrlConnection.setFixedLengthStreamingMode(largeData.length);
        mUrlConnection.getOutputStream().write(largeData);
        InputStream in = mUrlConnection.getInputStream();
        ByteArrayOutputStream out = new ByteArrayOutputStream();
        int b;
        while ((b = in.read()) != -1) {
            out.write(b);
        }

        // All data has been read. Try reading beyond what is available should give -1.
        assertThat(in.read()).isEqualTo(-1);
        assertThat(mUrlConnection.getResponseCode()).isEqualTo(200);
        assertThat(mUrlConnection.getResponseMessage()).isEqualTo("OK");
        String responseData = new String(out.toByteArray());
        TestUtil.checkLargeData(responseData);
    }

    @Test
    @SmallTest
    public void testInputStreamReadMoreBytesThanAvailable() throws Exception {
        String testInputString = "this is a really long header";
        byte[] testInputBytes = testInputString.getBytes();
        URL url = new URL(NativeTestServer.getEchoHeaderURL("foo"));
        mUrlConnection = (HttpURLConnection) mCronetEngine.openConnection(url);
        mUrlConnection.addRequestProperty("foo", testInputString);
        assertThat(mUrlConnection.getResponseCode()).isEqualTo(200);
        assertThat(mUrlConnection.getResponseMessage()).isEqualTo("OK");
        InputStream in = mUrlConnection.getInputStream();
        byte[] actualOutput = new byte[testInputBytes.length + 256];
        int bytesRead = in.read(actualOutput, 0, actualOutput.length);
        assertThat(bytesRead).isEqualTo(testInputBytes.length);
        byte[] readSomeMore = new byte[10];
        int bytesReadBeyondAvailable = in.read(readSomeMore, 0, 10);
        assertThat(bytesReadBeyondAvailable).isEqualTo(-1);
        for (int i = 0; i < bytesRead; i++) {
            assertThat(actualOutput[i]).isEqualTo(testInputBytes[i]);
        }
    }

    /**
     * Tests batch reading on CronetInputStream when {@link CronetHttpURLConnection#getMoreData} is
     * called multiple times.
     */
    @Test
    @SmallTest
    @IgnoreFor(
            implementations = {CronetImplementation.AOSP_PLATFORM},
            reason = "crbug.com/1495309: Enable once we drop MockUrlRequestJobFactory")
    public void testBigDataRead() throws Exception {
        String data = "MyBigFunkyData";
        int dataLength = data.length();
        int repeatCount = 100000;
        MockUrlRequestJobFactory mockUrlRequestJobFactory =
                new MockUrlRequestJobFactory(mTestRule.getTestFramework().getEngine());
        URL url = new URL(MockUrlRequestJobFactory.getMockUrlForData(data, repeatCount));
        mUrlConnection = (HttpURLConnection) mCronetEngine.openConnection(url);
        InputStream in = mUrlConnection.getInputStream();
        byte[] actualOutput = new byte[dataLength * repeatCount];
        int totalBytesRead = 0;
        // Number of bytes to read each time. It is incremented by one from 0.
        int numBytesToRead = 0;
        while (totalBytesRead < actualOutput.length) {
            if (actualOutput.length - totalBytesRead < numBytesToRead) {
                // Do not read out of bound.
                numBytesToRead = actualOutput.length - totalBytesRead;
            }
            int bytesRead = in.read(actualOutput, totalBytesRead, numBytesToRead);
            assertThat(bytesRead).isAtMost(numBytesToRead);
            totalBytesRead += bytesRead;
            numBytesToRead++;
        }

        // All data has been read. Try reading beyond what is available should give -1.
        assertThat(in.read(actualOutput, 0, 0)).isEqualTo(0);
        assertThat(in.read(actualOutput, 0, 1)).isEqualTo(-1);

        String responseData = new String(actualOutput);
        for (int i = 0; i < repeatCount; ++i) {
            assertThat(responseData.substring(dataLength * i, dataLength * (i + 1)))
                    .isEqualTo(data);
        }
        assertThat(mUrlConnection.getResponseCode()).isEqualTo(200);
        assertThat(mUrlConnection.getResponseMessage()).isEqualTo("OK");
        mockUrlRequestJobFactory.shutdown();
    }

    @Test
    @SmallTest
    public void testInputStreamReadExactBytesAvailable() throws Exception {
        String testInputString = "this is a really long header";
        byte[] testInputBytes = testInputString.getBytes();
        URL url = new URL(NativeTestServer.getEchoHeaderURL("foo"));
        mUrlConnection = (HttpURLConnection) mCronetEngine.openConnection(url);
        mUrlConnection.addRequestProperty("foo", testInputString);
        assertThat(mUrlConnection.getResponseCode()).isEqualTo(200);
        assertThat(mUrlConnection.getResponseMessage()).isEqualTo("OK");
        InputStream in = mUrlConnection.getInputStream();
        byte[] actualOutput = new byte[testInputBytes.length];
        int bytesRead = in.read(actualOutput, 0, actualOutput.length);
        mUrlConnection.disconnect();
        assertThat(bytesRead).isEqualTo(testInputBytes.length);
        assertThat(actualOutput).isEqualTo(testInputBytes);
    }

    @Test
    @SmallTest
    public void testInputStreamReadLessBytesThanAvailable() throws Exception {
        String testInputString = "this is a really long header";
        byte[] testInputBytes = testInputString.getBytes();
        URL url = new URL(NativeTestServer.getEchoHeaderURL("foo"));
        mUrlConnection = (HttpURLConnection) mCronetEngine.openConnection(url);
        mUrlConnection.addRequestProperty("foo", testInputString);
        assertThat(mUrlConnection.getResponseCode()).isEqualTo(200);
        assertThat(mUrlConnection.getResponseMessage()).isEqualTo("OK");
        InputStream in = mUrlConnection.getInputStream();
        byte[] firstPart = new byte[testInputBytes.length - 10];
        int firstBytesRead = in.read(firstPart, 0, testInputBytes.length - 10);
        byte[] secondPart = new byte[10];
        int secondBytesRead = in.read(secondPart, 0, 10);
        assertThat(firstBytesRead).isEqualTo(testInputBytes.length - 10);
        assertThat(secondBytesRead).isEqualTo(10);
        for (int i = 0; i < firstPart.length; i++) {
            assertThat(firstPart[i]).isEqualTo(testInputBytes[i]);
        }
        for (int i = 0; i < secondPart.length; i++) {
            assertThat(secondPart[i]).isEqualTo(testInputBytes[firstPart.length + i]);
        }
    }

    /**
     * Makes sure that disconnect while reading from InputStream, the message loop does not block.
     * Regression test for crbug.com/550605.
     */
    @Test
    @SmallTest
    public void testDisconnectWhileReadingDoesnotBlock() throws Exception {
        URL url = new URL(NativeTestServer.getEchoBodyURL());
        mUrlConnection = (HttpURLConnection) mCronetEngine.openConnection(url);
        // Make the server echo a large request body, so it exceeds the internal
        // read buffer.
        mUrlConnection.setDoOutput(true);
        mUrlConnection.setRequestMethod("POST");
        byte[] largeData = TestUtil.getLargeData();
        mUrlConnection.setFixedLengthStreamingMode(largeData.length);
        OutputStream out = mUrlConnection.getOutputStream();
        out.write(largeData);

        InputStream in = mUrlConnection.getInputStream();
        // Read one byte and disconnect.
        assertThat(in.read()).isNotEqualTo(1);
        mUrlConnection.disconnect();
        // TODO(crbug.com/40916513): This might be racy
        // Continue reading, and make sure the message loop will not block and the connection is
        // disconnected before EOF, since the response body is big.
        IOException e =
                assertThrows(
                        IOException.class,
                        () -> {
                            while (in.read() != -1) {}
                        });
        assertThat(e).hasMessageThat().isEqualTo("disconnect() called");
        // Read once more, and make sure exception is thrown.
        e = assertThrows(IOException.class, in::read);
        assertThat(e).hasMessageThat().isEqualTo("disconnect() called");
    }

    /**
     * Makes sure that {@link UrlRequest.Callback#onFailed} exception is propagated when calling
     * read on the input stream.
     */
    @Test
    @SmallTest
    public void testServerHangsUp() throws Exception {
        URL url = new URL(NativeTestServer.getExabyteResponseURL());
        mUrlConnection = (HttpURLConnection) mCronetEngine.openConnection(url);
        InputStream in = mUrlConnection.getInputStream();
        // Read one byte and shut down the server.
        assertThat(in.read()).isNotEqualTo(-1);
        NativeTestServer.shutdownNativeTestServer();
        // Continue reading, and make sure the message loop will not block and the server closes
        // the connection before EOF is received.
        IOException e =
                assertThrows(
                        IOException.class,
                        () -> {
                            while (in.read() != -1) {}
                        });
        assertThat(e).hasMessageThat().contains("net::ERR_CONTENT_LENGTH_MISMATCH");

        // Read once more, and make sure exception is thrown.
        e = assertThrows(IOException.class, in::read);
        assertThat(e).hasMessageThat().contains("net::ERR_CONTENT_LENGTH_MISMATCH");
        // Spins up server to avoid crash when shutting it down in tearDown().
        assertThat(
                        NativeTestServer.startNativeTestServer(
                                mTestRule.getTestFramework().getContext()))
                .isTrue();
    }

    @Test
    @SmallTest
    public void testFollowRedirects() throws Exception {
        URL url = new URL(NativeTestServer.getFileURL("/redirect.html"));
        mUrlConnection = (HttpURLConnection) mCronetEngine.openConnection(url);
        mUrlConnection.setInstanceFollowRedirects(true);
        assertThat(mUrlConnection.getResponseCode()).isEqualTo(200);
        assertThat(mUrlConnection.getResponseMessage()).isEqualTo("OK");
        assertThat(mUrlConnection.getURL().toString())
                .isEqualTo(NativeTestServer.getFileURL("/success.txt"));
        assertThat(TestUtil.getResponseAsString(mUrlConnection)).isEqualTo("this is a text file\n");
    }

    @Test
    @SmallTest
    public void testDisableRedirects() throws Exception {
        URL url = new URL(NativeTestServer.getFileURL("/redirect.html"));
        mUrlConnection = (HttpURLConnection) mCronetEngine.openConnection(url);
        mUrlConnection.setInstanceFollowRedirects(false);
        // Redirect following control broken in Android Marshmallow:
        // https://code.google.com/p/android/issues/detail?id=194495
        if (Build.VERSION.SDK_INT != Build.VERSION_CODES.M) {
            assertThat(mUrlConnection.getResponseCode()).isEqualTo(302);
            assertThat(mUrlConnection.getResponseMessage()).isEqualTo("Found");
            assertThat(mUrlConnection.getHeaderField("Location")).isEqualTo("/success.txt");
            assertThat(mUrlConnection.getURL().toString())
                    .isEqualTo(NativeTestServer.getFileURL("/redirect.html"));
        }
    }

    @Test
    @SmallTest
    public void testDisableRedirectsGlobal() throws Exception {
        HttpURLConnection.setFollowRedirects(false);
        URL url = new URL(NativeTestServer.getFileURL("/redirect.html"));
        mUrlConnection = (HttpURLConnection) mCronetEngine.openConnection(url);
        // Redirect following control broken in Android Marshmallow:
        // https://code.google.com/p/android/issues/detail?id=194495
        if (Build.VERSION.SDK_INT != Build.VERSION_CODES.M) {
            assertThat(mUrlConnection.getResponseCode()).isEqualTo(302);
            assertThat(mUrlConnection.getResponseMessage()).isEqualTo("Found");
            assertThat(mUrlConnection.getHeaderField("Location")).isEqualTo("/success.txt");
            assertThat(mUrlConnection.getURL().toString())
                    .isEqualTo(NativeTestServer.getFileURL("/redirect.html"));
        }
    }

    @Test
    @SmallTest
    public void testDisableRedirectsGlobalAfterConnectionIsCreated() throws Exception {
        HttpURLConnection.setFollowRedirects(true);
        URL url = new URL(NativeTestServer.getFileURL("/redirect.html"));
        mUrlConnection = (HttpURLConnection) mCronetEngine.openConnection(url);
        // Disabling redirects globally after creating the HttpURLConnection
        // object should have no effect on the request.
        HttpURLConnection.setFollowRedirects(false);
        assertThat(mUrlConnection.getResponseCode()).isEqualTo(200);
        assertThat(mUrlConnection.getResponseMessage()).isEqualTo("OK");
        assertThat(mUrlConnection.getURL().toString())
                .isEqualTo(NativeTestServer.getFileURL("/success.txt"));
        assertThat(TestUtil.getResponseAsString(mUrlConnection)).isEqualTo("this is a text file\n");
    }

    @Test
    @SmallTest
    // Cronet does not support reading response body of a 302 response.
    public void testDisableRedirectsTryReadBody() throws Exception {
        URL url = new URL(NativeTestServer.getFileURL("/redirect.html"));
        mUrlConnection = (HttpURLConnection) mCronetEngine.openConnection(url);
        mUrlConnection.setInstanceFollowRedirects(false);
        assertThrows(IOException.class, mUrlConnection::getInputStream);
        assertThat(mUrlConnection.getErrorStream()).isNull();
    }

    @Test
    @SmallTest
    // Tests that redirects across the HTTP and HTTPS boundary are not followed.
    public void testDoNotFollowRedirectsIfSchemesDontMatch() throws Exception {
        URL url = new URL(NativeTestServer.getFileURL("/redirect_invalid_scheme.html"));
        mUrlConnection = (HttpURLConnection) mCronetEngine.openConnection(url);
        mUrlConnection.setInstanceFollowRedirects(true);
        assertThat(mUrlConnection.getResponseCode()).isEqualTo(302);
        assertThat(mUrlConnection.getResponseMessage()).isEqualTo("Found");
        // Redirect is not followed, but the url is updated to the Location header.
        assertThat(mUrlConnection.getURL().toString())
                .isEqualTo("https://127.0.0.1:8000/success.txt");
    }

    @Test
    @SmallTest
    public void testGetResponseHeadersAsMap() throws Exception {
        URL url = new URL(NativeTestServer.getFileURL("/success.txt"));
        mUrlConnection = (HttpURLConnection) mCronetEngine.openConnection(url);
        Map<String, List<String>> responseHeaders = mUrlConnection.getHeaderFields();
        // Make sure response header map is not modifiable.
        assertThrows(
                UnsupportedOperationException.class,
                () -> responseHeaders.put("foo", Arrays.asList("v1", "v2")));

        List<String> contentType = responseHeaders.get("Content-type");
        // Make sure map value is not modifiable as well.
        assertThrows(UnsupportedOperationException.class, () -> contentType.add("v3"));

        // Make sure map look up is key insensitive.
        List<String> contentTypeWithOddCase = responseHeaders.get("ContENt-tYpe");
        assertThat(contentTypeWithOddCase).isEqualTo(contentType);

        assertThat(contentType).containsExactly("text/plain");
        List<String> accessControl = responseHeaders.get("Access-Control-Allow-Origin");
        assertThat(accessControl).containsExactly("*");
        List<String> singleHeader = responseHeaders.get("header-name");
        assertThat(singleHeader).containsExactly("header-value");
        List<String> multiHeader = responseHeaders.get("multi-header-name");
        assertThat(multiHeader).containsExactly("header-value1", "header-value2");
    }

    @Test
    @SmallTest
    public void testGetResponseHeaderField() throws Exception {
        URL url = new URL(NativeTestServer.getFileURL("/success.txt"));
        mUrlConnection = (HttpURLConnection) mCronetEngine.openConnection(url);
        assertThat(mUrlConnection.getHeaderField("Content-Type")).isEqualTo("text/plain");
        assertThat(mUrlConnection.getHeaderField("Access-Control-Allow-Origin")).isEqualTo("*");
        assertThat(mUrlConnection.getHeaderField("header-name")).isEqualTo("header-value");
        // If there are multiple headers with the same name, the last should be
        // returned.
        assertThat(mUrlConnection.getHeaderField("multi-header-name")).isEqualTo("header-value2");
        // Lastly, make sure lookup is case-insensitive.
        assertThat(mUrlConnection.getHeaderField("MUlTi-heAder-name")).isEqualTo("header-value2");
    }

    @Test
    @SmallTest
    public void testGetResponseHeaderFieldWithPos() throws Exception {
        URL url = new URL(NativeTestServer.getFileURL("/success.txt"));
        mUrlConnection = (HttpURLConnection) mCronetEngine.openConnection(url);

        assertThat(mUrlConnection.getHeaderFieldKey(0)).isEqualTo("Content-Type");
        assertThat(mUrlConnection.getHeaderField(0)).isEqualTo("text/plain");
        assertThat(mUrlConnection.getHeaderFieldKey(1)).isEqualTo("Access-Control-Allow-Origin");
        assertThat(mUrlConnection.getHeaderField(1)).isEqualTo("*");
        assertThat(mUrlConnection.getHeaderFieldKey(2)).isEqualTo("header-name");
        assertThat(mUrlConnection.getHeaderField(2)).isEqualTo("header-value");
        assertThat(mUrlConnection.getHeaderFieldKey(3)).isEqualTo("multi-header-name");
        assertThat(mUrlConnection.getHeaderField(3)).isEqualTo("header-value1");
        assertThat(mUrlConnection.getHeaderFieldKey(4)).isEqualTo("multi-header-name");
        assertThat(mUrlConnection.getHeaderField(4)).isEqualTo("header-value2");
    }

    @Test
    @SmallTest
    public void testGetResponseHeaderFieldWithPosExceed() throws Exception {
        URL url = new URL(NativeTestServer.getFileURL("/success.txt"));
        mUrlConnection = (HttpURLConnection) mCronetEngine.openConnection(url);
        // Expect null if we exceed the number of header entries.
        assertThat(mUrlConnection.getHeaderFieldKey(5)).isNull();
        assertThat(mUrlConnection.getHeaderField(5)).isNull();
        assertThat(mUrlConnection.getHeaderFieldKey(6)).isNull();
        assertThat(mUrlConnection.getHeaderField(6)).isNull();
    }

    @Test
    @SmallTest
    // Test that Cronet strips content-encoding header.
    public void testStripContentEncoding() throws Exception {
        URL url = new URL(NativeTestServer.getFileURL("/gzipped.html"));
        mUrlConnection = (HttpURLConnection) mCronetEngine.openConnection(url);
        assertThat(mUrlConnection.getHeaderFieldKey(0)).isEqualTo("foo");
        assertThat(mUrlConnection.getHeaderField(0)).isEqualTo("bar");
        assertThat(mUrlConnection.getHeaderField("content-encoding")).isNull();
        Map<String, List<String>> responseHeaders = mUrlConnection.getHeaderFields();
        assertThat(responseHeaders).hasSize(1);
        assertThat(mUrlConnection.getResponseCode()).isEqualTo(200);
        assertThat(mUrlConnection.getResponseMessage()).isEqualTo("OK");
        // Make sure Cronet decodes the gzipped content.
        assertThat(TestUtil.getResponseAsString(mUrlConnection)).isEqualTo("Hello, World!");
    }

    private static enum CacheSetting {
        USE_CACHE,
        DONT_USE_CACHE
    };

    private static enum ExpectedOutcome {
        SUCCESS,
        FAILURE
    };

    /**
     * Helper method to make a request with cache enabled or disabled, and check whether the request
     * is successful.
     *
     * @param requestUrl request url.
     * @param cacheSetting indicates cache should be used.
     * @param outcome indicates request is expected to be successful.
     */
    private void checkRequestCaching(
            String requestUrl, CacheSetting cacheSetting, ExpectedOutcome outcome)
            throws Exception {
        URL url = new URL(requestUrl);
        mUrlConnection = (HttpURLConnection) mCronetEngine.openConnection(url);
        mUrlConnection.setUseCaches(cacheSetting == CacheSetting.USE_CACHE);
        if (outcome == ExpectedOutcome.SUCCESS) {
            assertThat(mUrlConnection.getResponseCode()).isEqualTo(200);
            assertThat(TestUtil.getResponseAsString(mUrlConnection))
                    .isEqualTo("this is a cacheable file\n");
        } else {
            assertThrows(IOException.class, mUrlConnection::getResponseCode);
        }
    }

    @Test
    @SmallTest
    public void testSetUseCaches() throws Exception {
        String url = NativeTestServer.getFileURL("/cacheable.txt");
        checkRequestCaching(url, CacheSetting.USE_CACHE, ExpectedOutcome.SUCCESS);
        // Shut down the server, we should be able to receive a cached response.
        NativeTestServer.shutdownNativeTestServer();
        checkRequestCaching(url, CacheSetting.USE_CACHE, ExpectedOutcome.SUCCESS);
    }

    @Test
    @SmallTest
    public void testSetUseCachesFalse() throws Exception {
        String url = NativeTestServer.getFileURL("/cacheable.txt");
        checkRequestCaching(url, CacheSetting.USE_CACHE, ExpectedOutcome.SUCCESS);
        NativeTestServer.shutdownNativeTestServer();
        // Disables caching. No cached response is received.
        checkRequestCaching(url, CacheSetting.DONT_USE_CACHE, ExpectedOutcome.FAILURE);
    }

    @Test
    @SmallTest
    // Tests that if disconnect() is called on a different thread when
    // getResponseCode() is still waiting for response, there is no
    // NPE but only IOException.
    // Regression test for crbug.com/751786
    public void testDisconnectWhenGetResponseCodeIsWaiting() throws Exception {
        ServerSocket hangingServer = new ServerSocket(0);
        URL url = new URL("http://localhost:" + hangingServer.getLocalPort());
        mUrlConnection = (HttpURLConnection) mCronetEngine.openConnection(url);
        // connect() is non-blocking. This is to make sure disconnect() triggers cancellation.
        mUrlConnection.connect();
        FutureTask<IOException> task =
                new FutureTask<IOException>(
                        new Callable<IOException>() {
                            @Override
                            public IOException call() {
                                try {
                                    mUrlConnection.getResponseCode();
                                } catch (IOException e) {
                                    return e;
                                }
                                return null;
                            }
                        });
        new Thread(task).start();
        Socket s = hangingServer.accept();
        mUrlConnection.disconnect();
        IOException e = task.get();
        assertThat(e).hasMessageThat().isEqualTo("disconnect() called");
        s.close();
        hangingServer.close();
    }

    private static void checkExceptionsAreThrown(HttpURLConnection urlConnection) throws Exception {
        assertThrows(IOException.class, urlConnection::getInputStream);
        assertThrows(IOException.class, urlConnection::getResponseCode);
        assertThrows(IOException.class, urlConnection::getResponseMessage);

        Map<String, List<String>> headers = urlConnection.getHeaderFields();
        assertThat(headers).isNotNull();
        assertThat(headers).isEmpty();
        // Skip getHeaderFields(), since it can return null or an empty map.
        assertThat(urlConnection.getHeaderField("foo")).isNull();
        assertThat(urlConnection.getHeaderFieldKey(0)).isNull();
        assertThat(urlConnection.getHeaderField(0)).isNull();

        // getErrorStream() does not have a throw clause, it returns null if
        // there's an exception.
        InputStream errorStream = urlConnection.getErrorStream();
        assertThat(errorStream).isNull();
    }

    /** Helper method to extract a list of header values with the give header name. */
    private List<String> getRequestHeaderValues(String allHeaders, String headerName) {
        Pattern pattern = Pattern.compile(headerName + ":\\s(.*)\\r\\n");
        Matcher matcher = pattern.matcher(allHeaders);
        List<String> headerValues = new ArrayList<String>();
        while (matcher.find()) {
            headerValues.add(matcher.group(1));
        }
        return headerValues;
    }

    @Test
    @SmallTest
    @RequiresMinApi(9) // Tagging support added in API level 9: crrev.com/c/chromium/src/+/930086
    @RequiresMinAndroidApi(Build.VERSION_CODES.M) // crbug/1301957
    public void testTagging() throws Exception {
        if (!CronetTestUtil.nativeCanGetTaggedBytes()) {
            Log.i(TAG, "Skipping test - GetTaggedBytes unsupported.");
            return;
        }
        URL url = new URL(NativeTestServer.getEchoMethodURL());

        // Test untagged requests are given tag 0.
        int tag = 0;
        long priorBytes = CronetTestUtil.nativeGetTaggedBytes(tag);
        CronetHttpURLConnection cronetUrlConnection =
                (CronetHttpURLConnection) mCronetEngine.openConnection(url);
        try {
            assertThat(cronetUrlConnection.getResponseCode()).isEqualTo(200);
            cronetUrlConnection.disconnect();
            assertThat(CronetTestUtil.nativeGetTaggedBytes(tag)).isGreaterThan(priorBytes);

            // Test explicit tagging.
            tag = 0x12345678;
            priorBytes = CronetTestUtil.nativeGetTaggedBytes(tag);
            cronetUrlConnection = (CronetHttpURLConnection) mCronetEngine.openConnection(url);
            cronetUrlConnection.setTrafficStatsTag(tag);
            assertThat(cronetUrlConnection.getResponseCode()).isEqualTo(200);
            cronetUrlConnection.disconnect();
            assertThat(CronetTestUtil.nativeGetTaggedBytes(tag)).isGreaterThan(priorBytes);

            // Test a different tag value.
            tag = 0x87654321;
            priorBytes = CronetTestUtil.nativeGetTaggedBytes(tag);
            cronetUrlConnection = (CronetHttpURLConnection) mCronetEngine.openConnection(url);
            cronetUrlConnection.setTrafficStatsTag(tag);
            assertThat(cronetUrlConnection.getResponseCode()).isEqualTo(200);
            cronetUrlConnection.disconnect();
            assertThat(CronetTestUtil.nativeGetTaggedBytes(tag)).isGreaterThan(priorBytes);

            // Test tagging with TrafficStats.
            tag = 0x12348765;
            priorBytes = CronetTestUtil.nativeGetTaggedBytes(tag);
            cronetUrlConnection = (CronetHttpURLConnection) mCronetEngine.openConnection(url);
            TrafficStats.setThreadStatsTag(tag);
            assertThat(cronetUrlConnection.getResponseCode()).isEqualTo(200);
            cronetUrlConnection.disconnect();
            assertThat(CronetTestUtil.nativeGetTaggedBytes(tag)).isGreaterThan(priorBytes);

            // Test tagging with our UID.
            // NOTE(pauljensen): Explicitly setting the UID to the current UID isn't a particularly
            // thorough test of this API but at least provides coverage of the underlying code, and
            // verifies that traffic is still properly attributed.
            // The code path for UID is parallel to that for the tag, which we do have more thorough
            // testing for.  More thorough testing of setting the UID would require running tests
            // with a rare permission which isn't realistic for most apps.  Apps are allowed to set
            // the UID to their own UID as per this logic in the tagging kernel module:
            // https://android.googlesource.com/kernel/common/+/21dd5d7/net/netfilter/xt_qtaguid.c#154
            tag = 0;
            priorBytes = CronetTestUtil.nativeGetTaggedBytes(tag);
            cronetUrlConnection = (CronetHttpURLConnection) mCronetEngine.openConnection(url);
            cronetUrlConnection.setTrafficStatsUid(Process.myUid());
            assertThat(cronetUrlConnection.getResponseCode()).isEqualTo(200);
            cronetUrlConnection.disconnect();
            assertThat(CronetTestUtil.nativeGetTaggedBytes(tag)).isGreaterThan(priorBytes);

            // TrafficStats.getThreadStatsUid() which is required for this feature is added in API
            // level 28. Note, currently this part won't run as
            // CronetTestUtil.nativeCanGetTaggedBytes() will return false on P+.
            if (Build.VERSION.SDK_INT >= Build.VERSION_CODES.P) {
                tag = 0;
                priorBytes = CronetTestUtil.nativeGetTaggedBytes(tag);
                cronetUrlConnection = (CronetHttpURLConnection) mCronetEngine.openConnection(url);
                TrafficStats.setThreadStatsUid(Process.myUid());
                assertThat(cronetUrlConnection.getResponseCode()).isEqualTo(200);
                cronetUrlConnection.disconnect();
                assertThat(CronetTestUtil.nativeGetTaggedBytes(tag)).isGreaterThan(priorBytes);
            }
        } finally {
            cronetUrlConnection.disconnect();
        }
    }

    @Test
    @SmallTest
    public void testIOExceptionErrorRethrown() throws Exception {
        // URL that should fail to connect.
        URL url = new URL("http://localhost");
        mUrlConnection = (HttpURLConnection) mCronetEngine.openConnection(url);
        // This should not throw, even though internally it may encounter an exception.
        mUrlConnection.getHeaderField("blah");
        // This should throw an IOException.
        assertThrows(IOException.class, mUrlConnection::getResponseCode);
    }

    @Test
    @SmallTest
    public void testIOExceptionInterruptRethrown() throws Exception {
        ServerSocket hangingServer = new ServerSocket(0);
        URL url = new URL("http://localhost:" + hangingServer.getLocalPort());
        mUrlConnection = (HttpURLConnection) mCronetEngine.openConnection(url);
        // connect() is non-blocking.
        mUrlConnection.connect();
        FutureTask<IOException> task =
                new FutureTask<IOException>(
                        new Callable<IOException>() {
                            @Override
                            public IOException call() {
                                // This should not throw, even though internally it may encounter an
                                // exception.
                                mUrlConnection.getHeaderField("blah");
                                try {
                                    // This should throw an InterruptedIOException.
                                    mUrlConnection.getResponseCode();
                                } catch (InterruptedIOException e) {
                                    // Expected
                                    return e;
                                } catch (IOException e) {
                                    return null;
                                }
                                return null;
                            }
                        });
        Thread t = new Thread(task);
        t.start();
        Socket s = hangingServer.accept();
        hangingServer.close();
        Thread.sleep(100); // Give time for thread to get blocked, so interrupt is noticed.
        // This will trigger an InterruptException in getHeaderField() and getResponseCode().
        // getHeaderField() should not re-throw it.  getResponseCode() should re-throw it as an
        // InterruptedIOException.
        t.interrupt();
        // Make sure an IOException is thrown.
        assertThat(task.get()).isNotNull();
        s.close();
    }

    @Test
    @SmallTest
    // Regression test for crashes in disconnect() impl.
    public void testCancelRace() throws Exception {
        URL url = new URL(NativeTestServer.getEchoMethodURL());
        mUrlConnection = (HttpURLConnection) mCronetEngine.openConnection(url);
        final AtomicBoolean connected = new AtomicBoolean();
        // Start request on another thread.
        Thread thread =
                new Thread(
                        new Runnable() {
                            @Override
                            public void run() {
                                try {
                                    assertThat(mUrlConnection.getResponseCode()).isEqualTo(200);
                                } catch (IOException e) {
                                }
                                connected.set(true);
                            }
                        });
        thread.start();
        // Repeatedly call disconnect().  This used to crash.
        do {
            mUrlConnection.disconnect();
        } while (!connected.get());
    }
}
