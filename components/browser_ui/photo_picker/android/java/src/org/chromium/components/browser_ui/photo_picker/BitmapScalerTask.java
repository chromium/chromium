// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.components.browser_ui.photo_picker;

import android.graphics.Bitmap;
import android.os.SystemClock;
import android.util.LruCache;

import org.chromium.base.ThreadUtils;
import org.chromium.base.metrics.RecordHistogram;
import org.chromium.base.task.AsyncTask;

import java.util.ArrayList;
import java.util.List;

/** A worker task to scale bitmaps in the background. */
class BitmapScalerTask extends AsyncTask<Bitmap> {
    private final LruCache<String, PickerCategoryView.Thumbnail> mCache;
    private final String mFilePath;
    private final int mSize;
    private final Bitmap mBitmap;
    private final String mVideoDuration;
    private final float mRatio;

    /** A BitmapScalerTask constructor. */
    public BitmapScalerTask(
            LruCache<String, PickerCategoryView.Thumbnail> cache,
            Bitmap bitmap,
            String filePath,
            String videoDuration,
            int size,
            float ratio) {
        mCache = cache;
        mFilePath = filePath;
        mSize = size;
        mBitmap = bitmap;
        mVideoDuration = videoDuration;
        mRatio = ratio;
    }

    /**
     * Scales the image provided. Called on a non-UI thread.
     *
     * @return A sorted list of images (by last-modified first).
     */
    @Override
    protected Bitmap doInBackground() {
        assert !ThreadUtils.runningOnUiThread();

        if (isCancelled()) return null;

        long begin = SystemClock.elapsedRealtime();
        Bitmap bitmap = BitmapUtils.scale(mBitmap, mSize, false);
        long scaleTime = SystemClock.elapsedRealtime() - begin;
        RecordHistogram.recordTimesHistogram("Android.PhotoPicker.BitmapScalerTask", scaleTime);
        return bitmap;
    }

    /**
     * Communicates the results back to the client. Called on the UI thread.
     *
     * @param bitmap The resulting scaled bitmap.
     */
    @Override
    protected void onPostExecute(Bitmap bitmap) {
        if (isCancelled()) {
            return;
        }

        List<Bitmap> bitmaps = new ArrayList<>(1);
        bitmaps.add(bitmap);
        mCache.put(
                mFilePath,
                new PickerCategoryView.Thumbnail(
                        bitmaps, mVideoDuration, /* fullWidth= */ false, mRatio));
    }
}
