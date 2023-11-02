// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.video_tutorials.iph;

import android.content.Context;
import android.graphics.Bitmap.Config;
import android.graphics.drawable.BitmapDrawable;
import android.graphics.drawable.Drawable;
import android.view.ViewStub;

import org.chromium.base.Callback;
import org.chromium.chrome.browser.flags.ChromeFeatureList;
import org.chromium.chrome.browser.video_tutorials.FeatureType;
import org.chromium.chrome.browser.video_tutorials.Tutorial;
import org.chromium.chrome.browser.video_tutorials.VideoTutorialUtils;
import org.chromium.components.image_fetcher.ImageFetcher;
import org.chromium.ui.modelutil.PropertyModel;
import org.chromium.ui.modelutil.PropertyModelChangeProcessor;

import jp.tomorrowkey.android.gifplayer.BaseGifDrawable;

/**
 * Creates and shows a video tutorial IPH. Requires a {@link ViewStub} to be passed which will
 * inflate when the IPH is shown.
 */
public class VideoIPHCoordinatorImpl implements VideoIPHCoordinator {
    private static final String VARIATION_USE_ANIMATED_GIF_URL = "use_animated_gif_url";
    private static final String VARIATION_USE_ANIMATED_GIF_URL_FOR_SUMMARY_CARD =
            "use_animated_gif_url_for_summary_card";

    private final Context mContext;
    private final PropertyModel mModel;
    private final VideoIPHView mView;
    private final ImageFetcher mImageFetcher;
    private final Callback<Tutorial> mOnClickListener;
    private final Callback<Tutorial> mOnDismissListener;

    /**
     * Constructor.
     * @param viewStub The view stub which will inflate to show the IPH.
     * @param imageFetcher The {@link ImageFetcher} to fetch thumbnail.
     * @param onClickListener The on click listener that starts playing IPH.
     * @param onDismissListener The listener to be invoked on dismissal.
     */
    public VideoIPHCoordinatorImpl(ViewStub viewStub, ImageFetcher imageFetcher,
            Callback<Tutorial> onClickListener, Callback<Tutorial> onDismissListener) {
        mContext = viewStub.getContext();
        mImageFetcher = imageFetcher;
        mOnClickListener = onClickListener;
        mOnDismissListener = onDismissListener;

        mModel = new PropertyModel(VideoIPHProperties.ALL_KEYS);
        mView = new VideoIPHView(viewStub);
        PropertyModelChangeProcessor.create(mModel, mView, VideoIPHView::bind);
    }

    @Override
    public void showVideoIPH(Tutorial tutorial) {
        mModel.set(VideoIPHProperties.VISIBILITY, true);
        mModel.set(VideoIPHProperties.DISPLAY_TITLE, tutorial.title);
        mModel.set(VideoIPHProperties.VIDEO_LENGTH,
                VideoTutorialUtils.getVideoLengthString(tutorial.videoLength));
        mModel.set(VideoIPHProperties.SHOW_VIDEO_LENGTH, tutorial.videoLength != 0);
        mModel.set(VideoIPHProperties.CLICK_LISTENER, () -> mOnClickListener.onResult(tutorial));
        mModel.set(
                VideoIPHProperties.DISMISS_LISTENER, () -> mOnDismissListener.onResult(tutorial));

        mModel.set(VideoIPHProperties.THUMBNAIL_PROVIDER, (consumer, widthPx, heightPx) -> {
            fetchImage(consumer, widthPx, heightPx, tutorial);
            return () -> {};
        });
    }

    @Override
    public void hideVideoIPH() {
        mModel.set(VideoIPHProperties.VISIBILITY, false);
    }

    private void fetchImage(
            Callback<Drawable> consumer, int widthPx, int heightPx, Tutorial tutorial) {
        boolean isSummaryCard = tutorial.featureType == FeatureType.SUMMARY;
        boolean useAnimatedGifUrl = ChromeFeatureList.getFieldTrialParamByFeatureAsBoolean(
                ChromeFeatureList.VIDEO_TUTORIALS,
                isSummaryCard ? VARIATION_USE_ANIMATED_GIF_URL_FOR_SUMMARY_CARD
                              : VARIATION_USE_ANIMATED_GIF_URL,
                !isSummaryCard);
        ImageFetcher.Params params = ImageFetcher.Params.create(
                useAnimatedGifUrl ? tutorial.animatedGifUrl : tutorial.thumbnailUrl,
                ImageFetcher.VIDEO_TUTORIALS_IPH_UMA_CLIENT_NAME, widthPx, heightPx);
        if (useAnimatedGifUrl) {
            mImageFetcher.fetchGif(params, gifImage -> {
                BaseGifDrawable baseGifDrawable =
                        gifImage == null ? null : new BaseGifDrawable(gifImage, Config.ARGB_8888);
                consumer.onResult(baseGifDrawable);
            });
        } else {
            mImageFetcher.fetchImage(params, bitmap -> {
                consumer.onResult(new BitmapDrawable(mContext.getResources(), bitmap));
            });
        }
    }
}