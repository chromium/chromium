// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.net.urlconnection;

import static org.junit.Assert.assertEquals;
import static org.junit.Assert.assertNotNull;
import static org.junit.Assert.assertNull;
import static org.junit.Assert.assertTrue;
import static org.junit.Assert.fail;

import static org.chromium.net.CronetTestRule.getContext;

import android.os.Build;
import android.os.Process;
import android.support.test.filters.SmallTest;
import android.support.test.runner.AndroidJUnit4;

import org.junit.After;
import org.junit.Before;
import org.junit.Rule;
import org.junit.Test;
import org.junit.runner.RunWith;

import org.chromium.base.Log;
import org.chromium.base.test.util.Feature;
import org.chromium.net.CronetEngine;
import org.chromium.net.CronetException;
import org.chromium.net.CronetTestRule;
import org.chromium.net.CronetTestRule.CompareDefaultWithCronet;
import org.chromium.net.CronetTestRule.OnlyRunCronetHttpURLConnection;
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
import java.lang.reflect.Method;
import java.net.HttpURLConnection;
import java.net.MalformedURLException;
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

/**
 * Basic tests of Cronet's HttpURLConnection implementation.
 * Tests annotated with {@code CompareDefaultWithCronet} will run once with the
 * default HttpURLConnection implementation and then with Cronet's
 * HttpURLConnection implementation. Tests annotated with
 * {@code OnlyRunCronetHttpURLConnection} only run Cronet's implementation.
 * See {@link CronetTestBase#runTest()} for details.
 */
@RunWith(AndroidJUnit4.class)
public class CronetHttpURLConnectionTest {
    private static final String TAG = CronetHttpURLConnectionTest.class.getSimpleName();

    @Rule
    public final CronetTestRule mTestRule = new CronetTestRule();

    private CronetEngine mCronetEngine;

    @Before
    public void setUp() throws Exception {
        mCronetEngine = mTestRule.enableDiskCache(new CronetEngine.Builder(getContext())).build();
        mTestRule.setStreamHandlerFactory(mCronetEngine);
        assertTrue(NativeTestServer.startNativeTestServer(getContext()));
    }

    @After
    public void tearDown() throws Exception {
        NativeTestServer.shutdownNativeTestServer();
        mCronetEngine.shutdown();
    }

    @Test
    @SmallTest
    @Feature({"Cronet"})
    @CompareDefaultWithCronet
    public void testBasicGet() throws Exception {
        URL url = new URL(NativeTestServer.getEchoMethodURL());
        HttpURLConnection urlConnection =
                (HttpURLConnection) url.openConnection();
        assertEquals(200, urlConnection.getResponseCode());
        assertEquals("OK", urlConnection.getResponseMessage());
        assertEquals("GET", TestUtil.getResponseAsString(urlConnection));
        urlConnection.disconnect();
    }

    @Test
    @SmallTest
    @Feature({"Cronet"})
    @CompareDefaultWithCronet
    // Regression test for crbug.com/561678.
    public void testSetRequestMethod() throws Exception {
        URL url = new URL(NativeTestServer.getEchoMethodURL());
        HttpURLConnection connection = (HttpURLConnection) url.openConnection();
        connection.setDoOutput(true);
        connection.setRequestMethod("PUT");
        OutputStream out = connection.getOutputStream();
        out.write("dummy data".getBytes());
        assertEquals(200, connection.getResponseCode());
        assertEquals("OK", connection.getResponseMessage());
        assertEquals("PUT", TestUtil.getResponseAsString(connection));
        connection.disconnect();
    }

    @Test
    @SmallTest
    @Feature({"Cronet"})
    @OnlyRunCronetHttpURLConnection
    public void testConnectTimeout() throws Exception {
        URL url = new URL(NativeTestServer.getEchoMethodURL());
        HttpURLConnection connection = (HttpURLConnection) url.openConnection();
        // This should not throw an exception.
        connection.setConnectTimeout(1000);
        assertEquals(200, connection.getResponseCode());
        assertEquals("OK", connection.getResponseMessage());
        assertEquals("GET", TestUtil.getResponseAsString(connection));
        connection.disconnect();
    }

    @Test
    @SmallTest
    @Feature({"Cronet"})
    @OnlyRunCronetHttpURLConnection
    public void testReadTimeout() throws Exception {
        // Add url interceptors.
        MockUrlRequestJobFactory mockUrlRequestJobFactory =
                new MockUrlRequestJobFactory(mCronetEngine);
        URL url = new URL(MockUrlRequestJobFactory.getMockUrlForHangingRead());
        HttpURLConnection connection = (HttpURLConnection) url.openConnection();
        connection.setReadTimeout(1000);
        assertEquals(200, connection.getResponseCode());
        InputStream in = connection.getInputStream();
        try {
            in.read();
            fail();
        } catch (SocketTimeoutException e) {
            // Expected
        }
        connection.disconnect();
        mockUrlRequestJobFactory.shutdown();
    }

    @Test
    @SmallTest
    @Feature({"Cronet"})
    @CompareDefaultWithCronet
    // Regression test for crbug.com/571436.
    public void testDefaultToPostWhenDoOutput() throws Exception {
        URL url = new URL(NativeTestServer.getEchoMethodURL());
        HttpURLConnection connection = (HttpURLConnection) url.openConnection();
        connection.setDoOutput(true);
        OutputStream out = connection.getOutputStream();
        out.write("dummy data".getBytes());
        assertEquals(200, connection.getResponseCode());
        assertEquals("OK", connection.getResponseMessage());
        assertEquals("POST", TestUtil.getResponseAsString(connection));
        connection.disconnect();
    }

    /**
     * Tests that calling {@link HttpURLConnection#connect} will also initialize
     * {@code OutputStream} if necessary in the case where
     * {@code setFixedLengthStreamingMode} is called.
     * Regression test for crbug.com/582975.
     */
    @Test
    @SmallTest
    @Feature({"Cronet"})
    @CompareDefaultWithCronet
    public void testInitOutputStreamInConnect() throws Exception {
        URL url = new URL(NativeTestServer.getEchoBodyURL());
        HttpURLConnection connection = (HttpURLConnection) url.openConnection();
        connection.setDoOutput(true);
        String dataString = "some very important data";
        byte[] data = dataString.getBytes();
        connection.setFixedLengthStreamingMode(data.length);
        connection.connect();
        OutputStream out = connection.getOutputStream();
        out.write(data);
        assertEquals(200, connection.getResponseCode());
        assertEquals("OK", connection.getResponseMessage());
        assertEquals(dataString, TestUtil.getResponseAsString(connection));
        connection.disconnect();
    }

    /**
     * Tests that calling {@link HttpURLConnection#connect} will also initialize
     * {@code OutputStream} if necessary in the case where
     * {@code setChunkedStreamingMode} is called.
     * Regression test for crbug.com/582975.
     */
    @Test
    @SmallTest
    @Feature({"Cronet"})
    @CompareDefaultWithCronet
    public void testInitChunkedOutputStreamInConnect() throws Exception {
        URL url = new URL(NativeTestServer.getEchoBodyURL());
        HttpURLConnection connection = (HttpURLConnection) url.openConnection();
        connection.setDoOutput(true);
        String dataString = "some very important chunked data";
        byte[] data = dataString.getBytes();
        connection.setChunkedStreamingMode(0);
        connection.connect();
        OutputStream out = connection.getOutputStream();
        out.write(data);
        assertEquals(200, connection.getResponseCode());
        assertEquals("OK", connection.getResponseMessage());
        assertEquals(dataString, TestUtil.getResponseAsString(connection));
        connection.disconnect();
    }

    /**
     * Tests that using reflection to find {@code fixedContentLengthLong} works.
     */
    @Test
    @SmallTest
    @Feature({"Cronet"})
    @OnlyRunCronetHttpURLConnection
    public void testSetFixedLengthStreamingModeLong() throws Exception {
        URL url = new URL(NativeTestServer.getEchoBodyURL());
        HttpURLConnection connection =
                (HttpURLConnection) url.openConnection();
        connection.setDoOutput(true);
        connection.setRequestMethod("POST");
        if (Build.VERSION.SDK_INT >= Build.VERSION_CODES.KITKAT) {
            String dataString = "some very important data";
            byte[] data = dataString.getBytes();
            Class<?> c = connection.getClass();
            Method method = c.getMethod("setFixedLengthStreamingMode",
                    new Class[] {long.class});
            method.invoke(connection, (long) data.length);
            OutputStream out = connection.getOutputStream();
            out.write(data);
            assertEquals(200, connection.getResponseCode());
            assertEquals("OK", connection.getResponseMessage());
            assertEquals(dataString, TestUtil.getResponseAsString(connection));
            connection.disconnect();
        }
    }

    @Test
    @SmallTest
    @Feature({"Cronet"})
    @CompareDefaultWithCronet
    public void testNotFoundURLRequest() throws Exception {
        URL url = new URL(NativeTestServer.getFileURL("/notfound.html"));
        HttpURLConnection urlConnection =
                (HttpURLConnection) url.openConnection();
        assertEquals(404, urlConnection.getResponseCode());
        assertEquals("Not Found", urlConnection.getResponseMessage());
        try {
            urlConnection.getInputStream();
            fail();
        } catch (FileNotFoundException e) {
            // Expected.
        }
        InputStream errorStream = urlConnection.getErrorStream();
        ByteArrayOutputStream out = new ByteArrayOutputStream();
        int byteRead;
        while ((byteRead = errorStream.read()) != -1) {
            out.write(byteRead);
        }
        assertEquals("<!DOCTYPE html>\n<html>\n<head>\n"
                + "<title>Not found</title>\n<p>Test page loaded.</p>\n"
                + "</head>\n</html>\n",
                out.toString());
        urlConnection.disconnect();
    }

    @Test
    @SmallTest
    @Feature({"Cronet"})
    @CompareDefaultWithCronet
    public void testServerNotAvailable() throws Exception {
        URL url = new URL(NativeTestServer.getFileURL("/success.txt"));
        HttpURLConnection urlConnection =
                (HttpURLConnection) url.openConnection();
        assertEquals("this is a text file\n", TestUtil.getResponseAsString(urlConnection));
        // After shutting down the server, the server should not be handling
        // new requests.
        NativeTestServer.shutdownNativeTestServer();
        HttpURLConnection secondConnection =
                (HttpURLConnection) url.openConnection();
        // Default implementation reports this type of error in connect().
        // However, since Cronet's wrapper only receives the error in its listener
        // callback when message loop is running, Cronet's wrapper only knows
        // about the error when it starts to read response.
        try {
            secondConnection.getResponseCode();
            fail();
        } catch (IOException e) {
            assertTrue(e instanceof java.net.ConnectException || e instanceof CronetException);
            assertTrue(e.getMessage().contains("ECONNREFUSED")
                    || e.getMessage().contains("Connection refused")
                    || e.getMessage().contains("net::ERR_CONNECTION_REFUSED")
                    || e.getMessage().contains("Failed to connect"));
        }
        checkExceptionsAreThrown(secondConnection);
        urlConnection.disconnect();
        // Starts the server to avoid crashing on shutdown in tearDown().
        assertTrue(NativeTestServer.startNativeTestServer(getContext()));
    }

    @Test
    @SmallTest
    @Feature({"Cronet"})
    @CompareDefaultWithCronet
    public void testBadIP() throws Exception {
        URL url = new URL("http://0.0.0.0/");
        HttpURLConnection urlConnection =
                (HttpURLConnection) url.openConnection();
        // Default implementation reports this type of error in connect().
        // However, since Cronet's wrapper only receives the error in its listener
        // callback when message loop is running, Cronet's wrapper only knows
        // about the error when it starts to read response.
        try {
            urlConnection.getResponseCode();
            fail();
        } catch (IOException e) {
            assertTrue(e instanceof java.net.ConnectException || e instanceof CronetException);
            assertTrue(e.getMessage().contains("ECONNREFUSED")
                    || e.getMessage().contains("Connection refused")
                    || e.getMessage().contains("net::ERR_CONNECTION_REFUSED")
                    || e.getMessage().contains("Failed to connect"));
        }
        checkExceptionsAreThrown(urlConnection);
    }

    @Test
    @SmallTest
    @Feature({"Cronet"})
    @CompareDefaultWithCronet
    public void testBadHostname() throws Exception {
        URL url = new URL("http://this-weird-host-name-does-not-exist/");
        HttpURLConnection urlConnection =
                (HttpURLConnection) url.openConnection();
        // Default implementation reports this type of error in connect().
        // However, since Cronet's wrapper only receives the error in its listener
        // callback when message loop is running, Cronet's wrapper only knows
        // about the error when it starts to read response.
        try {
            urlConnection.getResponseCode();
            fail();
        } catch (java.net.UnknownHostException e) {
            // Expected.
        } catch (CronetException e) {
            // Expected.
        }
        checkExceptionsAreThrown(urlConnection);
    }

    @Test
    @SmallTest
    @Feature({"Cronet"})
    @CompareDefaultWithCronet
    public void testBadScheme() throws Exception {
        try {
            new URL("flying://goat");
            fail();
        } catch (MalformedURLException e) {
            // Expected.
        }
    }

    @Test
    @SmallTest
    @Feature({"Cronet"})
    @CompareDefaultWithCronet
    public void testDisconnectBeforeConnectionIsMade() throws Exception {
        URL url = new URL(NativeTestServer.getEchoMethodURL());
        HttpURLConnection urlConnection =
                (HttpURLConnection) url.openConnection();
        // Close connection before connection is made has no effect.
        // Default implementation passes this test.
        urlConnection.disconnect();
        assertEquals(200, urlConnection.getResponseCode());
        assertEquals("OK", urlConnection.getResponseMessage());
        assertEquals("GET", TestUtil.getResponseAsString(urlConnection));
    }

    @Test
    @SmallTest
    @Feature({"Cronet"})
    // TODO(xunjieli): Currently the wrapper does not throw an exception.
    // Need to change the behavior.
    // @CompareDefaultWithCronet
    public void testDisconnectAfterConnectionIsMade() throws Exception {
        URL url = new URL(NativeTestServer.getEchoMethodURL());
        HttpURLConnection urlConnection =
                (HttpURLConnection) url.openConnection();
        // Close connection before connection is made has no effect.
        urlConnection.connect();
        urlConnection.disconnect();
        try {
            urlConnection.getResponseCode();
            fail();
        } catch (Exception e) {
            // Ignored.
        }
        try {
            InputStream in = urlConnection.getInputStream();
            fail();
        } catch (Exception e) {
            // Ignored.
        }
    }

    @Test
    @SmallTest
    @Feature({"Cronet"})
    @CompareDefaultWithCronet
    public void testMultipleDisconnect() throws Exception {
        URL url = new URL(NativeTestServer.getEchoMethodURL());
        HttpURLConnection urlConnection =
                (HttpURLConnection) url.openConnection();
        assertEquals(200, urlConnection.getResponseCode());
        assertEquals("OK", urlConnection.getResponseMessage());
        assertEquals("GET", TestUtil.getResponseAsString(urlConnection));
        // Disconnect multiple times should be fine.
        for (int i = 0; i < 10; i++) {
            urlConnection.disconnect();
        }
    }

    @Test
    @SmallTest
    @Feature({"Cronet"})
    @CompareDefaultWithCronet
    public void testAddRequestProperty() throws Exception {
        URL url = new URL(NativeTestServer.getEchoAllHeadersURL());
        HttpURLConnection connection = (HttpURLConnection) url.openConnection();
        connection.addRequestProperty("foo-header", "foo");
        connection.addRequestProperty("bar-header", "bar");

        // Before connection is made, check request headers are set.
        Map<String, List<String>> requestHeadersMap =
                connection.getRequestProperties();
        List<String> fooValues = requestHeadersMap.get("foo-header");
        assertEquals(1, fooValues.size());
        assertEquals("foo", fooValues.get(0));
        assertEquals("foo", connection.getRequestProperty("foo-header"));
        List<String> barValues = requestHeadersMap.get("bar-header");
        assertEquals(1, barValues.size());
        assertEquals("bar", barValues.get(0));
        assertEquals("bar", connection.getRequestProperty("bar-header"));

        // Check the request headers echoed back by the server.
        assertEquals(200, connection.getResponseCode());
        assertEquals("OK", connection.getResponseMessage());
        String headers = TestUtil.getResponseAsString(connection);
        List<String> fooHeaderValues =
                getRequestHeaderValues(headers, "foo-header");
        List<String> barHeaderValues =
                getRequestHeaderValues(headers, "bar-header");
        assertEquals(1, fooHeaderValues.size());
        assertEquals("foo", fooHeaderValues.get(0));
        assertEquals(1, fooHeaderValues.size());
        assertEquals("bar", barHeaderValues.get(0));

        connection.disconnect();
    }

    @Test
    @SmallTest
    @Feature({"Cronet"})
    @OnlyRunCronetHttpURLConnection
    public void testAddRequestPropertyWithSameKey() throws Exception {
        URL url = new URL(NativeTestServer.getEchoAllHeadersURL());
        HttpURLConnection urlConnection =
                (HttpURLConnection) url.openConnection();
        urlConnection.addRequestProperty("header-name", "value1");
        try {
            urlConnection.addRequestProperty("header-Name", "value2");
            fail();
        } catch (UnsupportedOperationException e) {
            assertEquals(e.getMessage(),
                    "Cannot add multiple headers of the same key, header-Name. "
                    + "crbug.com/432719.");
        }
    }

    @Test
    @SmallTest
    @Feature({"Cronet"})
    @CompareDefaultWithCronet
    public void testSetRequestPropertyWithSameKey() throws Exception {
        URL url = new URL(NativeTestServer.getEchoAllHeadersURL());
        HttpURLConnection conn = (HttpURLConnection) url.openConnection();
        // The test always sets and retrieves one header with the same
        // capitalization, and the other header with slightly different
        // capitalization.
        conn.setRequestProperty("same-capitalization", "yo");
        conn.setRequestProperty("diFFerent-cApitalization", "foo");
        Map<String, List<String>> headersMap = conn.getRequestProperties();
        List<String>  values1 = headersMap.get("same-capitalization");
        assertEquals(1, values1.size());
        assertEquals("yo", values1.get(0));
        assertEquals("yo", conn.getRequestProperty("same-capitalization"));

        List<String> values2 = headersMap.get("different-capitalization");
        assertEquals(1, values2.size());
        assertEquals("foo", values2.get(0));
        assertEquals("foo",
                conn.getRequestProperty("Different-capitalization"));

        // Check request header is updated.
        conn.setRequestProperty("same-capitalization", "hi");
        conn.setRequestProperty("different-Capitalization", "bar");
        Map<String, List<String>> newHeadersMap = conn.getRequestProperties();
        List<String> newValues1 = newHeadersMap.get("same-capitalization");
        assertEquals(1, newValues1.size());
        assertEquals("hi", newValues1.get(0));
        assertEquals("hi", conn.getRequestProperty("same-capitalization"));

        List<String> newValues2 = newHeadersMap.get("differENT-capitalization");
        assertEquals(1, newValues2.size());
        assertEquals("bar", newValues2.get(0));
        assertEquals("bar",
                conn.getRequestProperty("different-capitalization"));

        // Check the request headers echoed back by the server.
        assertEquals(200, conn.getResponseCode());
        assertEquals("OK", conn.getResponseMessage());
        String headers = TestUtil.getResponseAsString(conn);
        List<String> actualValues1 =
                getRequestHeaderValues(headers, "same-capitalization");
        assertEquals(1, actualValues1.size());
        assertEquals("hi", actualValues1.get(0));
        List<String> actualValues2 =
                getRequestHeaderValues(headers, "different-Capitalization");
        assertEquals(1, actualValues2.size());
        assertEquals("bar", actualValues2.get(0));
        conn.disconnect();
    }

    @Test
    @SmallTest
    @Feature({"Cronet"})
    @CompareDefaultWithCronet
    public void testAddAndSetRequestPropertyWithSameKey() throws Exception {
        URL url = new URL(NativeTestServer.getEchoAllHeadersURL());
        HttpURLConnection connection = (HttpURLConnection) url.openConnection();
        connection.addRequestProperty("header-name", "value1");
        connection.setRequestProperty("Header-nAme", "value2");

        // Before connection is made, check request headers are set.
        assertEquals("value2", connection.getRequestProperty("header-namE"));
        Map<String, List<String>> requestHeadersMap =
                connection.getRequestProperties();
        assertEquals(1, requestHeadersMap.get("HeAder-name").size());
        assertEquals("value2", requestHeadersMap.get("HeAder-name").get(0));

        // Check the request headers echoed back by the server.
        assertEquals(200, connection.getResponseCode());
        assertEquals("OK", connection.getResponseMessage());
        String headers = TestUtil.getResponseAsString(connection);
        List<String> actualValues =
                getRequestHeaderValues(headers, "Header-nAme");
        assertEquals(1, actualValues.size());
        assertEquals("value2", actualValues.get(0));
        connection.disconnect();
    }

    @Test
    @SmallTest
    @Feature({"Cronet"})
    @CompareDefaultWithCronet
    public void testAddSetRequestPropertyAfterConnected() throws Exception {
        URL url = new URL(NativeTestServer.getEchoAllHeadersURL());
        HttpURLConnection connection = (HttpURLConnection) url.openConnection();
        connection.addRequestProperty("header-name", "value");
        assertEquals(200, connection.getResponseCode());
        assertEquals("OK", connection.getResponseMessage());
        try {
            connection.setRequestProperty("foo", "bar");
            fail();
        } catch (IllegalStateException e) {
            // Expected.
        }
        try {
            connection.addRequestProperty("foo", "bar");
            fail();
        } catch (IllegalStateException e) {
            // Expected.
        }
        connection.disconnect();
    }

    @Test
    @SmallTest
    @Feature({"Cronet"})
    @CompareDefaultWithCronet
    public void testGetRequestPropertyAfterConnected() throws Exception {
        URL url = new URL(NativeTestServer.getEchoAllHeadersURL());
        HttpURLConnection connection = (HttpURLConnection) url.openConnection();
        connection.addRequestProperty("header-name", "value");
        assertEquals(200, connection.getResponseCode());
        assertEquals("OK", connection.getResponseMessage());

        try {
            connection.getRequestProperties();
            fail();
        } catch (IllegalStateException e) {
            // Expected.
        }

        // Default implementation allows querying a particular request property.
        try {
            assertEquals("value", connection.getRequestProperty("header-name"));
        } catch (IllegalStateException e) {
            fail();
        }
        connection.disconnect();
    }

    @Test
    @SmallTest
    @Feature({"Cronet"})
    @CompareDefaultWithCronet
    public void testGetRequestPropertiesUnmodifiable() throws Exception {
        URL url = new URL(NativeTestServer.getEchoAllHeadersURL());
        HttpURLConnection connection = (HttpURLConnection) url.openConnection();
        connection.addRequestProperty("header-name", "value");
        Map<String, List<String>> headers = connection.getRequestProperties();
        try {
            headers.put("foo", Arrays.asList("v1", "v2"));
            fail();
        } catch (UnsupportedOperationException e) {
            // Expected.
        }

        List<String> values = headers.get("header-name");
        try {
            values.add("v3");
            fail();
        } catch (UnsupportedOperationException e) {
            // Expected.
        }

        connection.disconnect();
    }

    @Test
    @SmallTest
    @Feature({"Cronet"})
    @CompareDefaultWithCronet
    public void testInputStreamBatchReadBoundaryConditions() throws Exception {
        String testInputString = "this is a very important header";
        URL url = new URL(NativeTestServer.getEchoHeaderURL("foo"));
        HttpURLConnection urlConnection =
                (HttpURLConnection) url.openConnection();
        urlConnection.addRequestProperty("foo", testInputString);
        assertEquals(200, urlConnection.getResponseCode());
        assertEquals("OK", urlConnection.getResponseMessage());
        InputStream in = urlConnection.getInputStream();
        try {
            // Negative byteOffset.
            int r = in.read(new byte[10], -1, 1);
            fail();
        } catch (IndexOutOfBoundsException e) {
            // Expected.
        }
        try {
            // Negative byteCount.
            int r = in.read(new byte[10], 1, -1);
            fail();
        } catch (IndexOutOfBoundsException e) {
            // Expected.
        }
        try {
            // Read more than what buffer can hold.
            int r = in.read(new byte[10], 0, 11);
            fail();
        } catch (IndexOutOfBoundsException e) {
            // Expected.
        }
        urlConnection.disconnect();
    }

    @Test
    @SmallTest
    @Feature({"Cronet"})
    @CompareDefaultWithCronet
    public void testInputStreamReadOneByte() throws Exception {
        URL url = new URL(NativeTestServer.getEchoBodyURL());
        final HttpURLConnection connection = (HttpURLConnection) url.openConnection();
        // Make the server echo a large request body, so it exceeds the internal
        // read buffer.
        connection.setDoOutput(true);
        connection.setRequestMethod("POST");
        byte[] largeData = TestUtil.getLargeData();
        connection.setFixedLengthStreamingMode(largeData.length);
        connection.getOutputStream().write(largeData);
        InputStream in = connection.getInputStream();
        ByteArrayOutputStream out = new ByteArrayOutputStream();
        int b;
        while ((b = in.read()) != -1) {
            out.write(b);
        }

        // All data has been read. Try reading beyond what is available should give -1.
        assertEquals(-1, in.read());
        assertEquals(200, connection.getResponseCode());
        assertEquals("OK", connection.getResponseMessage());
        String responseData = new String(out.toByteArray());
        TestUtil.checkLargeData(responseData);
    }

    @Test
    @SmallTest
    @Feature({"Cronet"})
    @CompareDefaultWithCronet
    public void testInputStreamReadMoreBytesThanAvailable() throws Exception {
        String testInputString = "this is a really long header";
        byte[] testInputBytes = testInputString.getBytes();
        URL url = new URL(NativeTestServer.getEchoHeaderURL("foo"));
        HttpURLConnection urlConnection =
                (HttpURLConnection) url.openConnection();
        urlConnection.addRequestProperty("foo", testInputString);
        assertEquals(200, urlConnection.getResponseCode());
        assertEquals("OK", urlConnection.getResponseMessage());
        InputStream in = urlConnection.getInputStream();
        byte[] actualOutput = new byte[testInputBytes.length + 256];
        int bytesRead = in.read(actualOutput, 0, actualOutput.length);
        assertEquals(testInputBytes.length, bytesRead);
        byte[] readSomeMore = new byte[10];
        int bytesReadBeyondAvailable  = in.read(readSomeMore, 0, 10);
        assertEquals(-1, bytesReadBeyondAvailable);
        for (int i = 0; i < bytesRead; i++) {
            assertEquals(testInputBytes[i], actualOutput[i]);
        }
        urlConnection.disconnect();
    }

    /**
     * Tests batch reading on CronetInputStream when
     * {@link CronetHttpURLConnection#getMoreData} is called multiple times.
     */
    @Test
    @SmallTest
    @Feature({"Cronet"})
    @OnlyRunCronetHttpURLConnection
    public void testBigDataRead() throws Exception {
        String data = "MyBigFunkyData";
        int dataLength = data.length();
        int repeatCount = 100000;
        MockUrlRequestJobFactory mockUrlRequestJobFactory =
                new MockUrlRequestJobFactory(mCronetEngine);
        URL url = new URL(MockUrlRequestJobFactory.getMockUrlForData(data, repeatCount));
        HttpURLConnection connection =
                (HttpURLConnection) url.openConnection();
        InputStream in = connection.getInputStream();
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
            assertTrue(bytesRead <= numBytesToRead);
            totalBytesRead += bytesRead;
            numBytesToRead++;
        }

        // All data has been read. Try reading beyond what is available should give -1.
        assertEquals(0, in.read(actualOutput, 0, 0));
        assertEquals(-1, in.read(actualOutput, 0, 1));

        String responseData = new String(actualOutput);
        for (int i = 0; i < repeatCount; ++i) {
            assertEquals(data, responseData.substring(dataLength * i,
                    dataLength * (i + 1)));
        }
        assertEquals(200, connection.getResponseCode());
        assertEquals("OK", connection.getResponseMessage());
        mockUrlRequestJobFactory.shutdown();
    }

    @Test
    @SmallTest
    @Feature({"Cronet"})
    @CompareDefaultWithCronet
    public void testInputStreamReadExactBytesAvailable() throws Exception {
        String testInputString = "this is a really long header";
        byte[] testInputBytes = testInputString.getBytes();
        URL url = new URL(NativeTestServer.getEchoHeaderURL("foo"));
        HttpURLConnection urlConnection =
                (HttpURLConnection) url.openConnection();
        urlConnection.addRequestProperty("foo", testInputString);
        assertEquals(200, urlConnection.getResponseCode());
        assertEquals("OK", urlConnection.getResponseMessage());
        InputStream in = urlConnection.getInputStream();
        byte[] actualOutput = new byte[testInputBytes.length];
        int bytesRead = in.read(actualOutput, 0, actualOutput.length);
        urlConnection.disconnect();
        assertEquals(testInputBytes.length, bytesRead);
        assertTrue(Arrays.equals(testInputBytes, actualOutput));
    }

    @Test
    @SmallTest
    @Feature({"Cronet"})
    @CompareDefaultWithCronet
    public void testInputStreamReadLessBytesThanAvailable() throws Exception {
        String testInputString = "this is a really long header";
        byte[] testInputBytes = testInputString.getBytes();
        URL url = new URL(NativeTestServer.getEchoHeaderURL("foo"));
        HttpURLConnection urlConnection =
                (HttpURLConnection) url.openConnection();
        urlConnection.addRequestProperty("foo", testInputString);
        assertEquals(200, urlConnection.getResponseCode());
        assertEquals("OK", urlConnection.getResponseMessage());
        InputStream in = urlConnection.getInputStream();
        byte[] firstPart = new byte[testInputBytes.length - 10];
        int firstBytesRead = in.read(firstPart, 0, testInputBytes.length - 10);
        byte[] secondPart = new byte[10];
        int secondBytesRead = in.read(secondPart, 0, 10);
        assertEquals(testInputBytes.length - 10, firstBytesRead);
        assertEquals(10, secondBytesRead);
        for (int i = 0; i < firstPart.length; i++) {
            assertEquals(testInputBytes[i], firstPart[i]);
        }
        for (int i = 0; i < secondPart.length; i++) {
            assertEquals(testInputBytes[firstPart.length + i], secondPart[i]);
        }
        urlConnection.disconnect();
    }

    /**
     * Makes sure that disconnect while reading from InputStream, the message
     * loop does not block. Regression test for crbug.com/550605.
     */
    @Test
    @SmallTest
    @Feature({"Cronet"})
    @CompareDefaultWithCronet
    public void testDisconnectWhileReadingDoesnotBlock() throws Exception {
        URL url = new URL(NativeTestServer.getEchoBodyURL());
        final HttpURLConnection connection = (HttpURLConnection) url.openConnection();
        // Make the server echo a large request body, so it exceeds the internal
        // read buffer.
        connection.setDoOutput(true);
        connection.setRequestMethod("POST");
        byte[] largeData = TestUtil.getLargeData();
        connection.setFixedLengthStreamingMode(largeData.length);
        OutputStream out = connection.getOutputStream();
        out.write(largeData);

        InputStream in = connection.getInputStream();
        // Read one byte and disconnect.
        assertTrue(in.read() != 1);
        connection.disconnect();
        // Continue reading, and make sure the message loop will not block.
        try {
            int b = 0;
            while (b != -1) {
                b = in.read();
            }
            // The response body is big, the connection should be disconnected
            // before EOF can be received.
            fail();
        } catch (IOException e) {
            // Expected.
            if (!mTestRule.testingSystemHttpURLConnection()) {
                assertEquals("disconnect() called", e.getMessage());
            }
        }
        // Read once more, and make sure exception is thrown.
        try {
            in.read();
            fail();
        } catch (IOException e) {
            // Expected.
            if (!mTestRule.testingSystemHttpURLConnection()) {
                assertEquals("disconnect() called", e.getMessage());
            }
        }
    }

    /**
     * Makes sure that {@link UrlRequest.Callback#onFailed} exception is
     * propagated when calling read on the input stream.
     */
    @Test
    @SmallTest
    @Feature({"Cronet"})
    @CompareDefaultWithCronet
    public void testServerHangsUp() throws Exception {
        URL url = new URL(NativeTestServer.getExabyteResponseURL());
        final HttpURLConnection connection = (HttpURLConnection) url.openConnection();
        InputStream in = connection.getInputStream();
        // Read one byte and shut down the server.
        assertTrue(in.read() != -1);
        NativeTestServer.shutdownNativeTestServer();
        // Continue reading, and make sure the message loop will not block.
        try {
            int b = 0;
            while (b != -1) {
                b = in.read();
            }
            // On KitKat, the default implementation doesn't throw an error.
            if (!mTestRule.testingSystemHttpURLConnection()) {
                // Server closes the connection before EOF can be received.
                fail();
            }
        } catch (IOException e) {
            // Expected.
            // Cronet gives a net::ERR_CONTENT_LENGTH_MISMATCH while the
            // default implementation sometimes gives a
            // java.net.ProtocolException with "unexpected end of stream"
            // message.
        }

        // Read once more, and make sure exception is thrown.
        try {
            in.read();
            // On KitKat, the default implementation doesn't throw an error.
            if (!mTestRule.testingSystemHttpURLConnection()) {
                fail();
            }
        } catch (IOException e) {
            // Expected.
            // Cronet gives a net::ERR_CONTENT_LENGTH_MISMATCH while the
            // default implementation sometimes gives a
            // java.net.ProtocolException with "unexpected end of stream"
            // message.
        }
        // Spins up server to avoid crash when shutting it down in tearDown().
        assertTrue(NativeTestServer.startNativeTestServer(getContext()));
    }

    @Test
    @SmallTest
    @Feature({"Cronet"})
    @CompareDefaultWithCronet
    public void testFollowRedirects() throws Exception {
        URL url = new URL(NativeTestServer.getFileURL("/redirect.html"));
        HttpURLConnection connection =
                (HttpURLConnection) url.openConnection();
        connection.setInstanceFollowRedirects(true);
        assertEquals(200, connection.getResponseCode());
        assertEquals("OK", connection.getResponseMessage());
        assertEquals(NativeTestServer.getFileURL("/success.txt"),
                connection.getURL().toString());
        assertEquals("this is a text file\n", TestUtil.getResponseAsString(connection));
        connection.disconnect();
    }

    @Test
    @SmallTest
    @Feature({"Cronet"})
    @CompareDefaultWithCronet
    public void testDisableRedirects() throws Exception {
        URL url = new URL(NativeTestServer.getFileURL("/redirect.html"));
        HttpURLConnection connection =
                (HttpURLConnection) url.openConnection();
        connection.setInstanceFollowRedirects(false);
        // Redirect following control broken in Android Marshmallow:
        // https://code.google.com/p/android/issues/detail?id=194495
        if (!mTestRule.testingSystemHttpURLConnection()
                || Build.VERSION.SDK_INT != Build.VERSION_CODES.M) {
            assertEquals(302, connection.getResponseCode());
            assertEquals("Found", connection.getResponseMessage());
            assertEquals("/success.txt", connection.getHeaderField("Location"));
            assertEquals(
                    NativeTestServer.getFileURL("/redirect.html"), connection.getURL().toString());
        }
        connection.disconnect();
    }

    @Test
    @SmallTest
    @Feature({"Cronet"})
    @CompareDefaultWithCronet
    public void testDisableRedirectsGlobal() throws Exception {
        HttpURLConnection.setFollowRedirects(false);
        URL url = new URL(NativeTestServer.getFileURL("/redirect.html"));
        HttpURLConnection connection =
                (HttpURLConnection) url.openConnection();
        // Redirect following control broken in Android Marshmallow:
        // https://code.google.com/p/android/issues/detail?id=194495
        if (!mTestRule.testingSystemHttpURLConnection()
                || Build.VERSION.SDK_INT != Build.VERSION_CODES.M) {
            assertEquals(302, connection.getResponseCode());
            assertEquals("Found", connection.getResponseMessage());
            assertEquals("/success.txt", connection.getHeaderField("Location"));
            assertEquals(
                    NativeTestServer.getFileURL("/redirect.html"), connection.getURL().toString());
        }
        connection.disconnect();
    }

    @Test
    @SmallTest
    @Feature({"Cronet"})
    @CompareDefaultWithCronet
    public void testDisableRedirectsGlobalAfterConnectionIsCreated() throws Exception {
        HttpURLConnection.setFollowRedirects(true);
        URL url = new URL(NativeTestServer.getFileURL("/redirect.html"));
        HttpURLConnection connection =
                (HttpURLConnection) url.openConnection();
        // Disabling redirects globally after creating the HttpURLConnection
        // object should have no effect on the request.
        HttpURLConnection.setFollowRedirects(false);
        assertEquals(200, connection.getResponseCode());
        assertEquals("OK", connection.getResponseMessage());
        assertEquals(NativeTestServer.getFileURL("/success.txt"),
                connection.getURL().toString());
        assertEquals("this is a text file\n", TestUtil.getResponseAsString(connection));
        connection.disconnect();
    }

    @Test
    @SmallTest
    @Feature({"Cronet"})
    @OnlyRunCronetHttpURLConnection
    // Cronet does not support reading response body of a 302 response.
    public void testDisableRedirectsTryReadBody() throws Exception {
        URL url = new URL(NativeTestServer.getFileURL("/redirect.html"));
        HttpURLConnection connection =
                (HttpURLConnection) url.openConnection();
        connection.setInstanceFollowRedirects(false);
        try {
            connection.getInputStream();
            fail();
        } catch (IOException e) {
            // Expected.
        }
        assertNull(connection.getErrorStream());
        connection.disconnect();
    }

    @Test
    @SmallTest
    @Feature({"Cronet"})
    @CompareDefaultWithCronet
    // Tests that redirects across the HTTP and HTTPS boundary are not followed.
    public void testDoNotFollowRedirectsIfSchemesDontMatch() throws Exception {
        URL url = new URL(NativeTestServer.getFileURL("/redirect_invalid_scheme.html"));
        HttpURLConnection connection = (HttpURLConnection) url.openConnection();
        connection.setInstanceFollowRedirects(true);
        assertEquals(302, connection.getResponseCode());
        assertEquals("Found", connection.getResponseMessage());
        // Behavior changed in Android Marshmallow to not update the URL.
        if (mTestRule.testingSystemHttpURLConnection()
                && Build.VERSION.SDK_INT >= Build.VERSION_CODES.M) {
            // Redirected port is randomized, verify everything but port.
            assertEquals(url.getProtocol(), connection.getURL().getProtocol());
            assertEquals(url.getHost(), connection.getURL().getHost());
            assertEquals(url.getFile(), connection.getURL().getFile());
        } else {
            // Redirect is not followed, but the url is updated to the Location header.
            assertEquals("https://127.0.0.1:8000/success.txt", connection.getURL().toString());
        }
        connection.disconnect();
    }

    @Test
    @SmallTest
    @Feature({"Cronet"})
    @CompareDefaultWithCronet
    public void testGetResponseHeadersAsMap() throws Exception {
        URL url = new URL(NativeTestServer.getFileURL("/success.txt"));
        HttpURLConnection connection =
                (HttpURLConnection) url.openConnection();
        Map<String, List<String>> responseHeaders =
                connection.getHeaderFields();
        // Make sure response header map is not modifiable.
        try {
            responseHeaders.put("foo", Arrays.asList("v1", "v2"));
            fail();
        } catch (UnsupportedOperationException e) {
            // Expected.
        }
        List<String> contentType = responseHeaders.get("Content-type");
        // Make sure map value is not modifiable as well.
        try {
            contentType.add("v3");
            fail();
        } catch (UnsupportedOperationException e) {
            // Expected.
        }
        // Make sure map look up is key insensitive.
        List<String> contentTypeWithOddCase =
                responseHeaders.get("ContENt-tYpe");
        assertEquals(contentType, contentTypeWithOddCase);

        assertEquals(1, contentType.size());
        assertEquals("text/plain", contentType.get(0));
        List<String> accessControl =
                responseHeaders.get("Access-Control-Allow-Origin");
        assertEquals(1, accessControl.size());
        assertEquals("*", accessControl.get(0));
        List<String> singleHeader = responseHeaders.get("header-name");
        assertEquals(1, singleHeader.size());
        assertEquals("header-value", singleHeader.get(0));
        List<String> multiHeader = responseHeaders.get("multi-header-name");
        assertEquals(2, multiHeader.size());
        assertEquals("header-value1", multiHeader.get(0));
        assertEquals("header-value2", multiHeader.get(1));
        connection.disconnect();
    }

    @Test
    @SmallTest
    @Feature({"Cronet"})
    @CompareDefaultWithCronet
    public void testGetResponseHeaderField() throws Exception {
        URL url = new URL(NativeTestServer.getFileURL("/success.txt"));
        HttpURLConnection connection =
                (HttpURLConnection) url.openConnection();
        assertEquals("text/plain", connection.getHeaderField("Content-Type"));
        assertEquals("*",
                connection.getHeaderField("Access-Control-Allow-Origin"));
        assertEquals("header-value", connection.getHeaderField("header-name"));
        // If there are multiple headers with the same name, the last should be
        // returned.
        assertEquals("header-value2",
                connection.getHeaderField("multi-header-name"));
        // Lastly, make sure lookup is case-insensitive.
        assertEquals("header-value2",
                connection.getHeaderField("MUlTi-heAder-name"));
        connection.disconnect();
    }

    @Test
    @SmallTest
    @Feature({"Cronet"})
    @CompareDefaultWithCronet
    public void testGetResponseHeaderFieldWithPos() throws Exception {
        URL url = new URL(NativeTestServer.getFileURL("/success.txt"));
        HttpURLConnection connection =
                (HttpURLConnection) url.openConnection();
        assertEquals("Content-Type", connection.getHeaderFieldKey(0));
        assertEquals("text/plain", connection.getHeaderField(0));
        assertEquals("Access-Control-Allow-Origin",
                connection.getHeaderFieldKey(1));
        assertEquals("*", connection.getHeaderField(1));
        assertEquals("header-name", connection.getHeaderFieldKey(2));
        assertEquals("header-value", connection.getHeaderField(2));
        assertEquals("multi-header-name", connection.getHeaderFieldKey(3));
        assertEquals("header-value1", connection.getHeaderField(3));
        assertEquals("multi-header-name", connection.getHeaderFieldKey(4));
        assertEquals("header-value2", connection.getHeaderField(4));
        // Note that the default implementation also adds additional response
        // headers, which are not tested here.
        connection.disconnect();
    }

    @Test
    @SmallTest
    @Feature({"Cronet"})
    @OnlyRunCronetHttpURLConnection
    // The default implementation adds additional response headers, so this test
    // only tests Cronet's implementation.
    public void testGetResponseHeaderFieldWithPosExceed() throws Exception {
        URL url = new URL(NativeTestServer.getFileURL("/success.txt"));
        HttpURLConnection connection =
                (HttpURLConnection) url.openConnection();
        // Expect null if we exceed the number of header entries.
        assertEquals(null, connection.getHeaderFieldKey(5));
        assertEquals(null, connection.getHeaderField(5));
        assertEquals(null, connection.getHeaderFieldKey(6));
        assertEquals(null, connection.getHeaderField(6));
        connection.disconnect();
    }

    @Test
    @SmallTest
    @Feature({"Cronet"})
    @OnlyRunCronetHttpURLConnection
    // Test that Cronet strips content-encoding header.
    public void testStripContentEncoding() throws Exception {
        URL url = new URL(NativeTestServer.getFileURL("/gzipped.html"));
        HttpURLConnection connection = (HttpURLConnection) url.openConnection();
        assertEquals("foo", connection.getHeaderFieldKey(0));
        assertEquals("bar", connection.getHeaderField(0));
        assertEquals(null, connection.getHeaderField("content-encoding"));
        Map<String, List<String>> responseHeaders = connection.getHeaderFields();
        assertEquals(1, responseHeaders.size());
        assertEquals(200, connection.getResponseCode());
        assertEquals("OK", connection.getResponseMessage());
        // Make sure Cronet decodes the gzipped content.
        assertEquals("Hello, World!", TestUtil.getResponseAsString(connection));
        connection.disconnect();
    }

    private static enum CacheSetting { USE_CACHE, DONT_USE_CACHE };

    private static enum ExpectedOutcome { SUCCESS, FAILURE };

    /**
     * Helper method to make a request with cache enabled or disabled, and check
     * whether the request is successful.
     * @param requestUrl request url.
     * @param cacheSetting indicates cache should be used.
     * @param outcome indicates request is expected to be successful.
     */
    private void checkRequestCaching(String requestUrl,
            CacheSetting cacheSetting,
            ExpectedOutcome outcome) throws Exception {
        URL url = new URL(requestUrl);
        HttpURLConnection connection =
                (HttpURLConnection) url.openConnection();
        connection.setUseCaches(cacheSetting == CacheSetting.USE_CACHE);
        if (outcome == ExpectedOutcome.SUCCESS) {
            assertEquals(200, connection.getResponseCode());
            assertEquals("this is a cacheable file\n", TestUtil.getResponseAsString(connection));
        } else {
            try {
                connection.getResponseCode();
                fail();
            } catch (IOException e) {
                // Expected.
            }
        }
        connection.disconnect();
    }

    @Test
    @SmallTest
    @Feature({"Cronet"})
    @OnlyRunCronetHttpURLConnection
    // Strangely, the default implementation fails to return a cached response.
    // If the server is shut down, the request just fails with a connection
    // refused error. Therefore, this test and the next only runs Cronet.
    public void testSetUseCaches() throws Exception {
        String url = NativeTestServer.getFileURL("/cacheable.txt");
        checkRequestCaching(url,
                CacheSetting.USE_CACHE, ExpectedOutcome.SUCCESS);
        // Shut down the server, we should be able to receive a cached response.
        NativeTestServer.shutdownNativeTestServer();
        checkRequestCaching(url,
                CacheSetting.USE_CACHE, ExpectedOutcome.SUCCESS);
    }

    @Test
    @SmallTest
    @Feature({"Cronet"})
    @OnlyRunCronetHttpURLConnection
    public void testSetUseCachesFalse() throws Exception {
        String url = NativeTestServer.getFileURL("/cacheable.txt");
        checkRequestCaching(url,
                CacheSetting.USE_CACHE, ExpectedOutcome.SUCCESS);
        NativeTestServer.shutdownNativeTestServer();
        // Disables caching. No cached response is received.
        checkRequestCaching(url,
                CacheSetting.DONT_USE_CACHE, ExpectedOutcome.FAILURE);
    }

    @Test
    @SmallTest
    @Feature({"Cronet"})
    @OnlyRunCronetHttpURLConnection
    // Tests that if disconnect() is called on a different thread when
    // getResponseCode() is still waiting for response, there is no
    // NPE but only IOException.
    // Regression test for crbug.com/751786
    public void testDisconnectWhenGetResponseCodeIsWaiting() throws Exception {
        ServerSocket hangingServer = new ServerSocket(0);
        URL url = new URL("http://localhost:" + hangingServer.getLocalPort());
        final HttpURLConnection connection = (HttpURLConnection) url.openConnection();
        // connect() is non-blocking. This is to make sure disconnect() triggers cancellation.
        connection.connect();
        FutureTask<IOException> task = new FutureTask<IOException>(new Callable<IOException>() {
            @Override
            public IOException call() {
                try {
                    connection.getResponseCode();
                } catch (IOException e) {
                    return e;
                }
                return null;
            }
        });
        new Thread(task).start();
        Socket s = hangingServer.accept();
        connection.disconnect();
        IOException e = task.get();
        assertEquals("disconnect() called", e.getMessage());
        s.close();
        hangingServer.close();
    }

    private void checkExceptionsAreThrown(HttpURLConnection connection)
            throws Exception {
        try {
            connection.getInputStream();
            fail();
        } catch (IOException e) {
            // Expected.
        }

        try {
            connection.getResponseCode();
            fail();
        } catch (IOException e) {
            // Expected.
        }

        try {
            connection.getResponseMessage();
            fail();
        } catch (IOException e) {
            // Expected.
        }

        // Default implementation of getHeaderFields() returns null on K, but
        // returns an empty map on L.
        if (Build.VERSION.SDK_INT >= Build.VERSION_CODES.LOLLIPOP) {
            Map<String, List<String>> headers = connection.getHeaderFields();
            assertNotNull(headers);
            assertTrue(headers.isEmpty());
        }
        // Skip getHeaderFields(), since it can return null or an empty map.
        assertNull(connection.getHeaderField("foo"));
        assertNull(connection.getHeaderFieldKey(0));
        assertNull(connection.getHeaderField(0));

        // getErrorStream() does not have a throw clause, it returns null if
        // there's an exception.
        InputStream errorStream = connection.getErrorStream();
        assertNull(errorStream);
    }

    /**
     * Helper method to extract a list of header values with the give header
     * name.
     */
    private List<String> getRequestHeaderValues(String allHeaders,
            String headerName) {
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
    @Feature({"Cronet"})
    @RequiresMinApi(9) // Tagging support added in API level 9: crrev.com/c/chromium/src/+/930086
    public void testTagging() throws Exception {
        if (!CronetTestUtil.nativeCanGetTaggedBytes()) {
            Log.i(TAG, "Skipping test - GetTaggedBytes unsupported.");
            return;
        }
        URL url = new URL(NativeTestServer.getEchoMethodURL());

        // Test untagged requests are given tag 0.
        int tag = 0;
        long priorBytes = CronetTestUtil.nativeGetTaggedBytes(tag);
        CronetHttpURLConnection urlConnection = (CronetHttpURLConnection) url.openConnection();
        assertEquals(200, urlConnection.getResponseCode());
        urlConnection.disconnect();
        assertTrue(CronetTestUtil.nativeGetTaggedBytes(tag) > priorBytes);

        // Test explicit tagging.
        tag = 0x12345678;
        priorBytes = CronetTestUtil.nativeGetTaggedBytes(tag);
        urlConnection = (CronetHttpURLConnection) url.openConnection();
        urlConnection.setTrafficStatsTag(tag);
        assertEquals(200, urlConnection.getResponseCode());
        urlConnection.disconnect();
        assertTrue(CronetTestUtil.nativeGetTaggedBytes(tag) > priorBytes);

        // Test a different tag value.
        tag = 0x87654321;
        priorBytes = CronetTestUtil.nativeGetTaggedBytes(tag);
        urlConnection = (CronetHttpURLConnection) url.openConnection();
        urlConnection.setTrafficStatsTag(tag);
        assertEquals(200, urlConnection.getResponseCode());
        urlConnection.disconnect();
        assertTrue(CronetTestUtil.nativeGetTaggedBytes(tag) > priorBytes);

        // Test tagging with our UID.
        // NOTE(pauljensen): Explicitly setting the UID to the current UID isn't a particularly
        // thorough test of this API but at least provides coverage of the underlying code, and
        // verifies that traffic is still properly attributed.
        // The code path for UID is parallel to that for the tag, which we do have more thorough
        // testing for.  More thorough testing of setting the UID would require running tests with
        // a rare permission which isn't realistic for most apps.  Apps are allowed to set the UID
        // to their own UID as per this logic in the tagging kernel module:
        // https://android.googlesource.com/kernel/common/+/21dd5d7/net/netfilter/xt_qtaguid.c#154
        tag = 0;
        priorBytes = CronetTestUtil.nativeGetTaggedBytes(tag);
        urlConnection = (CronetHttpURLConnection) url.openConnection();
        urlConnection.setTrafficStatsUid(Process.myUid());
        assertEquals(200, urlConnection.getResponseCode());
        urlConnection.disconnect();
        assertTrue(CronetTestUtil.nativeGetTaggedBytes(tag) > priorBytes);
    }

    @Test
    @SmallTest
    @Feature({"Cronet"})
    @CompareDefaultWithCronet
    public void testIOExceptionErrorRethrown() throws Exception {
        // URL that should fail to connect.
        URL url = new URL("http://localhost");
        HttpURLConnection connection = (HttpURLConnection) url.openConnection();
        // This should not throw, even though internally it may encounter an exception.
        connection.getHeaderField("blah");
        try {
            // This should throw an IOException.
            connection.getResponseCode();
            fail();
        } catch (IOException e) {
            // Expected
        }
        connection.disconnect();
    }

    @Test
    @SmallTest
    @Feature({"Cronet"})
    @OnlyRunCronetHttpURLConnection // System impl flakily responds to interrupt.
    public void testIOExceptionInterruptRethrown() throws Exception {
        ServerSocket hangingServer = new ServerSocket(0);
        URL url = new URL("http://localhost:" + hangingServer.getLocalPort());
        final HttpURLConnection connection = (HttpURLConnection) url.openConnection();
        // connect() is non-blocking.
        connection.connect();
        FutureTask<IOException> task = new FutureTask<IOException>(new Callable<IOException>() {
            @Override
            public IOException call() {
                // This should not throw, even though internally it may encounter an exception.
                connection.getHeaderField("blah");
                try {
                    // This should throw an InterruptedIOException.
                    connection.getResponseCode();
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
        assertNotNull(task.get());
        s.close();
        connection.disconnect();
    }

    @Test
    @SmallTest
    @Feature({"Cronet"})
    @OnlyRunCronetHttpURLConnection // Not interested in system crashes.
    // Regression test for crashes in disconnect() impl.
    public void testCancelRace() throws Exception {
        URL url = new URL(NativeTestServer.getEchoMethodURL());
        final HttpURLConnection urlConnection = (HttpURLConnection) url.openConnection();
        final AtomicBoolean connected = new AtomicBoolean();
        // Start request on another thread.
        new Thread(new Runnable() {
            @Override
            public void run() {
                try {
                    assertEquals(200, urlConnection.getResponseCode());
                } catch (IOException e) {
                }
                connected.set(true);
            }
        })
                .start();
        // Repeatedly call disconnect().  This used to crash.
        do {
            urlConnection.disconnect();
        } while (!connected.get());
    }
}
