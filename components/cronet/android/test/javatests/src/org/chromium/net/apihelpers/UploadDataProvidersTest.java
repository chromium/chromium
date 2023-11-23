// Copyright 2016 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.net.apihelpers;

import static com.google.common.truth.Truth.assertThat;

import static org.chromium.net.truth.UrlResponseInfoSubject.assertThat;

import android.os.ConditionVariable;
import android.os.ParcelFileDescriptor;

import androidx.test.ext.junit.runners.AndroidJUnit4;
import androidx.test.filters.SmallTest;

import org.junit.After;
import org.junit.Before;
import org.junit.Rule;
import org.junit.Test;
import org.junit.runner.RunWith;

import org.chromium.base.test.util.DoNotBatch;
import org.chromium.net.CallbackException;
import org.chromium.net.CronetTestRule;
import org.chromium.net.CronetTestRule.CronetImplementation;
import org.chromium.net.CronetTestRule.IgnoreFor;
import org.chromium.net.NativeTestServer;
import org.chromium.net.TestUrlRequestCallback;
import org.chromium.net.UploadDataProvider;
import org.chromium.net.UploadDataSink;
import org.chromium.net.UrlRequest;

import java.io.File;
import java.io.FileOutputStream;
import java.io.IOException;
import java.nio.ByteBuffer;

/** Test the default provided implementations of {@link UploadDataProvider} */
@DoNotBatch(reason = "crbug/1459563")
@RunWith(AndroidJUnit4.class)
public class UploadDataProvidersTest {
    private static final String LOREM =
            "Lorem ipsum dolor sit amet, consectetur adipiscing elit. Proin elementum, libero"
                    + " laoreet fringilla faucibus, metus tortor vehicula ante, lacinia lorem eros vel"
                    + " sapien.";
    @Rule public final CronetTestRule mTestRule = CronetTestRule.withAutomaticEngineStartup();
    private File mFile;

    @Before
    public void setUp() throws Exception {
        assertThat(
                        NativeTestServer.startNativeTestServer(
                                mTestRule.getTestFramework().getContext()))
                .isTrue();
        // Add url interceptors after native application context is initialized.
        mFile =
                new File(
                        mTestRule.getTestFramework().getContext().getCacheDir().getPath()
                                + "/tmpfile");
        FileOutputStream fileOutputStream = new FileOutputStream(mFile);
        try {
            fileOutputStream.write(LOREM.getBytes("UTF-8"));
        } finally {
            fileOutputStream.close();
        }
    }

    @After
    public void tearDown() throws Exception {
        NativeTestServer.shutdownNativeTestServer();
        assertThat(mFile.delete()).isTrue();
    }

    @Test
    @SmallTest
    public void testFileProvider() throws Exception {
        TestUrlRequestCallback callback = new TestUrlRequestCallback();
        UrlRequest.Builder builder =
                mTestRule
                        .getTestFramework()
                        .getEngine()
                        .newUrlRequestBuilder(
                                NativeTestServer.getRedirectToEchoBody(),
                                callback,
                                callback.getExecutor());
        UploadDataProvider dataProvider = UploadDataProviders.create(mFile);
        builder.setUploadDataProvider(dataProvider, callback.getExecutor());
        builder.addHeader("Content-Type", "useless/string");
        builder.build().start();
        callback.blockForDone();
        assertThat(callback.getResponseInfoWithChecks()).hasHttpStatusCodeThat().isEqualTo(200);
        assertThat(callback.mResponseAsString).isEqualTo(LOREM);
    }

    @Test
    @SmallTest
    public void testFileDescriptorProvider() throws Exception {
        ParcelFileDescriptor descriptor =
                ParcelFileDescriptor.open(mFile, ParcelFileDescriptor.MODE_READ_ONLY);
        assertThat(descriptor.getFileDescriptor().valid()).isTrue();
        TestUrlRequestCallback callback = new TestUrlRequestCallback();
        UrlRequest.Builder builder =
                mTestRule
                        .getTestFramework()
                        .getEngine()
                        .newUrlRequestBuilder(
                                NativeTestServer.getRedirectToEchoBody(),
                                callback,
                                callback.getExecutor());
        UploadDataProvider dataProvider = UploadDataProviders.create(descriptor);
        builder.setUploadDataProvider(dataProvider, callback.getExecutor());
        builder.addHeader("Content-Type", "useless/string");
        builder.build().start();
        callback.blockForDone();
        assertThat(callback.getResponseInfoWithChecks()).hasHttpStatusCodeThat().isEqualTo(200);
        assertThat(callback.mResponseAsString).isEqualTo(LOREM);
    }

    @Test
    @SmallTest
    public void testBadFileDescriptorProvider() throws Exception {
        TestUrlRequestCallback callback = new TestUrlRequestCallback();
        UrlRequest.Builder builder =
                mTestRule
                        .getTestFramework()
                        .getEngine()
                        .newUrlRequestBuilder(
                                NativeTestServer.getRedirectToEchoBody(),
                                callback,
                                callback.getExecutor());
        ParcelFileDescriptor[] pipe = ParcelFileDescriptor.createPipe();
        try {
            UploadDataProvider dataProvider = UploadDataProviders.create(pipe[0]);
            builder.setUploadDataProvider(dataProvider, callback.getExecutor());
            builder.addHeader("Content-Type", "useless/string");
            builder.build().start();
            callback.blockForDone();

            assertThat(callback.mError).hasCauseThat().isInstanceOf(IllegalArgumentException.class);
        } finally {
            pipe[1].close();
        }
    }

    @Test
    @SmallTest
    public void testBufferProvider() throws Exception {
        TestUrlRequestCallback callback = new TestUrlRequestCallback();
        UrlRequest.Builder builder =
                mTestRule
                        .getTestFramework()
                        .getEngine()
                        .newUrlRequestBuilder(
                                NativeTestServer.getRedirectToEchoBody(),
                                callback,
                                callback.getExecutor());
        UploadDataProvider dataProvider = UploadDataProviders.create(LOREM.getBytes("UTF-8"));
        builder.setUploadDataProvider(dataProvider, callback.getExecutor());
        builder.addHeader("Content-Type", "useless/string");
        builder.build().start();
        callback.blockForDone();

        assertThat(callback.getResponseInfoWithChecks()).hasHttpStatusCodeThat().isEqualTo(200);
        assertThat(callback.mResponseAsString).isEqualTo(LOREM);
    }

    @Test
    @SmallTest
    @IgnoreFor(
            implementations = {CronetImplementation.FALLBACK},
            reason = "This is not the case for the fallback implementation")
    // Tests that ByteBuffer's limit cannot be changed by the caller.
    public void testUploadChangeBufferLimit() throws Exception {
        TestUrlRequestCallback callback = new TestUrlRequestCallback();
        UrlRequest.Builder builder =
                mTestRule
                        .getTestFramework()
                        .getEngine()
                        .newUrlRequestBuilder(
                                NativeTestServer.getEchoBodyURL(),
                                callback,
                                callback.getExecutor());
        builder.addHeader("Content-Type", "useless/string");
        builder.setUploadDataProvider(
                new UploadDataProvider() {
                    private static final String CONTENT = "hello";

                    @Override
                    public long getLength() throws IOException {
                        return CONTENT.length();
                    }

                    @Override
                    public void read(UploadDataSink uploadDataSink, ByteBuffer byteBuffer)
                            throws IOException {
                        int oldPos = byteBuffer.position();
                        int oldLimit = byteBuffer.limit();
                        byteBuffer.put(CONTENT.getBytes());
                        assertThat(byteBuffer.position()).isEqualTo(oldPos + CONTENT.length());
                        assertThat(byteBuffer.limit()).isEqualTo(oldLimit);
                        // Now change the limit to something else. This should give an error.
                        byteBuffer.limit(oldLimit - 1);
                        uploadDataSink.onReadSucceeded(false);
                    }

                    @Override
                    public void rewind(UploadDataSink uploadDataSink) throws IOException {}
                },
                callback.getExecutor());
        UrlRequest urlRequest = builder.build();
        urlRequest.start();
        callback.blockForDone();
        assertThat(callback.mOnErrorCalled).isTrue();
        assertThat(callback.mError)
                .hasMessageThat()
                .contains("Exception received from UploadDataProvider");
        assertThat(callback.mError)
                .hasCauseThat()
                .hasMessageThat()
                .contains("ByteBuffer limit changed");
    }

    @Test
    @SmallTest
    public void testNoErrorWhenCanceledDuringStart() throws Exception {
        TestUrlRequestCallback callback = new TestUrlRequestCallback();
        UrlRequest.Builder builder =
                mTestRule
                        .getTestFramework()
                        .getEngine()
                        .newUrlRequestBuilder(
                                NativeTestServer.getEchoBodyURL(),
                                callback,
                                callback.getExecutor());
        final ConditionVariable first = new ConditionVariable();
        final ConditionVariable second = new ConditionVariable();
        builder.addHeader("Content-Type", "useless/string");
        builder.setUploadDataProvider(
                new UploadDataProvider() {
                    @Override
                    public long getLength() throws IOException {
                        first.open();
                        second.block();
                        return 0;
                    }

                    @Override
                    public void read(UploadDataSink uploadDataSink, ByteBuffer byteBuffer)
                            throws IOException {}

                    @Override
                    public void rewind(UploadDataSink uploadDataSink) throws IOException {}
                },
                callback.getExecutor());
        UrlRequest urlRequest = builder.build();
        urlRequest.start();
        first.block();
        urlRequest.cancel();
        second.open();
        callback.blockForDone();
        assertThat(callback.mOnCanceledCalled).isTrue();
    }

    @Test
    @SmallTest
    public void testNoErrorWhenExceptionDuringStart() throws Exception {
        TestUrlRequestCallback callback = new TestUrlRequestCallback();
        UrlRequest.Builder builder =
                mTestRule
                        .getTestFramework()
                        .getEngine()
                        .newUrlRequestBuilder(
                                NativeTestServer.getEchoBodyURL(),
                                callback,
                                callback.getExecutor());
        final ConditionVariable first = new ConditionVariable();
        final String exceptionMessage = "Bad Length";
        builder.addHeader("Content-Type", "useless/string");
        builder.setUploadDataProvider(
                new UploadDataProvider() {
                    @Override
                    public long getLength() throws IOException {
                        first.open();
                        throw new IOException(exceptionMessage);
                    }

                    @Override
                    public void read(UploadDataSink uploadDataSink, ByteBuffer byteBuffer)
                            throws IOException {}

                    @Override
                    public void rewind(UploadDataSink uploadDataSink) throws IOException {}
                },
                callback.getExecutor());
        UrlRequest urlRequest = builder.build();
        urlRequest.start();
        first.block();
        callback.blockForDone();
        assertThat(callback.mOnCanceledCalled).isFalse();
        assertThat(callback.mError).isInstanceOf(CallbackException.class);
        assertThat(callback.mError)
                .hasMessageThat()
                .contains("Exception received from UploadDataProvider");
        assertThat(callback.mError).hasCauseThat().hasMessageThat().contains(exceptionMessage);
    }

    @Test
    @SmallTest
    // Tests that creating a ByteBufferUploadProvider using a byte array with an
    // offset gives a ByteBuffer with position 0. crbug.com/603124.
    public void testCreateByteBufferUploadWithArrayOffset() throws Exception {
        TestUrlRequestCallback callback = new TestUrlRequestCallback();
        // This URL will trigger a rewind().
        UrlRequest.Builder builder =
                mTestRule
                        .getTestFramework()
                        .getEngine()
                        .newUrlRequestBuilder(
                                NativeTestServer.getRedirectToEchoBody(),
                                callback,
                                callback.getExecutor());
        builder.addHeader("Content-Type", "useless/string");
        byte[] uploadData = LOREM.getBytes("UTF-8");
        int offset = 5;
        byte[] uploadDataWithPadding = new byte[uploadData.length + offset];
        System.arraycopy(uploadData, 0, uploadDataWithPadding, offset, uploadData.length);
        UploadDataProvider dataProvider =
                UploadDataProviders.create(uploadDataWithPadding, offset, uploadData.length);
        assertThat(dataProvider.getLength()).isEqualTo(uploadData.length);
        builder.setUploadDataProvider(dataProvider, callback.getExecutor());
        UrlRequest urlRequest = builder.build();
        urlRequest.start();
        callback.blockForDone();
        assertThat(callback.getResponseInfoWithChecks()).hasHttpStatusCodeThat().isEqualTo(200);
        assertThat(callback.mResponseAsString).isEqualTo(LOREM);
    }
}
