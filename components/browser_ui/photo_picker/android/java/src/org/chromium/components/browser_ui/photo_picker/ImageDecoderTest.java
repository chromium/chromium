// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.components.browser_ui.photo_picker;

import android.content.ComponentName;
import android.content.Context;
import android.content.Intent;
import android.content.ServiceConnection;
import android.graphics.Bitmap;
import android.os.Bundle;
import android.os.IBinder;
import android.os.ParcelFileDescriptor;

import androidx.test.InstrumentationRegistry;
import androidx.test.filters.LargeTest;

import org.junit.Assert;
import org.junit.Before;
import org.junit.Test;
import org.junit.runner.RunWith;

import org.chromium.base.test.BaseJUnit4ClassRunner;
import org.chromium.base.test.util.CriteriaHelper;
import org.chromium.base.test.util.UrlUtils;

import java.io.File;
import java.io.FileDescriptor;
import java.io.FileInputStream;

/** Tests for ImageDecoder and the aidl interfaces used for out-of-process decoding.. */
@RunWith(BaseJUnit4ClassRunner.class)
public class ImageDecoderTest {
    // By default, the test will wait for 3 seconds to create the decoder process, which (at least
    // in the emulators) brushes up against the actual time it takes to create the process, so these
    // tests are frequently flaky when run locally.
    private static final int DECODER_STARTUP_TIMEOUT_IN_MS = 7500;

    Context mContext;

    // Flag indicating whether we are bound to the service.
    private boolean mBound;

    private static class DecoderServiceCallback extends IDecoderServiceCallback.Stub {
        // The returned bundle from the decoder.
        private Bundle mDecodedBundle;

        public boolean resolved() {
            return mDecodedBundle != null;
        }

        public Bundle getBundle() {
            return mDecodedBundle;
        }

        @Override
        public void onDecodeImageDone(final Bundle payload) {
            mDecodedBundle = payload;
        }
    }

    IDecoderService mIRemoteService;
    private ServiceConnection mConnection =
            new ServiceConnection() {
                @Override
                public void onServiceConnected(ComponentName className, IBinder service) {
                    mIRemoteService = IDecoderService.Stub.asInterface(service);
                    mBound = true;
                }

                @Override
                public void onServiceDisconnected(ComponentName className) {
                    mIRemoteService = null;
                    mBound = false;
                }
            };

    @Before
    public void setUp() throws Exception {
        mContext = InstrumentationRegistry.getTargetContext();
    }

    private void startDecoderService() {
        Intent intent = new Intent(mContext, TestImageDecoderService.class);
        intent.setAction(IDecoderService.class.getName());
        mContext.bindService(intent, mConnection, Context.BIND_AUTO_CREATE);

        CriteriaHelper.pollUiThread(
                () -> mBound,
                DECODER_STARTUP_TIMEOUT_IN_MS,
                CriteriaHelper.DEFAULT_POLLING_INTERVAL);
    }

    private void decode(
            String filePath, FileDescriptor fd, int width, final DecoderServiceCallback callback)
            throws Exception {
        Bundle bundle = new Bundle();
        bundle.putString(ImageDecoder.KEY_FILE_PATH, filePath);
        ParcelFileDescriptor pfd = null;
        if (fd != null) {
            pfd = ParcelFileDescriptor.dup(fd);
            Assert.assertTrue(pfd != null);
        }
        bundle.putParcelable(ImageDecoder.KEY_FILE_DESCRIPTOR, pfd);
        bundle.putInt(ImageDecoder.KEY_WIDTH, width);

        mIRemoteService.decodeImage(bundle, callback);
        CriteriaHelper.pollUiThread(() -> callback.resolved());
    }

    @Test
    @LargeTest
    public void testServiceDecodeNullFileDescriptor() throws Throwable {
        startDecoderService();

        // Attempt to decode to a 50x50 thumbnail without a valid FileDescriptor (null).
        DecoderServiceCallback callback = new DecoderServiceCallback();
        decode("path", null, 50, callback);

        Bundle bundle = callback.getBundle();
        Assert.assertFalse("Expected decode to fail", bundle.getBoolean(ImageDecoder.KEY_SUCCESS));
        Assert.assertEquals("path", bundle.getString(ImageDecoder.KEY_FILE_PATH));
        Assert.assertEquals(null, bundle.getParcelable(ImageDecoder.KEY_IMAGE_BITMAP));
        Assert.assertEquals(0, bundle.getLong(ImageDecoder.KEY_DECODE_TIME));
    }

    @Test
    @LargeTest
    public void testServiceDecodeSimple() throws Exception {
        startDecoderService();

        File file =
                new File(
                        UrlUtils.getIsolatedTestFilePath(
                                "chrome/test/data/android/photo_picker/blue100x100.jpg"));
        FileInputStream inStream = new FileInputStream(file);

        // Attempt to decode a valid 100x100 image file to a 50x50 thumbnail.
        DecoderServiceCallback callback = new DecoderServiceCallback();
        decode(file.getPath(), inStream.getFD(), 50, callback);

        Bundle bundle = callback.getBundle();
        Assert.assertTrue(
                "Expecting success being returned", bundle.getBoolean(ImageDecoder.KEY_SUCCESS));
        Assert.assertEquals(file.getPath(), bundle.getString(ImageDecoder.KEY_FILE_PATH));
        Assert.assertFalse(
                "Decoding should take a non-zero amount of time",
                0 == bundle.getLong(ImageDecoder.KEY_DECODE_TIME));

        Bitmap decodedBitmap = bundle.getParcelable(ImageDecoder.KEY_IMAGE_BITMAP);
        Assert.assertFalse("Decoded bitmap should not be null", null == decodedBitmap);
        Assert.assertEquals(50, decodedBitmap.getWidth());
        Assert.assertEquals(50, decodedBitmap.getHeight());
    }
}
