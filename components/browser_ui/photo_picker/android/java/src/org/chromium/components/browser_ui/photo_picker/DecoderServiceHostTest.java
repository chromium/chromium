// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.components.browser_ui.photo_picker;

import android.content.Context;
import android.content.Intent;
import android.graphics.Bitmap;
import android.net.Uri;
import android.os.Build;

import androidx.test.InstrumentationRegistry;
import androidx.test.filters.LargeTest;
import androidx.test.filters.SmallTest;

import org.junit.Assert;
import org.junit.Before;
import org.junit.Test;
import org.junit.runner.RunWith;

import org.chromium.base.ThreadUtils;
import org.chromium.base.test.BaseJUnit4ClassRunner;
import org.chromium.base.test.util.Batch;
import org.chromium.base.test.util.CallbackHelper;
import org.chromium.base.test.util.DisabledTest;
import org.chromium.base.test.util.MinAndroidSdkLevel;
import org.chromium.base.test.util.UrlUtils;
import org.chromium.content_public.browser.test.NativeLibraryTestUtils;

import java.io.File;
import java.util.List;
import java.util.concurrent.TimeUnit;

/** Tests for the DecoderServiceHost. */
@RunWith(BaseJUnit4ClassRunner.class)
@Batch(Batch.UNIT_TESTS)
public class DecoderServiceHostTest
        implements DecoderServiceHost.DecoderStatusCallback,
                DecoderServiceHost.ImagesDecodedCallback {
    // The timeout (in milliseconds) to wait for the decoding.
    private static final int WAIT_TIMEOUT_MS = 7500;

    // The base test file path.
    private static final String TEST_FILE_PATH = "chrome/test/data/android/photo_picker/";

    private Context mContext;

    // A callback that fires when the decoder is ready.
    public final CallbackHelper mOnDecoderReadyCallback = new CallbackHelper();

    // A callback that fires when the decoder is idle.
    public final CallbackHelper mOnDecoderIdleCallback = new CallbackHelper();

    // A callback that fires when something is finished decoding in the dialog.
    public final CallbackHelper mOnDecodedCallback = new CallbackHelper();

    private String mLastDecodedPath;
    private boolean mLastIsVideo;
    private Bitmap mLastInitialFrame;
    private int mLastFrameCount;
    private String mLastVideoDuration;
    private float mLastRatio;

    @Before
    public void setUp() throws Exception {
        mContext = InstrumentationRegistry.getTargetContext();
        NativeLibraryTestUtils.loadNativeLibraryNoBrowserProcess();

        ThreadUtils.runOnUiThreadBlocking(
                () -> {
                    DecoderServiceHost.setIntentSupplier(
                            () -> {
                                return new Intent(mContext, TestImageDecoderService.class);
                            });
                });

        DecoderServiceHost.setStatusCallback(this);
    }

    // DecoderServiceHost.DecoderStatusCallback:

    @Override
    public void serviceReady() {
        mOnDecoderReadyCallback.notifyCalled();
    }

    @Override
    public void decoderIdle() {
        mOnDecoderIdleCallback.notifyCalled();
    }

    // DecoderServiceHost.ImagesDecodedCallback:

    @Override
    public void imagesDecodedCallback(
            String filePath,
            boolean isVideo,
            boolean isZoomedIn,
            List<Bitmap> bitmaps,
            String videoDuration,
            float ratio) {
        mLastDecodedPath = filePath;
        mLastIsVideo = isVideo;
        mLastFrameCount = bitmaps != null ? bitmaps.size() : -1;
        mLastInitialFrame = bitmaps != null ? bitmaps.get(0) : null;
        mLastVideoDuration = videoDuration;
        mLastRatio = ratio;

        mOnDecodedCallback.notifyCalled();
    }

    private void waitForDecoder() throws Exception {
        int callCount = mOnDecoderReadyCallback.getCallCount();
        mOnDecoderReadyCallback.waitForCallback(
                callCount, 1, WAIT_TIMEOUT_MS, TimeUnit.MILLISECONDS);
    }

    private void waitForDecoderIdle(int lastCount) throws Exception {
        mOnDecoderIdleCallback.waitForCallback(
                lastCount, 1, WAIT_TIMEOUT_MS, TimeUnit.MILLISECONDS);
    }

    private void waitForThumbnailDecode() throws Exception {
        int callCount = mOnDecodedCallback.getCallCount();
        mOnDecodedCallback.waitForCallback(callCount, 1, WAIT_TIMEOUT_MS, TimeUnit.MILLISECONDS);
    }

    private void decodeImage(
            DecoderServiceHost host,
            Uri uri,
            @PickerBitmap.TileTypes int fileType,
            int width,
            boolean fullWidth,
            DecoderServiceHost.ImagesDecodedCallback callback) {
        ThreadUtils.runOnUiThreadBlocking(
                () -> host.decodeImage(uri, fileType, width, fullWidth, callback));
    }

    private void cancelDecodeImage(DecoderServiceHost host, String filePath) {
        ThreadUtils.runOnUiThreadBlocking(() -> host.cancelDecodeImage(filePath));
    }

    @Test
    @SmallTest
    public void testRequestComparator() throws Throwable {
        Uri uri = Uri.parse("http://example.com");
        int width = 100;
        boolean fullWidth = true;
        DecoderServiceHost.ImagesDecodedCallback callback = null;

        DecoderServiceHost.DecoderServiceParams higherPri;
        DecoderServiceHost.DecoderServiceParams lowerPri;

        // Still image decoding has higher priority than first frame video decoding.
        higherPri =
                new DecoderServiceHost.DecoderServiceParams(
                        uri,
                        width,
                        fullWidth,
                        PickerBitmap.TileTypes.PICTURE,
                        /* firstFrame= */ true,
                        callback);
        lowerPri =
                new DecoderServiceHost.DecoderServiceParams(
                        uri,
                        width,
                        fullWidth,
                        PickerBitmap.TileTypes.VIDEO,
                        /* firstFrame= */ true,
                        callback);
        DecoderServiceHost host = new DecoderServiceHost(this, mContext);
        Assert.assertTrue(
                "Still images have priority over requests for initial video frame",
                host.mRequestComparator.compare(higherPri, lowerPri) < 0);

        // Still image decoding has higher priority than decoding remaining video frames.
        higherPri =
                new DecoderServiceHost.DecoderServiceParams(
                        uri,
                        width,
                        fullWidth,
                        PickerBitmap.TileTypes.PICTURE,
                        /* firstFrame= */ true,
                        callback);
        lowerPri =
                new DecoderServiceHost.DecoderServiceParams(
                        uri,
                        width,
                        fullWidth,
                        PickerBitmap.TileTypes.VIDEO,
                        /* firstFrame= */ false,
                        callback);
        Assert.assertTrue(
                "Still images have priority over requests for remaining video frames",
                host.mRequestComparator.compare(higherPri, lowerPri) < 0);

        // First frame video request have priority over remaining video frames.
        higherPri =
                new DecoderServiceHost.DecoderServiceParams(
                        uri,
                        width,
                        fullWidth,
                        PickerBitmap.TileTypes.VIDEO,
                        /* firstFrame= */ true,
                        callback);
        lowerPri =
                new DecoderServiceHost.DecoderServiceParams(
                        uri,
                        width,
                        fullWidth,
                        PickerBitmap.TileTypes.VIDEO,
                        /* firstFrame= */ false,
                        callback);
        Assert.assertTrue(
                "Initial video frames have priority over remaining video frames",
                host.mRequestComparator.compare(higherPri, lowerPri) < 0);

        // Enforce FIFO principle for two identical still image requests.
        higherPri =
                new DecoderServiceHost.DecoderServiceParams(
                        uri,
                        width,
                        fullWidth,
                        PickerBitmap.TileTypes.PICTURE,
                        /* firstFrame= */ true,
                        callback);
        lowerPri =
                new DecoderServiceHost.DecoderServiceParams(
                        uri,
                        width,
                        fullWidth,
                        PickerBitmap.TileTypes.PICTURE,
                        /* firstFrame= */ true,
                        callback);
        Assert.assertTrue(
                "Identical still image requests should be processed FIFO",
                host.mRequestComparator.compare(higherPri, lowerPri) < 0);

        // Enforce FIFO principle for two identical video requests (initial frames).
        higherPri =
                new DecoderServiceHost.DecoderServiceParams(
                        uri,
                        width,
                        fullWidth,
                        PickerBitmap.TileTypes.VIDEO,
                        /* firstFrame= */ true,
                        callback);
        lowerPri =
                new DecoderServiceHost.DecoderServiceParams(
                        uri,
                        width,
                        fullWidth,
                        PickerBitmap.TileTypes.VIDEO,
                        /* firstFrame= */ true,
                        callback);
        Assert.assertTrue(
                "Identical video requests (initial frames) should be processed FIFO",
                host.mRequestComparator.compare(higherPri, lowerPri) < 0);

        // Enforce FIFO principle for two identical video requests (remaining frames).
        higherPri =
                new DecoderServiceHost.DecoderServiceParams(
                        uri,
                        width,
                        fullWidth,
                        PickerBitmap.TileTypes.VIDEO,
                        /* firstFrame= */ false,
                        callback);
        lowerPri =
                new DecoderServiceHost.DecoderServiceParams(
                        uri,
                        width,
                        fullWidth,
                        PickerBitmap.TileTypes.VIDEO,
                        /* firstFrame= */ false,
                        callback);
        Assert.assertTrue(
                "Identical video requests (remanining frames) should be processed FIFO",
                host.mRequestComparator.compare(higherPri, lowerPri) < 0);
    }

    @Test
    @LargeTest
    @MinAndroidSdkLevel(Build.VERSION_CODES.O) // Video is only supported on O+.
    public void testDecodingOrder() throws Throwable {
        DecoderServiceHost host = new DecoderServiceHost(this, mContext);
        host.bind();
        waitForDecoder();

        String video1 = "noogler.mp4";
        String video2 = "noogler2.mp4";
        String jpg1 = "blue100x100.jpg";
        File file1 = new File(UrlUtils.getIsolatedTestFilePath(TEST_FILE_PATH + video1));
        File file2 = new File(UrlUtils.getIsolatedTestFilePath(TEST_FILE_PATH + video2));
        File file3 = new File(UrlUtils.getIsolatedTestFilePath(TEST_FILE_PATH + jpg1));

        decodeImage(
                host,
                Uri.fromFile(file1),
                PickerBitmap.TileTypes.VIDEO,
                10,
                /* fullWidth= */ false,
                this);
        decodeImage(
                host,
                Uri.fromFile(file2),
                PickerBitmap.TileTypes.VIDEO,
                10,
                /* fullWidth= */ false,
                this);
        decodeImage(
                host,
                Uri.fromFile(file3),
                PickerBitmap.TileTypes.PICTURE,
                10,
                /* fullWidth= */ false,
                this);

        int idleCallCount = mOnDecoderIdleCallback.getCallCount();

        // First decoding result should be first frame of video 1. Even though still images take
        // priority over video decoding, video 1 will be the only item in the queue when the first
        // decoding request is kicked off (as a result of calling decodeImage).
        waitForThumbnailDecode();
        Assert.assertTrue(mLastDecodedPath.contains(video1));
        Assert.assertEquals(true, mLastIsVideo);
        Assert.assertEquals("0:00", mLastVideoDuration);
        Assert.assertEquals(1, mLastFrameCount);

        // When the decoder is finished with the first frame of video 1, there will be two new
        // requests available for processing. Video2 was added first, but that will be skipped in
        // favor of the still image, so the jpg is expected to be decoded next.
        waitForThumbnailDecode();
        Assert.assertTrue(mLastDecodedPath.contains(jpg1));
        Assert.assertEquals(false, mLastIsVideo);
        Assert.assertEquals(null, mLastVideoDuration);
        Assert.assertEquals(1, mLastFrameCount);

        // Third and last decoding result is first frame of video 2.
        waitForThumbnailDecode();
        Assert.assertTrue(mLastDecodedPath.contains(video2));
        Assert.assertEquals(true, mLastIsVideo);
        Assert.assertEquals("0:00", mLastVideoDuration);
        Assert.assertEquals(1, mLastFrameCount);

        // Make sure nothing else is returned (no animations should be supported).
        waitForDecoderIdle(idleCallCount);

        // Everything should be as we left it.
        Assert.assertTrue(mLastDecodedPath.contains(video2));
        Assert.assertEquals(true, mLastIsVideo);
        Assert.assertEquals("0:00", mLastVideoDuration);
        Assert.assertEquals(1, mLastFrameCount);

        host.unbind();
    }

    @Test
    @LargeTest
    @MinAndroidSdkLevel(Build.VERSION_CODES.O) // Video is only supported on O+.
    public void testDecodingSizes() throws Throwable {
        DecoderServiceHost host = new DecoderServiceHost(this, mContext);
        host.bind();
        waitForDecoder();

        String video1 = "noogler.mp4"; // 1920 x 1080 video.
        String jpg1 = "blue100x100.jpg";
        File file1 = new File(UrlUtils.getIsolatedTestFilePath(TEST_FILE_PATH + video1));
        File file2 = new File(UrlUtils.getIsolatedTestFilePath(TEST_FILE_PATH + jpg1));

        // Thumbnail photo. 100 x 100 -> 10 x 10.
        decodeImage(
                host,
                Uri.fromFile(file2),
                PickerBitmap.TileTypes.PICTURE,
                10,
                /* fullWidth= */ false,
                this);
        waitForThumbnailDecode();
        Assert.assertTrue(mLastDecodedPath.contains(jpg1));
        Assert.assertEquals(false, mLastIsVideo);
        Assert.assertEquals(null, mLastVideoDuration);
        Assert.assertEquals(1, mLastFrameCount);
        Assert.assertEquals(1.0f, mLastRatio, 0.1f);
        Assert.assertEquals(10, mLastInitialFrame.getWidth());
        Assert.assertEquals(10, mLastInitialFrame.getHeight());

        // Full-width photo. 100 x 100 -> 200 x 200.
        decodeImage(
                host,
                Uri.fromFile(file2),
                PickerBitmap.TileTypes.PICTURE,
                200,
                /* fullWidth= */ true,
                this);
        waitForThumbnailDecode();
        Assert.assertTrue(mLastDecodedPath.contains(jpg1));
        Assert.assertEquals(false, mLastIsVideo);
        Assert.assertEquals(null, mLastVideoDuration);
        Assert.assertEquals(1, mLastFrameCount);
        Assert.assertEquals(1.0f, mLastRatio, 0.1f);
        Assert.assertEquals(200, mLastInitialFrame.getWidth());
        Assert.assertEquals(200, mLastInitialFrame.getHeight());

        // Thumbnail video. 1920 x 1080 -> 10 x 10.
        decodeImage(
                host,
                Uri.fromFile(file1),
                PickerBitmap.TileTypes.VIDEO,
                10,
                /* fullWidth= */ false,
                this);
        waitForThumbnailDecode(); // Initial frame.
        Assert.assertTrue(mLastDecodedPath.contains(video1));
        Assert.assertEquals(true, mLastIsVideo);
        Assert.assertEquals("0:00", mLastVideoDuration);
        Assert.assertEquals(1, mLastFrameCount);
        Assert.assertEquals(0.5625f, mLastRatio, 0.0001f);
        Assert.assertEquals(10, mLastInitialFrame.getWidth());
        Assert.assertEquals(10, mLastInitialFrame.getHeight());

        // Full-width video. 1920 x 1080 -> 2000 x 1125.
        decodeImage(
                host,
                Uri.fromFile(file1),
                PickerBitmap.TileTypes.VIDEO,
                2000,
                /* fullWidth= */ true,
                this);
        waitForThumbnailDecode(); // Initial frame.
        Assert.assertTrue(mLastDecodedPath.contains(video1));
        Assert.assertEquals(true, mLastIsVideo);
        Assert.assertEquals("0:00", mLastVideoDuration);
        Assert.assertEquals(1, mLastFrameCount);
        Assert.assertEquals(0.5625f, mLastRatio, 0.0001f);
        Assert.assertEquals(2000, mLastInitialFrame.getWidth());
        Assert.assertEquals(1125, mLastInitialFrame.getHeight());

        host.unbind();
    }

    @Test
    @LargeTest
    @DisabledTest(message = "See crbug.com/1306924") // Disabled because it is flaky
    public void testCancelation() throws Throwable {
        DecoderServiceHost host = new DecoderServiceHost(this, mContext);
        host.bind();
        waitForDecoder();

        String green = "green100x100.jpg";
        String yellow = "yellow100x100.jpg";
        String red = "red100x100.jpg";
        String greenPath = UrlUtils.getIsolatedTestFilePath(TEST_FILE_PATH + green);
        String yellowPath = UrlUtils.getIsolatedTestFilePath(TEST_FILE_PATH + yellow);
        String redPath = UrlUtils.getIsolatedTestFilePath(TEST_FILE_PATH + red);

        decodeImage(
                host,
                Uri.fromFile(new File(greenPath)),
                PickerBitmap.TileTypes.PICTURE,
                10,
                /* fullWidth= */ false,
                this);
        decodeImage(
                host,
                Uri.fromFile(new File(yellowPath)),
                PickerBitmap.TileTypes.PICTURE,
                10,
                /* fullWidth= */ false,
                this);

        // Now add and subsequently remove the request.
        decodeImage(
                host,
                Uri.fromFile(new File(redPath)),
                PickerBitmap.TileTypes.PICTURE,
                10,
                /* fullWidth= */ false,
                this);
        cancelDecodeImage(host, redPath);

        // First decoding result should be the green image.
        waitForThumbnailDecode();
        Assert.assertEquals(greenPath, mLastDecodedPath);

        // Next is the yellow image, and asserts in DecoderServiceHost (designed to catch when
        // multiple simultaneous decoding requests are started) should not fire.
        waitForThumbnailDecode();
        Assert.assertEquals(yellowPath, mLastDecodedPath);

        host.unbind();
    }

    @Test
    @LargeTest
    public void testNoConnectionFailureMode() throws Throwable {
        DecoderServiceHost host = new DecoderServiceHost(this, mContext);

        // Try decoding without a connection to the decoder.
        String green = "green100x100.jpg";
        String greenPath = UrlUtils.getIsolatedTestFilePath(TEST_FILE_PATH + green);
        decodeImage(
                host,
                Uri.fromFile(new File(greenPath)),
                PickerBitmap.TileTypes.PICTURE,
                10,
                /* fullWidth= */ false,
                this);
        Assert.assertEquals(greenPath, mLastDecodedPath);
        Assert.assertEquals(null, mLastInitialFrame);
    }

    @Test
    @LargeTest
    public void testFileNotFoundFailureMode() throws Throwable {
        DecoderServiceHost host = new DecoderServiceHost(this, mContext);
        host.bind();
        waitForDecoder();

        // Try decoding a file that doesn't exist.
        String noPath = "/nonexistentpath/nonexistentfile";
        decodeImage(
                host,
                Uri.fromFile(new File(noPath)),
                PickerBitmap.TileTypes.PICTURE,
                10,
                /* fullWidth= */ false,
                this);
        Assert.assertEquals(noPath, mLastDecodedPath);
        Assert.assertEquals(null, mLastInitialFrame);

        host.unbind();
    }
}
