// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.video_tutorials.list;

import android.content.Context;
import android.graphics.drawable.BitmapDrawable;
import android.graphics.drawable.Drawable;

import org.chromium.base.Callback;
import org.chromium.chrome.browser.video_tutorials.Tutorial;
import org.chromium.chrome.browser.video_tutorials.VideoTutorialService;
import org.chromium.chrome.browser.video_tutorials.VideoTutorialUtils;
import org.chromium.chrome.browser.video_tutorials.metrics.VideoTutorialMetrics;
import org.chromium.chrome.browser.video_tutorials.metrics.VideoTutorialMetrics.UserAction;
import org.chromium.components.image_fetcher.ImageFetcher;
import org.chromium.ui.modelutil.MVCListAdapter;
import org.chromium.ui.modelutil.MVCListAdapter.ListItem;
import org.chromium.ui.modelutil.PropertyModel;

import java.util.List;

/**
 *  The mediator associated with the recycler view in the video tutorials home UI.
 */
public class TutorialListMediator {
    private final Context mContext;
    private final MVCListAdapter.ModelList mListModel;
    private final VideoTutorialService mVideoTutorialService;
    private final ImageFetcher mImageFetcher;
    private final Callback<Tutorial> mClickCallback;

    /**
     * Constructor.
     * @param context The activity context.
     * @param videoTutorialService The video tutorial service backend.
     */
    public TutorialListMediator(MVCListAdapter.ModelList listModel, Context context,
            VideoTutorialService videoTutorialService, ImageFetcher imageFetcher,
            Callback<Tutorial> clickCallback) {
        mListModel = listModel;
        mContext = context;
        mVideoTutorialService = videoTutorialService;
        mImageFetcher = imageFetcher;
        mClickCallback = clickCallback;
        videoTutorialService.getTutorials(this::populateList);
    }

    private void populateList(List<Tutorial> tutorials) {
        assert mListModel.size() == 0;
        for (Tutorial tutorial : tutorials) {
            ListItem listItem = new ListItem(TutorialCardProperties.VIDEO_TUTORIAL_CARD_VIEW_TYPE,
                    buildModelFromTutorial(tutorial));
            mListModel.add(listItem);
        }
    }

    private PropertyModel buildModelFromTutorial(Tutorial tutorial) {
        PropertyModel.Builder builder =
                new PropertyModel.Builder(TutorialCardProperties.ALL_KEYS)
                        .with(TutorialCardProperties.TITLE, tutorial.title)
                        .with(TutorialCardProperties.VIDEO_LENGTH,
                                VideoTutorialUtils.getVideoLengthString(tutorial.videoLength))
                        .with(TutorialCardProperties.CLICK_CALLBACK, () -> onCardClicked(tutorial));

        builder.with(TutorialCardProperties.VISUALS_PROVIDER, (consumer, widthPx, heightPx) -> {
            fetchImage(consumer, widthPx, heightPx, tutorial);
            return () -> {};
        });
        return builder.build();
    }

    private void onCardClicked(Tutorial tutorial) {
        VideoTutorialMetrics.recordUserAction(tutorial.featureType, UserAction.PLAYED_FROM_RECAP);
        mClickCallback.onResult(tutorial);
    }

    private void fetchImage(
            Callback<Drawable> consumer, int widthPx, int heightPx, Tutorial tutorial) {
        ImageFetcher.Params params = ImageFetcher.Params.create(tutorial.thumbnailUrl,
                ImageFetcher.VIDEO_TUTORIALS_LIST_UMA_CLIENT_NAME, widthPx, heightPx);
        mImageFetcher.fetchImage(params, bitmap -> {
            Drawable drawable = new BitmapDrawable(mContext.getResources(), bitmap);
            consumer.onResult(drawable);
        });
    }
}