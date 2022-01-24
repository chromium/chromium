// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.components.paintpreview.player.frame;

import android.graphics.Bitmap;
import android.graphics.BitmapFactory;
import android.graphics.Canvas;
import android.graphics.ColorMatrixColorFilter;
import android.graphics.Paint;
import android.graphics.PorterDuff.Mode;
import android.graphics.PorterDuffXfermode;

import androidx.annotation.VisibleForTesting;

import org.chromium.base.Callback;
import org.chromium.base.TraceEvent;
import org.chromium.base.task.SequencedTaskRunner;

import java.io.ByteArrayOutputStream;
import java.nio.ByteBuffer;
import java.util.Arrays;
import java.util.concurrent.atomic.AtomicBoolean;
import java.util.zip.Deflater;
import java.util.zip.Inflater;

/**
 * A class representing a {@link Bitmap} that can be compressed into the associated byte array.
 * When compressed the {@link Bitmap} can safely be discarded and restored from the compressed
 * version. Compressing the bitmap is preferred for all bitmaps outside the current viewport.
 */
class CompressibleBitmap {
    private static final int IN_USE_BACKOFF_MS = 50;

    // For some reason this doesn't work if there isn't a color in one of the channels. As a result
    // we need to transform alpha to also apply a solid color like red.
    private static final ColorMatrixColorFilter sAlphaFilter = new ColorMatrixColorFilter(
            new float[] {0, 0, 0, 1, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 1, 0});

    private Bitmap mBitmap;
    private int mWidth;
    private int mHeight;
    private boolean mIgnoreMissingAlpha;

    // Compression by this class achieves a compression ratio of about 20.

    // Compressed as a JPEG.
    private byte[] mCompressedData;
    // Compressed with zip.
    @VisibleForTesting
    byte[] mCompressedAlphaBytes;
    private SequencedTaskRunner mTaskRunner;
    private AtomicBoolean mInUse = new AtomicBoolean();

    /**
     * Creates a new compressible bitmap which starts to compress immediately.
     * @param bitmap The bitmap to store.
     * @param taskRunner The task runner to compress/inflate the bitmap on.
     * @param visible Whether the bitmap is currently visible. If visible, the bitmap won't be
     *     immediately discarded.
     */
    CompressibleBitmap(Bitmap bitmap, SequencedTaskRunner taskRunner, boolean visible,
            boolean shouldCompress) {
        mBitmap = bitmap;
        mWidth = mBitmap.getWidth();
        mHeight = mBitmap.getHeight();
        mIgnoreMissingAlpha = false;
        // The alpha flag isn't always set even though it should be as the input bitmap is
        // ARGB8888 AKA N32Premultiplied.
        mBitmap.setHasAlpha(true);
        mTaskRunner = taskRunner;
        if (shouldCompress) {
            compressInBackground(visible);
        }
    }

    /**
     * Permits missing alpha channel to be ignored when inflating. If this is set to true, the
     * compressed JPEG will be used without alpha. This will result in black or white backing
     * to transparent/translucent pixels. Default is false causing the inflation to fail.
     * @param shouldIgnore whether to ignore missing alpha channel on inflation.
     */
    void setIgnoreMissingAlphaForTesting(boolean shouldIgnore) {
        mIgnoreMissingAlpha = shouldIgnore;
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
        mTaskRunner.postTask(() -> { destroyInternal(false); });
    }

    /**
     * Destroys the data associated with this bitmap ignoring any locks.
     */
    void forceDestroy() {
        mTaskRunner.postTask(() -> { destroyInternal(true); });
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
            TraceEvent.begin("CompressibleBitmap.inflate");
            inflate();
            TraceEvent.end("CompressibleBitmap.inflate");
            if (onInflated != null) {
                onInflated.onResult(this);
            }
        });
    }

    private boolean inflate() {
        if (mBitmap != null) return true;

        if (mCompressedData == null) return false;

        BitmapFactory.Options options = new BitmapFactory.Options();
        options.inMutable = true;
        options.inPreferredConfig = Bitmap.Config.ARGB_8888;
        mBitmap =
                BitmapFactory.decodeByteArray(mCompressedData, 0, mCompressedData.length, options);
        if (mBitmap == null) return false;

        // Decompress the alpha channel and apply the alpha mask.
        Bitmap alphaChannel = decompressAlpha(mCompressedAlphaBytes, mWidth, mHeight);
        if (alphaChannel != null) {
            // The alpha flag isn't set by default despite requesting ARGB_8888.Set it only if alpha
            // inflation succeeded.
            mBitmap.setHasAlpha(true);
            applyAlpha(mBitmap, alphaChannel);
            alphaChannel.recycle();

            // The bitmap must be premultiplied if alpha is true and is ARGB_8888.
            if (!mBitmap.isPremultiplied()) {
                mBitmap.recycle();
                mBitmap = null;
                return false;
            }
        } else if (!mIgnoreMissingAlpha) {
            // Abort if alpha inflation failed and we ignoring it is unacceptable.
            mBitmap.recycle();
            mBitmap = null;
            return false;
        }
        return true;
    }

    private void compress() {
        if (mBitmap == null) return;

        Bitmap alphaChannel = mBitmap.extractAlpha();
        // Bitmap#compress() doesn't work for Bitmap.Config.ALPHA_8 so use zip instead.
        mCompressedAlphaBytes = compressAlpha(alphaChannel);
        alphaChannel.recycle();

        ByteArrayOutputStream byteArrayStream = new ByteArrayOutputStream();
        boolean success = mBitmap.compress(Bitmap.CompressFormat.JPEG, 100, byteArrayStream);
        if (success) {
            mCompressedData = byteArrayStream.toByteArray();
        }
    }

    private void compressInBackground(boolean visible) {
        mTaskRunner.postTask(() -> {
            TraceEvent.begin("CompressibleBitmap.compress");
            compress();
            TraceEvent.end("CompressibleBitmap.compress");
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

    private void destroyInternal(boolean forceDestroy) {
        if (!lock() && !forceDestroy) {
            mTaskRunner.postDelayedTask(
                    () -> { destroyInternal(forceDestroy); }, IN_USE_BACKOFF_MS);
            return;
        }
        if (mBitmap != null) {
            mBitmap.recycle();
            mBitmap = null;
        }
        mCompressedData = null;
        mCompressedAlphaBytes = null;
        unlock();
    }

    private byte[] compressAlpha(Bitmap bitmap) {
        ByteBuffer byteBuffer = ByteBuffer.allocate(bitmap.getByteCount());
        bitmap.copyPixelsToBuffer(byteBuffer);

        Deflater deflater = new Deflater();
        deflater.setInput(byteBuffer.array());
        deflater.finish();

        ByteArrayOutputStream out = new ByteArrayOutputStream();
        byte[] buffer = new byte[8192]; // This limit is arbitrary.
        while (!deflater.finished()) {
            int byteCount = deflater.deflate(buffer);
            out.write(buffer, 0, byteCount);
        }
        deflater.end();

        return out.toByteArray();
    }

    private Bitmap decompressAlpha(byte[] alpha, int width, int height) {
        if (width == 0 || height == 0 || alpha.length == 0) return null;

        Inflater inflater = new Inflater();
        inflater.setInput(alpha, 0, alpha.length);

        Bitmap alphaBitmap = Bitmap.createBitmap(width, height, Bitmap.Config.ALPHA_8);
        if (alphaBitmap == null) return null;

        ByteBuffer byteBuffer = ByteBuffer.allocate(width * height);
        try {
            inflater.inflate(byteBuffer.array());
            alphaBitmap.copyPixelsFromBuffer(byteBuffer);
        } catch (Exception e) {
            // This can happen if the inflated content is the wrong size or the inflation fails.
            // This can happen if the device is under memory pressure or some sort of corruption
            // occurs. When this happens we should return a null bitmap.
            alphaBitmap.recycle();
            alphaBitmap = null;
        }
        inflater.end();
        return alphaBitmap;
    }

    private void applyAlpha(Bitmap bitmap, Bitmap alphaChannel) {
        Canvas c = new Canvas(bitmap);
        final Paint alphaPaint = new Paint(Paint.ANTI_ALIAS_FLAG);
        alphaPaint.setColorFilter(sAlphaFilter);
        alphaPaint.setXfermode(new PorterDuffXfermode(Mode.DST_IN));
        c.drawBitmap(alphaChannel, 0, 0, alphaPaint);
    }

    @Override
    public boolean equals(Object o) {
        if (o == this) return true;

        if (!(o instanceof CompressibleBitmap)) return false;

        CompressibleBitmap od = (CompressibleBitmap) o;

        if (mCompressedData != null && od.mCompressedData != null) {
            return Arrays.equals(mCompressedData, od.mCompressedData);
        }
        if (mCompressedAlphaBytes != null && od.mCompressedAlphaBytes != null) {
            return Arrays.equals(mCompressedAlphaBytes, od.mCompressedAlphaBytes);
        }
        if (mBitmap != null && od.mBitmap != null) {
            return mBitmap.equals(od.mBitmap);
        }
        return false;
    }
}
