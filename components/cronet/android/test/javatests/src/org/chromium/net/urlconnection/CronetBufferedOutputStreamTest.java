// Copyright 2015 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.net.urlconnection;

import static com.google.common.truth.Truth.assertThat;

import static org.junit.Assert.assertTrue;
import static org.junit.Assert.fail;

import static org.chromium.net.CronetTestRule.getContext;

import androidx.test.ext.junit.runners.AndroidJUnit4;
import androidx.test.filters.SmallTest;

import org.junit.After;
import org.junit.Before;
import org.junit.Rule;
import org.junit.Test;
import org.junit.runner.RunWith;

import org.chromium.base.test.util.DoNotBatch;
import org.chromium.net.CronetTestRule;
import org.chromium.net.CronetTestRule.CompareDefaultWithCronet;
import org.chromium.net.CronetTestRule.CronetTestFramework;
import org.chromium.net.CronetTestRule.OnlyRunCronetHttpURLConnection;
import org.chromium.net.NativeTestServer;

import java.io.IOException;
import java.io.OutputStream;
import java.net.HttpURLConnection;
import java.net.URL;

/**
 * Tests the CronetBufferedOutputStream implementation.
 */
@DoNotBatch(
        reason = "URL#setURLStreamHandlerFactory can be called at most once during JVM lifetime")
@RunWith(AndroidJUnit4.class)
public class CronetBufferedOutputStreamTest {
    @Rule
    public final CronetTestRule mTestRule = new CronetTestRule();

    private CronetTestFramework mTestFramework;
    private HttpURLConnection mConnection;

    @Before
    public void setUp() throws Exception {
        mTestFramework = mTestRule.startCronetTestFramework();
        mTestRule.setStreamHandlerFactory(mTestFramework.mCronetEngine);
        assertTrue(NativeTestServer.startNativeTestServer(getContext()));
    }

    @After
    public void tearDown() throws Exception {
        if (mConnection != null) {
            mConnection.disconnect();
        }
        mTestFramework.shutdownEngine();
        NativeTestServer.shutdownNativeTestServer();
    }

    @Test
    @SmallTest
    @CompareDefaultWithCronet
    public void testGetOutputStreamAfterConnectionMade() throws Exception {
        URL url = new URL(NativeTestServer.getEchoBodyURL());
        mConnection = (HttpURLConnection) url.openConnection();
        mConnection.setDoOutput(true);
        mConnection.setRequestMethod("POST");
        assertThat(mConnection.getResponseCode()).isEqualTo(200);
        try {
            mConnection.getOutputStream();
            fail();
        } catch (java.net.ProtocolException e) {
            // Expected.
        }
    }

    /**
     * Tests write after connect. Strangely, the default implementation allows
     * writing after being connected, so this test only runs against Cronet's
     * implementation.
     */
    @Test
    @SmallTest
    @OnlyRunCronetHttpURLConnection
    public void testWriteAfterConnect() throws Exception {
        URL url = new URL(NativeTestServer.getEchoBodyURL());
        mConnection = (HttpURLConnection) url.openConnection();
        mConnection.setDoOutput(true);
        mConnection.setRequestMethod("POST");
        OutputStream out = mConnection.getOutputStream();
        out.write(TestUtil.UPLOAD_DATA);
        mConnection.connect();
        try {
            // Attemp to write some more.
            out.write(TestUtil.UPLOAD_DATA);
            fail();
        } catch (IllegalStateException e) {
            assertThat(e).hasMessageThat().isEqualTo("Use setFixedLengthStreamingMode() or "
                    + "setChunkedStreamingMode() for writing after connect");
        }
    }

    @Test
    @SmallTest
    @CompareDefaultWithCronet
    public void testWriteAfterReadingResponse() throws Exception {
        URL url = new URL(NativeTestServer.getEchoBodyURL());
        mConnection = (HttpURLConnection) url.openConnection();
        mConnection.setDoOutput(true);
        mConnection.setRequestMethod("POST");
        OutputStream out = mConnection.getOutputStream();
        assertThat(mConnection.getResponseCode()).isEqualTo(200);
        try {
            out.write(TestUtil.UPLOAD_DATA);
            fail();
        } catch (Exception e) {
            // Default implementation gives an IOException and says that the
            // stream is closed. Cronet gives an IllegalStateException and
            // complains about write after connected.
        }
    }

    @Test
    @SmallTest
    @CompareDefaultWithCronet
    public void testPostWithContentLength() throws Exception {
        URL url = new URL(NativeTestServer.getEchoBodyURL());
        mConnection = (HttpURLConnection) url.openConnection();
        mConnection.setDoOutput(true);
        mConnection.setRequestMethod("POST");
        byte[] largeData = TestUtil.getLargeData();
        mConnection.setRequestProperty("Content-Length", Integer.toString(largeData.length));
        OutputStream out = mConnection.getOutputStream();
        int totalBytesWritten = 0;
        // Number of bytes to write each time. It is doubled each time
        // to make sure that the buffer grows.
        int bytesToWrite = 683;
        while (totalBytesWritten < largeData.length) {
            if (bytesToWrite > largeData.length - totalBytesWritten) {
                // Do not write out of bound.
                bytesToWrite = largeData.length - totalBytesWritten;
            }
            out.write(largeData, totalBytesWritten, bytesToWrite);
            totalBytesWritten += bytesToWrite;
            bytesToWrite *= 2;
        }
        assertThat(mConnection.getResponseCode()).isEqualTo(200);
        assertThat(mConnection.getResponseMessage()).isEqualTo("OK");
        TestUtil.checkLargeData(TestUtil.getResponseAsString(mConnection));
    }

    @Test
    @SmallTest
    @CompareDefaultWithCronet
    public void testPostWithContentLengthOneMassiveWrite() throws Exception {
        URL url = new URL(NativeTestServer.getEchoBodyURL());
        mConnection = (HttpURLConnection) url.openConnection();
        mConnection.setDoOutput(true);
        mConnection.setRequestMethod("POST");
        byte[] largeData = TestUtil.getLargeData();
        mConnection.setRequestProperty("Content-Length", Integer.toString(largeData.length));
        OutputStream out = mConnection.getOutputStream();
        out.write(largeData);
        assertThat(mConnection.getResponseCode()).isEqualTo(200);
        assertThat(mConnection.getResponseMessage()).isEqualTo("OK");
        TestUtil.checkLargeData(TestUtil.getResponseAsString(mConnection));
    }

    @Test
    @SmallTest
    @CompareDefaultWithCronet
    public void testPostWithContentLengthWriteOneByte() throws Exception {
        URL url = new URL(NativeTestServer.getEchoBodyURL());
        mConnection = (HttpURLConnection) url.openConnection();
        mConnection.setDoOutput(true);
        mConnection.setRequestMethod("POST");
        byte[] largeData = TestUtil.getLargeData();
        mConnection.setRequestProperty("Content-Length", Integer.toString(largeData.length));
        OutputStream out = mConnection.getOutputStream();
        for (int i = 0; i < largeData.length; i++) {
            out.write(largeData[i]);
        }
        assertThat(mConnection.getResponseCode()).isEqualTo(200);
        assertThat(mConnection.getResponseMessage()).isEqualTo("OK");
        TestUtil.checkLargeData(TestUtil.getResponseAsString(mConnection));
    }

    @Test
    @SmallTest
    @CompareDefaultWithCronet
    public void testPostWithZeroContentLength() throws Exception {
        URL url = new URL(NativeTestServer.getEchoBodyURL());
        mConnection = (HttpURLConnection) url.openConnection();
        mConnection.setDoOutput(true);
        mConnection.setRequestMethod("POST");
        mConnection.setRequestProperty("Content-Length", "0");
        assertThat(mConnection.getResponseCode()).isEqualTo(200);
        assertThat(mConnection.getResponseMessage()).isEqualTo("OK");
        assertThat(TestUtil.getResponseAsString(mConnection)).isEmpty();
    }

    @Test
    @SmallTest
    @CompareDefaultWithCronet
    public void testPostZeroByteWithoutContentLength() throws Exception {
        // Make sure both implementation sets the Content-Length header to 0.
        URL url = new URL(NativeTestServer.getEchoHeaderURL("Content-Length"));
        mConnection = (HttpURLConnection) url.openConnection();
        mConnection.setDoOutput(true);
        mConnection.setRequestMethod("POST");
        assertThat(mConnection.getResponseCode()).isEqualTo(200);
        assertThat(mConnection.getResponseMessage()).isEqualTo("OK");
        assertThat(TestUtil.getResponseAsString(mConnection)).isEqualTo("0");
        mConnection.disconnect();

        // Make sure the server echoes back empty body for both implementation.
        URL echoBody = new URL(NativeTestServer.getEchoBodyURL());
        mConnection = (HttpURLConnection) echoBody.openConnection();
        mConnection.setDoOutput(true);
        mConnection.setRequestMethod("POST");
        assertThat(mConnection.getResponseCode()).isEqualTo(200);
        assertThat(mConnection.getResponseMessage()).isEqualTo("OK");
        assertThat(TestUtil.getResponseAsString(mConnection)).isEmpty();
    }

    @Test
    @SmallTest
    @CompareDefaultWithCronet
    public void testPostWithoutContentLengthSmall() throws Exception {
        URL url = new URL(NativeTestServer.getEchoBodyURL());
        mConnection = (HttpURLConnection) url.openConnection();
        mConnection.setDoOutput(true);
        mConnection.setRequestMethod("POST");
        OutputStream out = mConnection.getOutputStream();
        out.write(TestUtil.UPLOAD_DATA);
        assertThat(mConnection.getResponseCode()).isEqualTo(200);
        assertThat(mConnection.getResponseMessage()).isEqualTo("OK");
        assertThat(TestUtil.getResponseAsString(mConnection))
                .isEqualTo(TestUtil.UPLOAD_DATA_STRING);
    }

    @Test
    @SmallTest
    @CompareDefaultWithCronet
    public void testPostWithoutContentLength() throws Exception {
        URL url = new URL(NativeTestServer.getEchoBodyURL());
        mConnection = (HttpURLConnection) url.openConnection();
        mConnection.setDoOutput(true);
        mConnection.setRequestMethod("POST");
        byte[] largeData = TestUtil.getLargeData();
        OutputStream out = mConnection.getOutputStream();
        int totalBytesWritten = 0;
        // Number of bytes to write each time. It is doubled each time
        // to make sure that the buffer grows.
        int bytesToWrite = 683;
        while (totalBytesWritten < largeData.length) {
            if (bytesToWrite > largeData.length - totalBytesWritten) {
                // Do not write out of bound.
                bytesToWrite = largeData.length - totalBytesWritten;
            }
            out.write(largeData, totalBytesWritten, bytesToWrite);
            totalBytesWritten += bytesToWrite;
            bytesToWrite *= 2;
        }
        assertThat(mConnection.getResponseCode()).isEqualTo(200);
        assertThat(mConnection.getResponseMessage()).isEqualTo("OK");
        TestUtil.checkLargeData(TestUtil.getResponseAsString(mConnection));
    }

    @Test
    @SmallTest
    @CompareDefaultWithCronet
    public void testPostWithoutContentLengthOneMassiveWrite() throws Exception {
        URL url = new URL(NativeTestServer.getEchoBodyURL());
        mConnection = (HttpURLConnection) url.openConnection();
        mConnection.setDoOutput(true);
        mConnection.setRequestMethod("POST");
        OutputStream out = mConnection.getOutputStream();
        byte[] largeData = TestUtil.getLargeData();
        out.write(largeData);
        assertThat(mConnection.getResponseCode()).isEqualTo(200);
        assertThat(mConnection.getResponseMessage()).isEqualTo("OK");
        TestUtil.checkLargeData(TestUtil.getResponseAsString(mConnection));
    }

    @Test
    @SmallTest
    @CompareDefaultWithCronet
    public void testPostWithoutContentLengthWriteOneByte() throws Exception {
        URL url = new URL(NativeTestServer.getEchoBodyURL());
        mConnection = (HttpURLConnection) url.openConnection();
        mConnection.setDoOutput(true);
        mConnection.setRequestMethod("POST");
        OutputStream out = mConnection.getOutputStream();
        byte[] largeData = TestUtil.getLargeData();
        for (int i = 0; i < largeData.length; i++) {
            out.write(largeData[i]);
        }
        assertThat(mConnection.getResponseCode()).isEqualTo(200);
        assertThat(mConnection.getResponseMessage()).isEqualTo("OK");
        TestUtil.checkLargeData(TestUtil.getResponseAsString(mConnection));
    }

    @Test
    @SmallTest
    @CompareDefaultWithCronet
    public void testWriteLessThanContentLength() throws Exception {
        URL url = new URL(NativeTestServer.getEchoBodyURL());
        mConnection = (HttpURLConnection) url.openConnection();
        mConnection.setDoOutput(true);
        mConnection.setRequestMethod("POST");
        // Set a content length that's 1 byte more.
        mConnection.setRequestProperty(
                "Content-Length", Integer.toString(TestUtil.UPLOAD_DATA.length + 1));
        OutputStream out = mConnection.getOutputStream();
        out.write(TestUtil.UPLOAD_DATA);
        try {
            mConnection.getResponseCode();
            fail();
        } catch (IOException e) {
            // Expected.
        }
    }

    /**
     * Tests that if caller writes more than the content length provided,
     * an exception should occur.
     */
    @Test
    @SmallTest
    @CompareDefaultWithCronet
    public void testWriteMoreThanContentLength() throws Exception {
        URL url = new URL(NativeTestServer.getEchoBodyURL());
        mConnection = (HttpURLConnection) url.openConnection();
        mConnection.setDoOutput(true);
        mConnection.setRequestMethod("POST");
        // Use a content length that is 1 byte shorter than actual data.
        mConnection.setRequestProperty(
                "Content-Length", Integer.toString(TestUtil.UPLOAD_DATA.length - 1));
        OutputStream out = mConnection.getOutputStream();
        // Write a few bytes first.
        out.write(TestUtil.UPLOAD_DATA, 0, 3);
        try {
            // Write remaining bytes.
            out.write(TestUtil.UPLOAD_DATA, 3, TestUtil.UPLOAD_DATA.length - 3);
            // On Lollipop, default implementation only triggers the error when reading response.
            mConnection.getInputStream();
            fail();
        } catch (IOException e) {
            assertThat(e).hasMessageThat().isEqualTo("exceeded content-length limit of "
                    + (TestUtil.UPLOAD_DATA.length - 1) + " bytes");
        }
    }

    /**
     * Same as {@code testWriteMoreThanContentLength()}, but it only writes one byte
     * at a time.
     */
    @Test
    @SmallTest
    @CompareDefaultWithCronet
    public void testWriteMoreThanContentLengthWriteOneByte() throws Exception {
        URL url = new URL(NativeTestServer.getEchoBodyURL());
        mConnection = (HttpURLConnection) url.openConnection();
        mConnection.setDoOutput(true);
        mConnection.setRequestMethod("POST");
        // Use a content length that is 1 byte shorter than actual data.
        mConnection.setRequestProperty(
                "Content-Length", Integer.toString(TestUtil.UPLOAD_DATA.length - 1));
        OutputStream out = mConnection.getOutputStream();
        try {
            for (int i = 0; i < TestUtil.UPLOAD_DATA.length; i++) {
                out.write(TestUtil.UPLOAD_DATA[i]);
            }
            // On Lollipop, default implementation only triggers the error when reading response.
            mConnection.getInputStream();
            fail();
        } catch (IOException e) {
            assertThat(e).hasMessageThat().isEqualTo("exceeded content-length limit of "
                    + (TestUtil.UPLOAD_DATA.length - 1) + " bytes");
        }
    }

    /**
     * Tests that {@link CronetBufferedOutputStream} supports rewind in a
     * POST preserving redirect.
     * Use {@code OnlyRunCronetHttpURLConnection} as the default implementation
     * does not pass this test.
     */
    @Test
    @SmallTest
    @OnlyRunCronetHttpURLConnection
    public void testRewind() throws Exception {
        URL url = new URL(NativeTestServer.getRedirectToEchoBody());
        mConnection = (HttpURLConnection) url.openConnection();
        mConnection.setDoOutput(true);
        mConnection.setRequestMethod("POST");
        mConnection.setRequestProperty(
                "Content-Length", Integer.toString(TestUtil.UPLOAD_DATA.length));
        OutputStream out = mConnection.getOutputStream();
        out.write(TestUtil.UPLOAD_DATA);
        assertThat(TestUtil.getResponseAsString(mConnection))
                .isEqualTo(TestUtil.UPLOAD_DATA_STRING);
    }

    /**
     * Like {@link #testRewind} but does not set Content-Length header.
     */
    @Test
    @SmallTest
    @OnlyRunCronetHttpURLConnection
    public void testRewindWithoutContentLength() throws Exception {
        URL url = new URL(NativeTestServer.getRedirectToEchoBody());
        mConnection = (HttpURLConnection) url.openConnection();
        mConnection.setDoOutput(true);
        mConnection.setRequestMethod("POST");
        OutputStream out = mConnection.getOutputStream();
        out.write(TestUtil.UPLOAD_DATA);
        assertThat(TestUtil.getResponseAsString(mConnection))
                .isEqualTo(TestUtil.UPLOAD_DATA_STRING);
    }
}
