// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.components.browser_ui.photo_picker;

import static org.chromium.build.NullUtil.assumeNonNull;

import android.content.ContentResolver;
import android.content.res.AssetFileDescriptor;
import android.graphics.Bitmap;
import android.media.MediaMetadataRetriever;
import android.net.Uri;
import android.util.Pair;

import org.chromium.base.ThreadUtils;
import org.chromium.base.task.AsyncTask;
import org.chromium.build.annotations.NullMarked;
import org.chromium.build.annotations.Nullable;

import java.io.IOException;
import java.util.List;
import java.util.Locale;

/** A worker task to decode video and extract information from it off of the UI thread. */
@NullMarked
class DecodeVideoTask extends AsyncTask<@Nullable List<Bitmap>> {
    /** An interface to use to communicate back the results to the client. */
    public interface VideoDecodingCallback {
        /**
         * A callback to define to receive the list of all images on disk.
         *
         * @param uri The uri of the video decoded.
         * @param bitmaps An array of thumbnails extracted from the video.
         * @param duration The duration of the video.
         * @param fullWidth Whether the image is using the full width of the screen.
         * @param ratio The aspect ratio of the first frame of the video.
         */
        void videoDecodedCallback(
                Uri uri,
                @Nullable List<Bitmap> bitmaps,
                @Nullable String duration,
                boolean fullWidth,
                float ratio);
    }

    // The callback to use to communicate the results.
    private final VideoDecodingCallback mCallback;

    // The URI of the video to decode.
    private final Uri mUri;

    // The desired width and height (in pixels) of the returned thumbnail from the video.
    int mSize;

    // Whether the image is taking up the full width of the screen.
    boolean mFullWidth;

    // The number of frames to extract.
    int mFrames;

    // The interval between frames (in milliseconds).
    long mIntervalMs;

    // The ContentResolver to use to retrieve image metadata from disk.
    private final ContentResolver mContentResolver;

    // The duration of the video.
    private @Nullable String mDuration;

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
    public static @Nullable String formatDuration(Long durationMs) {
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
    protected @Nullable List<Bitmap> doInBackground() {
        assert !ThreadUtils.runningOnUiThread();

        if (isCancelled()) return null;

        try (AssetFileDescriptor afd = mContentResolver.openAssetFileDescriptor(mUri, "r");
                MediaMetadataRetriever retriever = new MediaMetadataRetriever()) {
            assumeNonNull(afd);
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
            return bitmaps.first;
        } catch (RuntimeException | IOException exception) {
            return null;
        }
    }

    /**
     * Communicates the results back to the client. Called on the UI thread.
     *
     * @param results A pair of bitmap (video thumbnail) and the duration of the video.
     */
    @Override
    protected void onPostExecute(@Nullable List<Bitmap> results) {
        if (isCancelled()) {
            return;
        }

        if (results == null) {
            mCallback.videoDecodedCallback(mUri, null, "", mFullWidth, 1.0f);
            return;
        }

        mCallback.videoDecodedCallback(mUri, results, mDuration, mFullWidth, mRatio);
    }
}
