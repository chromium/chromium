// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.components.browser_ui.photo_picker;

import android.content.ContentResolver;
import android.content.res.AssetFileDescriptor;
import android.graphics.Bitmap;
import android.media.MediaMetadataRetriever;
import android.net.Uri;
import android.util.Pair;

import androidx.annotation.IntDef;

import org.chromium.base.ThreadUtils;
import org.chromium.base.task.AsyncTask;

import java.io.FileNotFoundException;
import java.io.IOException;
import java.lang.annotation.Retention;
import java.lang.annotation.RetentionPolicy;
import java.util.List;
import java.util.Locale;

/** A worker task to decode video and extract information from it off of the UI thread. */
class DecodeVideoTask extends AsyncTask<List<Bitmap>> {
    /** The possible error states while decoding. */
    @Retention(RetentionPolicy.SOURCE)
    @IntDef({
        DecodingResult.SUCCESS,
        DecodingResult.FILE_ERROR,
        DecodingResult.RUNTIME_ERROR,
        DecodingResult.IO_ERROR
    })
    public @interface DecodingResult {
        int SUCCESS = 0;
        int FILE_ERROR = 1;
        int RUNTIME_ERROR = 2;
        int IO_ERROR = 3;
    }

    /** An interface to use to communicate back the results to the client. */
    public interface VideoDecodingCallback {
        /**
         * A callback to define to receive the list of all images on disk.
         *
         * @param uri The uri of the video decoded.
         * @param bitmaps An array of thumbnails extracted from the video.
         * @param duration The duration of the video.
         * @param fullWidth Whether the image is using the full width of the screen.
         * @param decodingStatus Whether the decoding was successful.
         */
        void videoDecodedCallback(
                Uri uri,
                List<Bitmap> bitmaps,
                String duration,
                boolean fullWidth,
                @DecodingResult int decodingStatus,
                float ratio);
    }

    // The callback to use to communicate the results.
    private VideoDecodingCallback mCallback;

    // The URI of the video to decode.
    private Uri mUri;

    // The desired width and height (in pixels) of the returned thumbnail from the video.
    int mSize;

    // Whether the image is taking up the full width of the screen.
    boolean mFullWidth;

    // The number of frames to extract.
    int mFrames;

    // The interval between frames (in milliseconds).
    long mIntervalMs;

    // The ContentResolver to use to retrieve image metadata from disk.
    private ContentResolver mContentResolver;

    // Keeps track of errors during decoding.
    private @DecodingResult int mDecodingResult;

    // The duration of the video.
    private String mDuration;

    // The ratio of the first frame of the video.
    private float mRatio;

    /**
     * A DecodeVideoTask constructor.
     *
     * @param callback The callback to use to communicate back the results.
     * @param contentResolver The ContentResolver to use to retrieve image metadata from disk.
     * @param uri The URI of the video to decode.
     * @param size The desired width and height (in pixels) of the returned thumbnail from the
     *     video.
     * @param fullWidth Whether this is a video thumbnail that takes up the full screen width.
     * @param frames The number of frames to extract.
     * @param intervalMs The interval between frames (in milliseconds).
     */
    public DecodeVideoTask(
            VideoDecodingCallback callback,
            ContentResolver contentResolver,
            Uri uri,
            int size,
            boolean fullWidth,
            int frames,
            long intervalMs) {
        mCallback = callback;
        mContentResolver = contentResolver;
        mUri = uri;
        mSize = size;
        mFullWidth = fullWidth;
        mFrames = frames;
        mIntervalMs = intervalMs;
    }

    /**
     * Converts a duration string in ms to a human-readable form.
     *
     * @param durationMs The duration in milliseconds.
     * @return The duration in human-readable form.
     */
    public static String formatDuration(Long durationMs) {
        if (durationMs == null) return null;

        long duration = durationMs / 1000;
        long hours = duration / 3600;
        duration -= hours * 3600;
        long minutes = duration / 60;
        duration -= minutes * 60;
        long seconds = duration;
        if (hours > 0) {
            return String.format(Locale.US, "%d:%02d:%02d", hours, minutes, seconds);
        } else {
            return String.format(Locale.US, "%d:%02d", minutes, seconds);
        }
    }

    /**
     * Decodes a video and extracts metadata and a thumbnail. Called on a non-UI thread
     *
     * @return A list of bitmaps (video thumbnails).
     */
    @Override
    protected List<Bitmap> doInBackground() {
        assert !ThreadUtils.runningOnUiThread();

        if (isCancelled()) return null;

        // TODO(finnur): Apply try-with-resources to MediaMetadataRetriever once Chrome no longer
        //               supports versions below Build.VERSION_CODES.Q. API 29 is when it started
        //               to implement AutoCloseable:
        //
        // https://developer.android.com/sdk/api_diff/29/changes/android.media.MediaMetadataRetriever
        MediaMetadataRetriever retriever = null;
        try (AssetFileDescriptor afd = mContentResolver.openAssetFileDescriptor(mUri, "r")) {
            retriever = new MediaMetadataRetriever();
            retriever.setDataSource(afd.getFileDescriptor());
            String duration =
                    retriever.extractMetadata(MediaMetadataRetriever.METADATA_KEY_DURATION);
            if (duration != null) {
                // Adjust to a shorter video, if the frame requests exceed the length of the video.
                long durationMs = Long.parseLong(duration);
                if (mFrames > 1 && mFrames * mIntervalMs > durationMs) {
                    mIntervalMs = durationMs / mFrames;
                }
                duration = formatDuration(durationMs);
            }
            Pair<List<Bitmap>, Float> bitmaps =
                    BitmapUtils.decodeVideoFromFileDescriptor(
                            retriever,
                            afd.getFileDescriptor(),
                            mSize,
                            mFrames,
                            mFullWidth,
                            mIntervalMs);
            mDuration = duration;
            mRatio = bitmaps.second;
            mDecodingResult = DecodingResult.SUCCESS;
            return bitmaps.first;
        } catch (FileNotFoundException exception) {
            mDecodingResult = DecodingResult.FILE_ERROR;
            return null;
        } catch (RuntimeException exception) {
            mDecodingResult = DecodingResult.RUNTIME_ERROR;
            return null;
        } catch (IOException exception) {
            mDecodingResult = DecodingResult.IO_ERROR;
            return null;
        } finally {
            try {
                if (retriever != null) retriever.release();
            } catch (IOException exception) {
            }
        }
    }

    /**
     * Communicates the results back to the client. Called on the UI thread.
     *
     * @param results A pair of bitmap (video thumbnail) and the duration of the video.
     */
    @Override
    protected void onPostExecute(List<Bitmap> results) {
        if (isCancelled()) {
            return;
        }

        if (results == null) {
            mCallback.videoDecodedCallback(mUri, null, "", mFullWidth, mDecodingResult, 1.0f);
            return;
        }

        mCallback.videoDecodedCallback(
                mUri, results, mDuration, mFullWidth, mDecodingResult, mRatio);
    }
}
