// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.video_tutorials.list;

import android.content.Context;
import android.graphics.drawable.BitmapDrawable;
import android.graphics.drawable.Drawable;

import org.chromium.base.Callback;
import org.chromium.chrome.browser.image_fetcher.ImageFetcher;
import org.chromium.chrome.browser.video_tutorials.Tutorial;
import org.chromium.chrome.browser.video_tutorials.VideoTutorialService;
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
        return new PropertyModel.Builder(TutorialCardProperties.ALL_KEYS)
                .with(TutorialCardProperties.TITLE, tutorial.displayTitle)
                // TODO(shaktisahu): Provide a string in mm:ss format.
                .with(TutorialCardProperties.VIDEO_LENGTH, Integer.toString(tutorial.videoLength))
                .with(TutorialCardProperties.CLICK_CALLBACK,
                        () -> mClickCallback.onResult(tutorial))
                .with(TutorialCardProperties.VISUALS_PROVIDER,
                        callback -> getBitmap(tutorial, callback))
                .build();
    }

    private void getBitmap(Tutorial tutorial, Callback<Drawable> callback) {
        ImageFetcher.Params params = ImageFetcher.Params.create(
                tutorial.posterUrl, ImageFetcher.VIDEO_TUTORIALS_LIST_UMA_CLIENT_NAME);
        mImageFetcher.fetchImage(params, bitmap -> {
            Drawable drawable = new BitmapDrawable(mContext.getResources(), bitmap);
            callback.onResult(drawable);
        });
    }
}