// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.components.browser_ui.photo_picker;

import android.content.ComponentName;
import android.content.ContentResolver;
import android.content.Context;
import android.content.Intent;
import android.content.ServiceConnection;
import android.content.res.AssetFileDescriptor;
import android.graphics.Bitmap;
import android.net.Uri;
import android.os.Build;
import android.os.Bundle;
import android.os.IBinder;
import android.os.ParcelFileDescriptor;
import android.os.SystemClock;

import androidx.annotation.NonNull;
import androidx.annotation.Nullable;
import androidx.annotation.VisibleForTesting;

import org.chromium.base.Log;
import org.chromium.base.StreamUtil;
import org.chromium.base.StrictModeContext;
import org.chromium.base.ThreadUtils;
import org.chromium.base.metrics.RecordHistogram;
import org.chromium.base.supplier.Supplier;
import org.chromium.base.task.AsyncTask;
import org.chromium.base.task.PostTask;
import org.chromium.base.task.TaskTraits;
import org.chromium.components.browser_ui.util.ConversionUtils;

import java.util.ArrayList;
import java.util.Comparator;
import java.util.Iterator;
import java.util.List;
import java.util.PriorityQueue;

/** A class to communicate with the {@link DecoderService}. */
public class DecoderServiceHost extends IDecoderServiceCallback.Stub
        implements DecodeVideoTask.VideoDecodingCallback {
    // A tag for logging error messages.
    private static final String TAG = "ImageDecoderHost";

    // The current context.
    private final Context mContext;

    // A content resolver for providing file descriptors for the images.
    private ContentResolver mContentResolver;

    // The number of successful image decodes (not video), per batch.
    private int mSuccessfulImageDecodes;

    // The number of runtime failures during image decoding (not video), per batch.
    private int mFailedImageDecodesRuntime;

    // The number of out of memory failures during image decoding (not video), per batch.
    private int mFailedImageDecodesMemory;

    // The number of successful video decodes, per batch.
    private int mSuccessfulVideoDecodes;

    // The number of file errors during video decoding, per batch.
    private int mFailedVideoDecodesFile;

    // The number of runtime failures during video decoding, per batch.
    private int mFailedVideoDecodesRuntime;

    // The number of io failures during video decoding, per batch.
    private int mFailedVideoDecodesIo;

    // The number of io failures during video decoding, per batch.
    private int mFailedVideoDecodesUnknown;

    // A worker task for asynchronously handling video decode requests.
    private DecodeVideoTask mWorkerTask;

    // The current processing request.
    private DecoderServiceParams mProcessingRequest;

    // The callbacks used to notify the clients when the service is ready.
    private final List<DecoderStatusCallback> mCallbacks = new ArrayList<DecoderStatusCallback>();

    // Keeps track of the last decoding ordinal issued.
    static int sLastDecodingOrdinal = 0;

    // A callback to use for testing to see if decoder is ready.
    static DecoderStatusCallback sStatusCallbackForTesting;

    // Used to create intents for launching the {@link DecoderService} service.
    private static Supplier<Intent> sIntentSupplier;

    /**
     * Sets a factory for creating intents that launch the {@link DecoderService} service. This must
     * be called prior to using the PhotoPicker.
     *
     * @param intentSupplier a factory that creates a new Intent. Will be called every time the
     *     PhotoPicker is launched.
     */
    public static void setIntentSupplier(@NonNull Supplier<Intent> intentSupplier) {
        sIntentSupplier = intentSupplier;
    }

    // This is true after {#link bindService()} has been called for {@link mConnection}. It
    // indicates that {@link unbindService()} should be called.
    private boolean mBindServiceCalled;
    IDecoderService mIRemoteService;
    private ServiceConnection mConnection =
            new ServiceConnection() {
                @Override
                public void onServiceConnected(ComponentName className, IBinder service) {
                    mIRemoteService = IDecoderService.Stub.asInterface(service);
                    assert mIRemoteService != null;
                    for (DecoderStatusCallback callback : mCallbacks) {
                        callback.serviceReady();
                    }
                }

                @Override
                public void onServiceDisconnected(ComponentName className) {
                    Log.e(TAG, "Service has unexpectedly disconnected");
                    mIRemoteService = null;
                }
            };

    /** Interface for notifying clients of status of the decoder service. */
    public interface DecoderStatusCallback {
        /** A function to define to receive a notification once the service is up and running. */
        void serviceReady();

        /** Called when the decoder is idle. */
        void decoderIdle();
    }

    /** An interface notifying clients when all images have finished decoding. */
    public interface ImagesDecodedCallback {
        /**
         * A function to define to receive a notification that an image has been decoded.
         *
         * @param filePath The file path for the newly decoded image.
         * @param isVideo Whether the decoding was from a video or not.
         * @param fullWidth Whether the image is using the full width of the screen.
         * @param bitmaps The results of the decoding (or placeholder image, if failed).
         * @param videoDuration The time-length of the video (null if not a video).
         */
        void imagesDecodedCallback(
                String filePath,
                boolean isVideo,
                boolean fullWidth,
                List<Bitmap> bitmaps,
                String videoDuration,
                float ratio);
    }

    /** Class for keeping track of the data involved with each request. */
    protected static class DecoderServiceParams {
        // The URI for the file containing the bitmap to decode.
        final Uri mUri;

        // The requested width of the bitmap, once decoded.
        final int mWidth;

        // Whether this is image is taking up the full width of the screen.
        final boolean mFullWidth;

        // The type of media being decoded.
        @PickerBitmap.TileTypes final int mFileType;

        // Whether this is a request to decode only the first frame of a video. This field is
        // ignored for non-videos.
        final boolean mFirstFrame;

        // An ordinal used to enforce FIFO decoding, in the case where all other things are equal
        // (when it comes to determining which record to decode first).
        private int mRequestOrdinal;

        // The callback to use to communicate the results of the decoding.
        final ImagesDecodedCallback mCallback;

        // The timestamp for when the request was sent for decoding.
        long mTimestamp;

        public DecoderServiceParams(
                Uri uri,
                int width,
                boolean fullWidth,
                @PickerBitmap.TileTypes int fileType,
                boolean firstFrame,
                ImagesDecodedCallback callback) {
            mUri = uri;
            mWidth = width;
            mFullWidth = fullWidth;
            mFileType = fileType;
            mFirstFrame = firstFrame;
            mRequestOrdinal = sLastDecodingOrdinal++;
            mCallback = callback;
        }
    }

    // A request comparator that assign priority in the following way:
    // Top priority: Still images (TileType.PICTURES).
    // Medium priority: Videos (decoding of first frame only).
    // Low priority: Videos (multiple frames for animating).
    protected Comparator<DecoderServiceParams> mRequestComparator =
            (r1, r2) -> {
                if (r1.mFileType != r2.mFileType) {
                    return r1.mFileType - r2.mFileType;
                }

                // If they are both video types, then first frame decoding has priority.
                if (r1.mFileType == PickerBitmap.TileTypes.VIDEO
                        && r1.mFirstFrame != r2.mFirstFrame) {
                    return r1.mFirstFrame ? -1 : 1;
                }

                // The two requests share the same file type, or are identical video requests (both
                // requesting first frame or both requesting additional frames) so they can be
                // considered equal. Go with first in first out.
                return r1.mRequestOrdinal - r2.mRequestOrdinal;
            };

    // A queue of pending requests.
    PriorityQueue<DecoderServiceParams> mPendingRequests =
            new PriorityQueue<>(/* initialCapacity= */ 1, mRequestComparator);

    /**
     * The DecoderServiceHost constructor.
     *
     * @param callback The callback to use when communicating back to the client.
     * @param context The current context.
     */
    public DecoderServiceHost(DecoderStatusCallback callback, Context context) {
        mCallbacks.add(callback);
        if (sStatusCallbackForTesting != null) {
            mCallbacks.add(sStatusCallbackForTesting);
        }
        mContext = context;
        mContentResolver = mContext.getContentResolver();
    }

    /** Initiate binding with the {@link DecoderService}. */
    public void bind() {
        Intent intent = sIntentSupplier.get();
        intent.setAction(IDecoderService.class.getName());
        mContext.bindService(intent, mConnection, Context.BIND_AUTO_CREATE);
        mBindServiceCalled = true;
    }

    /** Unbind from the {@link DecoderService}. */
    public void unbind() {
        if (mBindServiceCalled) mContext.unbindService(mConnection);
        mBindServiceCalled = false;
    }

    /**
     * Accepts a request to decode a single image. Queues up the request and reports back
     * asynchronously on |callback|.
     *
     * @param uri The URI of the file to decode.
     * @param fileType The type of image being sent for decoding.
     * @param width The requested size (width and height) of the resulting bitmap.
     * @param fullWidth Whether the image is using the full width of the screen.
     * @param callback The callback to use to communicate the decoding results.
     */
    public void decodeImage(
            Uri uri,
            @PickerBitmap.TileTypes int fileType,
            int width,
            boolean fullWidth,
            ImagesDecodedCallback callback) {
        ThreadUtils.assertOnUiThread();

        DecoderServiceParams params =
                new DecoderServiceParams(
                        uri, width, fullWidth, fileType, /* firstFrame= */ true, callback);
        mPendingRequests.add(params);

        if (mProcessingRequest == null) dispatchNextDecodeRequest();
    }

    /**
     * Fetches the next decoding request from the queue (and removes it from the queue). Note: Still
     * images are preferred over videos (if available) because they are both faster to complete
     * decoding and (arguably) also more likely to be shared by the user.
     *
     * @return Next pending request (of highest priority).
     */
    private DecoderServiceParams getNextPending() {
        if (mPendingRequests.isEmpty()) {
            return null;
        }

        return mPendingRequests.remove();
    }

    /** Dispatches the next image/video for decoding (from the queue). */
    private void dispatchNextDecodeRequest() {
        // A new decoding request should not be dispatched while something is already in progress.
        assert mProcessingRequest == null;
        mProcessingRequest = getNextPending();
        if (mProcessingRequest != null) {
            mProcessingRequest.mTimestamp = SystemClock.elapsedRealtime();
            if (mProcessingRequest.mFileType != PickerBitmap.TileTypes.VIDEO) {
                dispatchDecodeImageRequest(mProcessingRequest);
            } else {
                dispatchDecodeVideoRequest(mProcessingRequest, mProcessingRequest.mFirstFrame);
            }
            return;
        }

        int totalImageRequests =
                mSuccessfulImageDecodes + mFailedImageDecodesRuntime + mFailedImageDecodesMemory;
        if (totalImageRequests > 0) {
            // Calculate and transmit UMA for image decoding.
            int runtimeFailures = 100 * mFailedImageDecodesRuntime / totalImageRequests;
            RecordHistogram.recordPercentageHistogram(
                    "Android.PhotoPicker.DecoderHostFailureRuntime", runtimeFailures);

            int memoryFailures = 100 * mFailedImageDecodesMemory / totalImageRequests;
            RecordHistogram.recordPercentageHistogram(
                    "Android.PhotoPicker.DecoderHostFailureOutOfMemory", memoryFailures);

            mSuccessfulImageDecodes = 0;
            mFailedImageDecodesRuntime = 0;
            mFailedImageDecodesMemory = 0;
        }

        int totalVideoRequests =
                mSuccessfulVideoDecodes
                        + mFailedVideoDecodesFile
                        + mFailedVideoDecodesRuntime
                        + mFailedVideoDecodesIo
                        + mFailedVideoDecodesUnknown;
        if (totalVideoRequests > 0) {
            // Calculate and transmit UMA for video decoding.
            int videoFileFailures = 100 * mFailedVideoDecodesFile / totalVideoRequests;
            RecordHistogram.recordPercentageHistogram(
                    "Android.PhotoPicker.DecoderHostVideoFileError", videoFileFailures);

            int videoRuntimeFailures = 100 * mFailedVideoDecodesRuntime / totalVideoRequests;
            RecordHistogram.recordPercentageHistogram(
                    "Android.PhotoPicker.DecoderHostVideoRuntimeError", videoRuntimeFailures);

            int videoIoFailures = 100 * mFailedVideoDecodesIo / totalVideoRequests;
            RecordHistogram.recordPercentageHistogram(
                    "Android.PhotoPicker.DecoderHostVideoIoError", videoIoFailures);

            int videoUnknownFailures = 100 * mFailedVideoDecodesUnknown / totalVideoRequests;
            RecordHistogram.recordPercentageHistogram(
                    "Android.PhotoPicker.DecoderHostVideoUnknownError", videoUnknownFailures);

            mSuccessfulVideoDecodes = 0;
            mFailedVideoDecodesFile = 0;
            mFailedVideoDecodesRuntime = 0;
            mFailedVideoDecodesIo = 0;
            mFailedVideoDecodesUnknown = 0;
        }

        for (DecoderStatusCallback callback : mCallbacks) {
            callback.decoderIdle();
        }
    }

    /**
     * A callback that receives the results of the video decoding.
     *
     * @param uri The uri of the decoded video.
     * @param bitmaps The thumbnails representing the decoded video.
     * @param duration The video duration (a formatted human-readable string, for example "3:00").
     * @param fullWidth Whether the image is using the full width of the screen.
     * @param decodingResult Whether the decoding was successful.
     * @param ratio The ratio of the first decoded frame in the video (>1.0=portrait,
     *     <1.0=landscape).
     */
    @Override
    public void videoDecodedCallback(
            Uri uri,
            List<Bitmap> bitmaps,
            String duration,
            boolean fullWidth,
            @DecodeVideoTask.DecodingResult int decodingResult,
            float ratio) {
        switch (decodingResult) {
            case DecodeVideoTask.DecodingResult.SUCCESS:
                if (bitmaps == null || bitmaps.size() == 0) {
                    mFailedVideoDecodesUnknown++;
                } else {
                    mSuccessfulVideoDecodes++;
                }
                break;
            case DecodeVideoTask.DecodingResult.FILE_ERROR:
                mFailedVideoDecodesFile++;
                break;
            case DecodeVideoTask.DecodingResult.RUNTIME_ERROR:
                mFailedVideoDecodesRuntime++;
                break;
            case DecodeVideoTask.DecodingResult.IO_ERROR:
                mFailedVideoDecodesIo++;
                break;
        }

        closeRequest(uri.getPath(), true, fullWidth, bitmaps, duration, -1, ratio);
    }

    @Override
    public void onDecodeImageDone(final Bundle payload) {
        // As per the Android documentation, AIDL callbacks can (and will) happen on any thread, so
        // make sure the code runs on the UI thread, since further down the callchain the code will
        // end up creating UI objects.
        PostTask.runOrPostTask(
                TaskTraits.UI_DEFAULT,
                () -> {
                    String filePath = "";
                    List<Bitmap> bitmaps = null;
                    Boolean fullWidth = false;
                    float ratio = 0;
                    long decodeTime = -1;
                    try {
                        // Read the reply back from the service.
                        filePath = payload.getString(ImageDecoder.KEY_FILE_PATH);
                        Boolean success = payload.getBoolean(ImageDecoder.KEY_SUCCESS);
                        Bitmap bitmap =
                                success
                                        ? (Bitmap)
                                                payload.getParcelable(ImageDecoder.KEY_IMAGE_BITMAP)
                                        : null;
                        ratio = payload.getFloat(ImageDecoder.KEY_RATIO);
                        decodeTime = payload.getLong(ImageDecoder.KEY_DECODE_TIME);
                        fullWidth = payload.getBoolean(ImageDecoder.KEY_FULL_WIDTH);
                        mSuccessfulImageDecodes++;
                        bitmaps = new ArrayList<>(1);
                        bitmaps.add(bitmap);
                    } catch (RuntimeException e) {
                        mFailedImageDecodesRuntime++;
                    } catch (OutOfMemoryError e) {
                        mFailedImageDecodesMemory++;
                    } finally {
                        closeRequest(
                                filePath,
                                /* isVideo= */ false,
                                fullWidth,
                                bitmaps,
                                /* videoDuration= */ null,
                                decodeTime,
                                ratio);
                    }
                });
    }

    private void closeRequestWithError(String filePath) {
        closeRequest(filePath, false, false, null, null, -1, 1.0f);
    }

    /**
     * Ties up all the loose ends from the decoding request (communicates the results of the
     * decoding process back to the client, and takes care of house-keeping chores regarding the
     * request queue).
     *
     * @param filePath The path to the image that was just decoded.
     * @param isVideo True if the request was for video decoding.
     * @param fullWidth Whether the image is using the full width of the screen.
     * @param bitmaps The resulting decoded bitmaps, or null if decoding fails.
     * @param decodeTime The length of time it took to decode the bitmaps.
     * @param ratio The ratio of the images (>1.0=portrait, <1.0=landscape).
     */
    private void closeRequest(
            String filePath,
            boolean isVideo,
            boolean fullWidth,
            @Nullable List<Bitmap> bitmaps,
            String videoDuration,
            long decodeTime,
            float ratio) {
        // If this assert triggers, it means that simultaneous requests have been sent for
        // decoding, which should not happen.
        assert mProcessingRequest.mUri.getPath().equals(filePath);
        long endRpcCall = SystemClock.elapsedRealtime();
        if (isVideo && bitmaps != null) {
            if (bitmaps != null && bitmaps.size() > 1) {
                RecordHistogram.recordTimesHistogram(
                        "Android.PhotoPicker.RequestProcessTimeAnimation",
                        endRpcCall - mProcessingRequest.mTimestamp);
            } else {
                RecordHistogram.recordTimesHistogram(
                        "Android.PhotoPicker.RequestProcessTimeThumbnail",
                        endRpcCall - mProcessingRequest.mTimestamp);
            }
        } else {
            RecordHistogram.recordTimesHistogram(
                    "Android.PhotoPicker.RequestProcessTime",
                    endRpcCall - mProcessingRequest.mTimestamp);
        }

        mProcessingRequest.mCallback.imagesDecodedCallback(
                filePath, isVideo, fullWidth, bitmaps, videoDuration, ratio);

        if (decodeTime != -1 && bitmaps != null && bitmaps.get(0) != null) {
            int sizeInKB = bitmaps.get(0).getByteCount() / ConversionUtils.BYTES_PER_KILOBYTE;
            if (isVideo) {
                if (bitmaps.size() > 1) {
                    RecordHistogram.recordTimesHistogram(
                            "Android.PhotoPicker.VideoDecodeTimeAnimation", decodeTime);
                } else {
                    RecordHistogram.recordTimesHistogram(
                            "Android.PhotoPicker.VideoDecodeTimeThumbnail", decodeTime);
                    RecordHistogram.recordCustomCountHistogram(
                            "Android.PhotoPicker.VideoByteCount", sizeInKB, 1, 100000, 50);
                }
            } else {
                RecordHistogram.recordTimesHistogram(
                        "Android.PhotoPicker.ImageDecodeTime", decodeTime);
                RecordHistogram.recordCustomCountHistogram(
                        "Android.PhotoPicker.ImageByteCount", sizeInKB, 1, 100000, 50);
            }
        }
        mProcessingRequest = null;

        dispatchNextDecodeRequest();
    }

    /**
     * Communicates with the utility process to decode a single video.
     *
     * @param params The information about the decoding request.
     * @param firstFrame True if the decoding request is for the first frame only.
     */
    private void dispatchDecodeVideoRequest(DecoderServiceParams params, boolean firstFrame) {
        // Videos are decoded by the system (on O+) using a restricted helper process, so
        // there's no need to use our custom sandboxed process.
        assert Build.VERSION.SDK_INT >= Build.VERSION_CODES.O;

        int frames = firstFrame ? 1 : 10;
        int intervalMs = 2000;
        mWorkerTask =
                new DecodeVideoTask(
                        this,
                        mContentResolver,
                        params.mUri,
                        params.mWidth,
                        params.mFullWidth,
                        frames,
                        intervalMs);
        mWorkerTask.executeOnExecutor(AsyncTask.THREAD_POOL_EXECUTOR);
    }

    /**
     * Communicates with the server to decode a single bitmap.
     *
     * @param params The information about the decoding request.
     */
    private void dispatchDecodeImageRequest(DecoderServiceParams params) {
        if (mIRemoteService == null) {
            // If the connection is lost, ignore the request and continue. Further still image
            // decoding requests will likely be dropped but note that there may be video requests
            // remaining (which don't require this connection to be open).
            Log.e(TAG, "Connection to decoder service unexpectedly terminated.");
            closeRequestWithError(mProcessingRequest.mUri.getPath());
            return;
        }

        // Obtain a file descriptor to send over to the sandboxed process.
        ParcelFileDescriptor pfd = null;
        Bundle bundle = new Bundle();

        // The restricted utility process can't open the file to read the
        // contents, so we need to obtain a file descriptor to pass over.
        try (StrictModeContext ignored = StrictModeContext.allowAllThreadPolicies()) {
            AssetFileDescriptor afd = null;
            try {
                afd = mContentResolver.openAssetFileDescriptor(params.mUri, "r");
            } catch (Exception e) {
                // FileNotFoundException, IllegalStateException, IllegalArgumentException.
                Log.e(TAG, "Unable to obtain FileDescriptor", e);
                closeRequestWithError(params.mUri.getPath());
                return;
            }
            pfd = afd.getParcelFileDescriptor();
            if (pfd == null) {
                closeRequestWithError(params.mUri.getPath());
                return;
            }
        }

        // Prepare and send the data over.
        bundle.putString(ImageDecoder.KEY_FILE_PATH, params.mUri.getPath());
        bundle.putParcelable(ImageDecoder.KEY_FILE_DESCRIPTOR, pfd);
        bundle.putInt(ImageDecoder.KEY_WIDTH, params.mWidth);
        bundle.putBoolean(ImageDecoder.KEY_FULL_WIDTH, params.mFullWidth);
        try {
            mIRemoteService.decodeImage(bundle, this);
        } catch (Exception e) {
            // RemoteException, IOException.
            Log.e(TAG, "IPC Failed", e);
            closeRequestWithError(params.mUri.getPath());
        }
        StreamUtil.closeQuietly(pfd);
    }

    /**
     * Cancels a request to decode an image (if it hasn't already been dispatched).
     *
     * @param filePath The path to the image to cancel decoding.
     */
    public void cancelDecodeImage(String filePath) {
        ThreadUtils.assertOnUiThread();

        // It is important not to null out only pending requests and not mProcessingRequest, because
        // it is used as a signal to see if the decoder is busy.
        Iterator it = mPendingRequests.iterator();
        while (it.hasNext()) {
            DecoderServiceParams param = (DecoderServiceParams) it.next();
            if (param.mUri.getPath().equals(filePath)) it.remove();
        }
    }

    /** Sets a callback to use when the service is ready. For testing use only. */
    @VisibleForTesting
    public static void setStatusCallback(DecoderStatusCallback callback) {
        sStatusCallbackForTesting = callback;
    }
}
