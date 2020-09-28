// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.video_tutorials.languages;

import android.content.Context;
import android.content.res.Resources;
import android.text.TextUtils;
import android.view.View;

import org.chromium.chrome.browser.video_tutorials.VideoTutorialService;
import org.chromium.ui.modelutil.MVCListAdapter.ListItem;
import org.chromium.ui.modelutil.MVCListAdapter.ModelList;
import org.chromium.ui.modelutil.PropertyModel;

import java.util.ArrayList;
import java.util.List;

/**
 *  The top level coordinator for the language picker UI.
 */
public class LanguagePickerCoordinator {
    private final Context mContext;
    private final VideoTutorialService mVideoTutorialService;
    private final LanguagePickerView mView;
    private final PropertyModel mModel;
    private final ModelList mListModel;

    /**
     * Constructor.
     * @param view The view representing this language picker.
     * @param videoTutorialService The video tutorial service backend.
     */
    public LanguagePickerCoordinator(View view, VideoTutorialService videoTutorialService) {
        mContext = view.getContext();
        mVideoTutorialService = videoTutorialService;
        mModel = new PropertyModel(LanguagePickerProperties.ALL_KEYS);
        mListModel = new ModelList();
        mView = new LanguagePickerView(view, mModel, mListModel);
    }

    /**
     * Called to open the language picker UI.
     * @param doneCallback The callback to be invoked when the watch button is clicked.
     * @param closeCallback The callback to be invoked when the close button is clicked.
     */
    public void showLanguagePicker(Runnable doneCallback, Runnable closeCallback) {
        mModel.set(LanguagePickerProperties.CLOSE_CALLBACK, closeCallback);
        mModel.set(LanguagePickerProperties.WATCH_CALLBACK, doneCallback);
        populateList(mVideoTutorialService.getSupportedLanguages());
    }

    /** @return A {@link View} representing this coordinator. */
    public View getView() {
        return mView.getView();
    }

    private void onLanguageSelected(String locale) {
        mVideoTutorialService.setPreferredLocale(locale);
        populateList(mVideoTutorialService.getSupportedLanguages());
    }

    private void populateList(List<String> supportedLocales) {
        List<ListItem> listItems = new ArrayList<>();
        for (String locale : supportedLocales) {
            ListItem listItem = new ListItem(
                    LanguageItemProperties.ITEM_VIEW_TYPE, buildListItemModelFromLocale(locale));
            listItems.add(listItem);
        }
        mListModel.set(listItems);
    }

    private PropertyModel buildListItemModelFromLocale(String locale) {
        Resources resources = mContext.getResources();
        String preferredLocale = mVideoTutorialService.getPreferredLocale();
        return new PropertyModel.Builder(LanguageItemProperties.ALL_KEYS)
                .with(LanguageItemProperties.LOCALE, locale)
                .with(LanguageItemProperties.NAME,
                        LanguageUtils.getLanguageForLocale(resources, locale))
                .with(LanguageItemProperties.NATIVE_NAME,
                        LanguageUtils.getLanguageForLocaleInNativeText(resources, locale))
                .with(LanguageItemProperties.IS_SELECTED, TextUtils.equals(locale, preferredLocale))
                .with(LanguageItemProperties.SELECTION_CALLBACK, this::onLanguageSelected)
                .build();
    }
}
