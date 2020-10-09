// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.components.paintpreview.player.frame;

import android.graphics.Bitmap;
import android.graphics.BitmapFactory;

import org.chromium.base.Callback;
import org.chromium.base.task.SequencedTaskRunner;

import java.io.ByteArrayOutputStream;
import java.util.Arrays;
import java.util.concurrent.atomic.AtomicBoolean;

/**
 * A class representing a {@link Bitmap} that can be compressed into the associated byte array.
 * When compressed the {@link Bitmap} can safely be discarded and restored from the compressed
 * version. Compressing the bitmap is preferred for all bitmaps outside the current viewport.
 */
class CompressibleBitmap {
    private static final int IN_USE_BACKOFF_MS = 50;

    private Bitmap mBitmap;
    private byte[] mCompressedData;
    private SequencedTaskRunner mTaskRunner;
    private AtomicBoolean mInUse = new AtomicBoolean();

    /**
     * Creates a new compressible bitmap which starts to compress immediately.
     * @param bitmap The bitmap to store.
     * @param taskRunner The task runner to compress/inflate the bitmap on.
     * @param visible Whether the bitmap is currently visible. If visible, the bitmap won't be
     *     immediately discarded.
     */
    CompressibleBitmap(Bitmap bitmap, SequencedTaskRunner taskRunner, boolean visible) {
        mBitmap = bitmap;
        mTaskRunner = taskRunner;
        compressInBackground(visible);
    }

    /**
     * Locks modifying {@link mBitmap} to prevent use/discard from happening in parallel.
     */
    boolean lock() {
        return mInUse.compareAndSet(false, true);
    }

    /**
     * Unlocks modifying of {@link mBitmap} so that it is available for use/discard by the next
     * thread that calls {@link lock()}.
     */
    boolean unlock() {
        return mInUse.compareAndSet(true, false);
    }

    /**
     * Gets the bitmap if one is inflated.
     * @return the bitmap or null if not inflated.
     */
    Bitmap getBitmap() {
        return mBitmap;
    }

    /**
     * Destroys the data associated with this bitmap.
     */
    void destroy() {
        mTaskRunner.postTask(this::destroyInternal);
    }

    /**
     * Discards the inflated bitmap if it has been successfully compressed.
     */
    void discardBitmap() {
        mTaskRunner.postTask(this::discardBitmapInternal);
    }

    /**
     * Inflates the compressed bitmap in the background. Call from the UI thread.
     * @param onInflated Callback that is called when inflation is completed on the UI Thread.
     *     Callers should check that the bitmap was actually inflated via {@link getBitmap()}.
     */
    void inflateInBackground(Callback<CompressibleBitmap> onInflated) {
        mTaskRunner.postTask(() -> {
            inflate();
            if (onInflated != null) {
                onInflated.onResult(this);
            }
        });
    }

    private boolean inflate() {
        if (mBitmap != null) return true;

        if (mCompressedData == null) return false;

        mBitmap = BitmapFactory.decodeByteArray(mCompressedData, 0, mCompressedData.length);
        return mBitmap != null;
    }

    private void compress() {
        if (mBitmap == null) return;

        ByteArrayOutputStream byteArrayStream = new ByteArrayOutputStream();
        boolean success = mBitmap.compress(Bitmap.CompressFormat.JPEG, 100, byteArrayStream);
        if (success) {
            mCompressedData = byteArrayStream.toByteArray();
        }
    }

    private void compressInBackground(boolean visible) {
        mTaskRunner.postTask(() -> {
            compress();
            if (visible) return;

            discardBitmapInternal();
        });
    }

    private void discardBitmapInternal() {
        if (!lock()) {
            mTaskRunner.postDelayedTask(this::discardBitmapInternal, IN_USE_BACKOFF_MS);
            return;
        }

        if (mBitmap != null && mCompressedData != null) {
            mBitmap.recycle();
            mBitmap = null;
        }
        unlock();
    }

    private void destroyInternal() {
        if (!lock()) {
            mTaskRunner.postDelayedTask(this::destroyInternal, IN_USE_BACKOFF_MS);
            return;
        }
        if (mBitmap != null) {
            mBitmap.recycle();
            mBitmap = null;
        }
        mCompressedData = null;
        unlock();
    }

    @Override
    public boolean equals(Object o) {
        if (o == this) return true;

        if (!(o instanceof CompressibleBitmap)) return false;

        CompressibleBitmap od = (CompressibleBitmap) o;

        if (mCompressedData != null && od.mCompressedData != null) {
            return Arrays.equals(mCompressedData, od.mCompressedData);
        }
        if (mBitmap != null && od.mBitmap != null) {
            return mBitmap.equals(od.mBitmap);
        }
        return false;
    }
}
