// Copyright 2016 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.components.browser_ui.media;

import static org.chromium.build.NullUtil.assumeNonNull;

import android.graphics.Bitmap;
import android.graphics.Rect;

import androidx.annotation.VisibleForTesting;

import org.chromium.base.FileUtils;
import org.chromium.build.annotations.NullMarked;
import org.chromium.build.annotations.Nullable;
import org.chromium.content_public.browser.MediaSession;
import org.chromium.services.media_session.MediaImage;
import org.chromium.url.GURL;

import java.util.List;

/**
 * A class for managing the MediaImage download process.
 *
 * The manager takes a list of {@link MediaMetadata.MediaImage} as input, and
 * selects one of them based on scoring and start download through
 * {@link MediaSession} asynchronously. When the download successfully finishes,
 * the manager runs the callback function to notify the completion and pass the
 * downloaded Bitmap.
 *
 * The scoring works as follows:
 * - A image score is computed by multiplying the type score with the size score.
 * - The type score lies in [0, 1] and is based on the image MIME type/file extension.
 *   - PNG and JPEG are prefered than others.
 *   - If unspecified, use the default type score (0.6).
 * - The size score lies in [0, 1] and is computed by multiplying the dominant size score and aspect
 *   ratio score:
 *   - The dominant size score lies in [0, 1] and is computed using |mMinimumSize| and |mIdealSize|:
 *     - If size < |mMinimumSize| (too small), the size score is 0.
 *     - If |mMinimumSize| <= size <= |mIdealSize|, the score increases linearly from 0.2 to 1.
 *     - If size > |mIdealSize|, the score is |mIdealSize| / size, which drops from 1 to 0.
 *     - When the size is "any", the size score is 0.8.
 *     - If unspecified, use the default size score (0.4).
 *   - The aspect ratio score lies in [0, 1] and is computed by dividing the short edge length by
 *     the long edge.
 */
@NullMarked
public class MediaImageManager {
    // The default score of unknown image size.
    private static final double DEFAULT_IMAGE_SIZE_SCORE = 0.4;
    // The scores for different image types. Keep them sorted by value.
    private static final double TYPE_SCORE_DEFAULT = 0.6;
    private static final double TYPE_SCORE_PNG = 1.0;
    private static final double TYPE_SCORE_JPEG = 0.7;
    private static final double TYPE_SCORE_BMP = 0.5;
    private static final double TYPE_SCORE_XICON = 0.4;
    private static final double TYPE_SCORE_GIF = 0.3;

    private @Nullable MediaSession mMediaSession;
    // The minimum image size. Images that are smaller than |mMinimumSize| will be ignored.
    final int mMinimumSize;
    // The ideal image size. Images that are too large than |mIdealSize| will be ignored.
    final int mIdealSize;
    // The pending download image request id, which is incremented when calling
    // {@link MediaSession#getMediaImageBitmap()}, and reset when image download completes or
    // {@link #clearRequests()} is called.
    private int mCurrentRequestId;
    // The callback to be called when the pending download image request completes.
    private @Nullable MediaImageCallback mCallback;

    // The last image src for download, used for avoiding fetching the same src when artwork is set
    // multiple times but the same src is chosen.
    //
    // Will be reset when initiating a new download request.
    private @Nullable GURL mLastImageSrc;

    /**
     * MediaImageManager constructor.
     * @param minimumSize The minimum size of images to download.
     * @param idealSize The ideal size of images to download.
     */
    public MediaImageManager(int minimumSize, int idealSize) {
        mMinimumSize = minimumSize;
        mIdealSize = idealSize;
        clearRequests();
    }

    /**
     * Called when the MediaSession changes.
     *
     * @param mediaSession The new MediaSession.
     */
    public void setMediaSession(@Nullable MediaSession mediaSession) {
        mMediaSession = mediaSession;
        mLastImageSrc = null;
        clearRequests();
    }

    /**
     * Select the best image from |images| and start download.
     *
     * @param images The list of images to choose from. Null is equivalent to empty list.
     * @param callback The callback when image download completes.
     */
    public void downloadImage(@Nullable List<MediaImage> images, MediaImageCallback callback) {
        if (mMediaSession == null) return;

        mCallback = callback;
        MediaImage image = selectImage(images);
        if (image == null) {
            mLastImageSrc = null;
            mCallback.onImageDownloaded(null);
            clearRequests();
            return;
        }

        // Avoid fetching the same image twice.
        if (image.getSrc().equals(mLastImageSrc)) return;
        mLastImageSrc = image.getSrc();

        final int requestId = ++mCurrentRequestId;
        mMediaSession.getMediaImageBitmap(
                image,
                mMinimumSize,
                mIdealSize,
                (bitmap) -> onFinishDownloadImage(requestId, bitmap));
    }

    @VisibleForTesting
    void onFinishDownloadImage(int requestId, @Nullable Bitmap bitmap) {
        if (requestId != mCurrentRequestId) return;

        assumeNonNull(mCallback);
        mCallback.onImageDownloaded(bitmap);
        clearRequests();
    }

    /**
     * Select the best image from the |images|.
     *
     * @param images The list of images to select from. Null is equivalent to empty list.
     */
    private @Nullable MediaImage selectImage(@Nullable List<MediaImage> images) {
        if (images == null) return null;

        MediaImage selectedImage = null;
        double bestScore = 0;
        for (MediaImage image : images) {
            double newScore = getImageScore(image);
            if (newScore > bestScore) {
                bestScore = newScore;
                selectedImage = image;
            }
        }
        return selectedImage;
    }

    private void clearRequests() {
        mCurrentRequestId++;
        mCallback = null;
    }

    private double getImageScore(MediaImage image) {
        if (image == null) return 0;
        if (image.getSizes().isEmpty()) return DEFAULT_IMAGE_SIZE_SCORE;

        double bestSizeScore = 0;
        for (Rect size : image.getSizes()) {
            bestSizeScore = Math.max(bestSizeScore, getImageSizeScore(size));
        }
        double typeScore = getImageTypeScore(image.getSrc(), image.getType());
        return bestSizeScore * typeScore;
    }

    private double getImageSizeScore(Rect size) {
        return getImageDominantSizeScore(size.width(), size.height())
                * getImageAspectRatioScore(size.width(), size.height());
    }

    private double getImageDominantSizeScore(int width, int height) {
        int dominantSize = Math.max(width, height);
        // When the size is "any".
        if (dominantSize == 0) return 0.8;
        // Ignore images that are too small.
        if (dominantSize < mMinimumSize) return 0;

        if (dominantSize <= mIdealSize) {
            return 0.8 * (dominantSize - mMinimumSize) / (mIdealSize - mMinimumSize) + 0.2;
        }
        return 1.0 * mIdealSize / dominantSize;
    }

    private double getImageAspectRatioScore(int width, int height) {
        double longEdge = Math.max(width, height);
        double shortEdge = Math.min(width, height);
        return shortEdge / longEdge;
    }

    private double getImageTypeScore(GURL url, String type) {
        String extension = FileUtils.getExtension(url.getSpec());

        if ("bmp".equals(extension) || "image/bmp".equals(type)) {
            return TYPE_SCORE_BMP;
        } else if ("gif".equals(extension) || "image/gif".equals(type)) {
            return TYPE_SCORE_GIF;
        } else if ("icon".equals(extension) || "image/x-icon".equals(type)) {
            return TYPE_SCORE_XICON;
        } else if ("png".equals(extension) || "image/png".equals(type)) {
            return TYPE_SCORE_PNG;
        } else if ("jpeg".equals(extension)
                || "jpg".equals(extension)
                || "image/jpeg".equals(type)) {
            return TYPE_SCORE_JPEG;
        }
        return TYPE_SCORE_DEFAULT;
    }
}
