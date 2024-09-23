// Copyright 2015 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.net.urlconnection;

import static com.google.common.truth.Truth.assertThat;

import static org.junit.Assert.assertThrows;

import androidx.test.ext.junit.runners.AndroidJUnit4;
import androidx.test.filters.SmallTest;

import org.junit.After;
import org.junit.Before;
import org.junit.Rule;
import org.junit.Test;
import org.junit.runner.RunWith;

import org.chromium.base.test.util.DoNotBatch;
import org.chromium.base.test.util.RequiresRestart;
import org.chromium.net.CronetEngine;
import org.chromium.net.CronetTestRule;
import org.chromium.net.CronetTestRule.CronetImplementation;
import org.chromium.net.CronetTestRule.IgnoreFor;
import org.chromium.net.NativeTestServer;

import java.io.IOException;
import java.io.OutputStream;
import java.net.HttpURLConnection;
import java.net.ProtocolException;
import java.net.URL;

/** Tests the CronetBufferedOutputStream implementation. */
@DoNotBatch(reason = "crbug/1459563")
@IgnoreFor(
        implementations = {CronetImplementation.FALLBACK},
        reason = "See crrev.com/c/4590329")
@RunWith(AndroidJUnit4.class)
public class CronetBufferedOutputStreamTest {
    @Rule public final CronetTestRule mTestRule = CronetTestRule.withAutomaticEngineStartup();

    private HttpURLConnection mConnection;

    private CronetEngine mCronetEngine;

    @Before
    public void setUp() throws Exception {
        mCronetEngine = mTestRule.getTestFramework().getEngine();
        assertThat(
                        NativeTestServer.startNativeTestServer(
                                mTestRule.getTestFramework().getContext()))
                .isTrue();
    }

    @After
    public void tearDown() throws Exception {
        if (mConnection != null) {
            mConnection.disconnect();
        }
        NativeTestServer.shutdownNativeTestServer();
    }

    @Test
    @SmallTest
    public void testGetOutputStreamAfterConnectionMade() throws Exception {
        URL url = new URL(NativeTestServer.getEchoBodyURL());
        mConnection = (HttpURLConnection) mCronetEngine.openConnection(url);
        mConnection.setDoOutput(true);
        mConnection.setRequestMethod("POST");
        assertThat(mConnection.getResponseCode()).isEqualTo(200);
        assertThrows(ProtocolException.class, mConnection::getOutputStream);
    }

    // Tests the case where we don't specify any streaming mode, call connect(), and *then* start
    // writing to the output stream. It's not entirely clear from HttpURLConnection docs if this is
    // supposed to work, but the default Android implementation does support this pattern, so for
    // compatibility we should too. See http://crbug.com/348166397.
    @Test
    @SmallTest
    @IgnoreFor(
            implementations = {CronetImplementation.AOSP_PLATFORM},
            reason = "New behavior that has not made it to HttpEngine yet")
    public void testWriteAfterConnect() throws Exception {
        URL url = new URL(NativeTestServer.getEchoBodyURL());
        mConnection = (HttpURLConnection) mCronetEngine.openConnection(url);
        mConnection.setDoOutput(true);
        mConnection.setRequestMethod("POST");
        mConnection.connect();

        String dataString = "some very important data";
        mConnection.getOutputStream().write(dataString.getBytes());
        assertThat(mConnection.getResponseCode()).isEqualTo(200);
        assertThat(mConnection.getResponseMessage()).isEqualTo("OK");
        assertThat(TestUtil.getResponseAsString(mConnection)).isEqualTo(dataString);
    }

    @Test
    @SmallTest
    public void testWriteAfterReadingResponse() throws Exception {
        URL url = new URL(NativeTestServer.getEchoBodyURL());
        mConnection = (HttpURLConnection) mCronetEngine.openConnection(url);
        mConnection.setDoOutput(true);
        mConnection.setRequestMethod("POST");
        OutputStream out = mConnection.getOutputStream();
        assertThat(mConnection.getResponseCode()).isEqualTo(200);
        assertThrows(Exception.class, () -> out.write(TestUtil.UPLOAD_DATA));
    }

    @Test
    @SmallTest
    public void testPostWithContentLength() throws Exception {
        URL url = new URL(NativeTestServer.getEchoBodyURL());
        mConnection = (HttpURLConnection) mCronetEngine.openConnection(url);
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
    public void testPostWithContentLengthOneMassiveWrite() throws Exception {
        URL url = new URL(NativeTestServer.getEchoBodyURL());
        mConnection = (HttpURLConnection) mCronetEngine.openConnection(url);
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
    @RequiresRestart("crbug.com/344966615")
    public void testPostWithContentLengthWriteOneByte() throws Exception {
        URL url = new URL(NativeTestServer.getEchoBodyURL());
        mConnection = (HttpURLConnection) mCronetEngine.openConnection(url);
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
    public void testPostWithZeroContentLength() throws Exception {
        URL url = new URL(NativeTestServer.getEchoBodyURL());
        mConnection = (HttpURLConnection) mCronetEngine.openConnection(url);
        mConnection.setDoOutput(true);
        mConnection.setRequestMethod("POST");
        mConnection.setRequestProperty("Content-Length", "0");
        assertThat(mConnection.getResponseCode()).isEqualTo(200);
        assertThat(mConnection.getResponseMessage()).isEqualTo("OK");
        assertThat(TestUtil.getResponseAsString(mConnection)).isEmpty();
    }

    @Test
    @SmallTest
    public void testPostZeroByteWithoutContentLength() throws Exception {
        // Make sure both implementation sets the Content-Length header to 0.
        URL url = new URL(NativeTestServer.getEchoHeaderURL("Content-Length"));
        mConnection = (HttpURLConnection) mCronetEngine.openConnection(url);
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
    public void testPostWithoutContentLengthSmall() throws Exception {
        URL url = new URL(NativeTestServer.getEchoBodyURL());
        mConnection = (HttpURLConnection) mCronetEngine.openConnection(url);
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
    public void testPostWithoutContentLength() throws Exception {
        URL url = new URL(NativeTestServer.getEchoBodyURL());
        mConnection = (HttpURLConnection) mCronetEngine.openConnection(url);
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
    public void testPostWithoutContentLengthOneMassiveWrite() throws Exception {
        URL url = new URL(NativeTestServer.getEchoBodyURL());
        mConnection = (HttpURLConnection) mCronetEngine.openConnection(url);
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
    public void testPostWithoutContentLengthWriteOneByte() throws Exception {
        URL url = new URL(NativeTestServer.getEchoBodyURL());
        mConnection = (HttpURLConnection) mCronetEngine.openConnection(url);
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
    public void testWriteLessThanContentLength() throws Exception {
        URL url = new URL(NativeTestServer.getEchoBodyURL());
        mConnection = (HttpURLConnection) mCronetEngine.openConnection(url);
        mConnection.setDoOutput(true);
        mConnection.setRequestMethod("POST");
        // Set a content length that's 1 byte more.
        mConnection.setRequestProperty(
                "Content-Length", Integer.toString(TestUtil.UPLOAD_DATA.length + 1));
        OutputStream out = mConnection.getOutputStream();
        out.write(TestUtil.UPLOAD_DATA);
        assertThrows(IOException.class, mConnection::getResponseCode);
    }

    /** Tests that if caller writes more than the content length provided, an exception should occur. */
    @Test
    @SmallTest
    public void testWriteMoreThanContentLength() throws Exception {
        URL url = new URL(NativeTestServer.getEchoBodyURL());
        mConnection = (HttpURLConnection) mCronetEngine.openConnection(url);
        mConnection.setDoOutput(true);
        mConnection.setRequestMethod("POST");
        // Use a content length that is 1 byte shorter than actual data.
        mConnection.setRequestProperty(
                "Content-Length", Integer.toString(TestUtil.UPLOAD_DATA.length - 1));
        OutputStream out = mConnection.getOutputStream();
        // Write a few bytes first.
        out.write(TestUtil.UPLOAD_DATA, 0, 3);
        // Write remaining bytes.
        ProtocolException e =
                assertThrows(
                        ProtocolException.class,
                        () -> out.write(TestUtil.UPLOAD_DATA, 3, TestUtil.UPLOAD_DATA.length - 3));
        assertThat(e)
                .hasMessageThat()
                .isEqualTo(
                        "exceeded content-length limit of "
                                + (TestUtil.UPLOAD_DATA.length - 1)
                                + " bytes");
    }

    /** Same as {@code testWriteMoreThanContentLength()}, but it only writes one byte at a time. */
    @Test
    @SmallTest
    public void testWriteMoreThanContentLengthWriteOneByte() throws Exception {
        URL url = new URL(NativeTestServer.getEchoBodyURL());
        mConnection = (HttpURLConnection) mCronetEngine.openConnection(url);
        mConnection.setDoOutput(true);
        mConnection.setRequestMethod("POST");
        // Use a content length that is 1 byte shorter than actual data.
        mConnection.setRequestProperty(
                "Content-Length", Integer.toString(TestUtil.UPLOAD_DATA.length - 1));
        OutputStream out = mConnection.getOutputStream();
        ProtocolException e =
                assertThrows(
                        ProtocolException.class,
                        () -> {
                            for (int i = 0; i < TestUtil.UPLOAD_DATA.length; i++) {
                                out.write(TestUtil.UPLOAD_DATA[i]);
                            }
                        });
        assertThat(e)
                .hasMessageThat()
                .isEqualTo(
                        "exceeded content-length limit of "
                                + (TestUtil.UPLOAD_DATA.length - 1)
                                + " bytes");
    }

    @Test
    @SmallTest
    public void testRewind() throws Exception {
        URL url = new URL(NativeTestServer.getRedirectToEchoBody());
        mConnection = (HttpURLConnection) mCronetEngine.openConnection(url);
        mConnection.setDoOutput(true);
        mConnection.setRequestMethod("POST");
        mConnection.setRequestProperty(
                "Content-Length", Integer.toString(TestUtil.UPLOAD_DATA.length));
        OutputStream out = mConnection.getOutputStream();
        out.write(TestUtil.UPLOAD_DATA);
        assertThat(TestUtil.getResponseAsString(mConnection))
                .isEqualTo(TestUtil.UPLOAD_DATA_STRING);
    }

    /** Like {@link #testRewind} but does not set Content-Length header. */
    @Test
    @SmallTest
    public void testRewindWithoutContentLength() throws Exception {
        URL url = new URL(NativeTestServer.getRedirectToEchoBody());
        mConnection = (HttpURLConnection) mCronetEngine.openConnection(url);
        mConnection.setDoOutput(true);
        mConnection.setRequestMethod("POST");
        OutputStream out = mConnection.getOutputStream();
        out.write(TestUtil.UPLOAD_DATA);
        assertThat(TestUtil.getResponseAsString(mConnection))
                .isEqualTo(TestUtil.UPLOAD_DATA_STRING);
    }
}
