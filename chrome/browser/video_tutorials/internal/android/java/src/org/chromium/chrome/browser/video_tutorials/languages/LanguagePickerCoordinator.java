// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.video_tutorials.languages;

import android.content.Context;
import android.view.View;

import org.chromium.chrome.browser.video_tutorials.FeatureType;
import org.chromium.chrome.browser.video_tutorials.LanguageInfoProvider;
import org.chromium.chrome.browser.video_tutorials.VideoTutorialService;
import org.chromium.ui.modelutil.MVCListAdapter.ModelList;
import org.chromium.ui.modelutil.PropertyModel;

/**
 *  The top level coordinator for the language picker UI.
 */
public class LanguagePickerCoordinator {
    private final Context mContext;
    private final VideoTutorialService mVideoTutorialService;
    private final LanguagePickerMediator mMediator;
    private final LanguagePickerView mView;
    private final PropertyModel mModel;
    private final ModelList mListModel;

    /**
     * Constructor.
     * @param view The view representing this language picker.
     * @param videoTutorialService The video tutorial service backend.
     */
    public LanguagePickerCoordinator(View view, VideoTutorialService videoTutorialService,
            LanguageInfoProvider languageInfoProvider) {
        mContext = view.getContext();
        mVideoTutorialService = videoTutorialService;
        mModel = new PropertyModel(LanguagePickerProperties.ALL_KEYS);
        mListModel = new ModelList();
        mView = new LanguagePickerView(view, mModel, mListModel);
        mMediator = new LanguagePickerMediator(
                mContext, mModel, mListModel, videoTutorialService, languageInfoProvider);
    }

    /**
     * Called to open the language picker UI.
     * @param feature The tutorial for which the language options will be shown.
     * @param doneCallback The callback to be invoked when the watch button is clicked.
     * @param closeCallback The callback to be invoked when the close button is clicked.
     */
    public void showLanguagePicker(
            @FeatureType int feature, Runnable doneCallback, Runnable closeCallback) {
        mMediator.showLanguagePicker(feature, doneCallback, closeCallback);
    }

    /** @return A {@link View} representing this coordinator. */
    public View getView() {
        return mView.getView();
    }
}
