// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.video_tutorials.languages;

import android.content.Context;
import android.text.TextUtils;

import org.chromium.chrome.browser.video_tutorials.Language;
import org.chromium.chrome.browser.video_tutorials.VideoTutorialService;
import org.chromium.ui.modelutil.MVCListAdapter.ListItem;
import org.chromium.ui.modelutil.MVCListAdapter.ModelList;
import org.chromium.ui.modelutil.PropertyModel;

import java.util.ArrayList;
import java.util.List;

/**
 *  The mediator for language selection UI.
 */
public class LanguagePickerMediator {
    private final Context mContext;
    private final VideoTutorialService mVideoTutorialService;
    private final PropertyModel mModel;
    private final ModelList mListModel;

    /**
     * Constructor.
     * @param videoTutorialService The video tutorial service backend.
     */
    public LanguagePickerMediator(Context context, PropertyModel model, ModelList listModel,
            VideoTutorialService videoTutorialService) {
        mContext = context;
        mVideoTutorialService = videoTutorialService;
        mModel = model;
        mListModel = listModel;
    }

    /**
     * See {@link LanguagePickerCoordinator#showLanguagePicker(Runnable, Runnable)}.
     */
    public void showLanguagePicker(Runnable doneCallback, Runnable closeCallback) {
        mModel.set(LanguagePickerProperties.CLOSE_CALLBACK, closeCallback);
        mModel.set(LanguagePickerProperties.WATCH_CALLBACK, doneCallback);
        populateList(mVideoTutorialService.getSupportedLanguages());
    }

    private void onLanguageSelected(String locale) {
        mVideoTutorialService.setPreferredLocale(locale);
        populateList(mVideoTutorialService.getSupportedLanguages());
    }

    private void populateList(List<Language> supportedLanguages) {
        List<ListItem> listItems = new ArrayList<>();
        for (Language locale : supportedLanguages) {
            ListItem listItem = new ListItem(
                    LanguageItemProperties.ITEM_VIEW_TYPE, buildListItemModelFromLocale(locale));
            listItems.add(listItem);
        }
        mListModel.set(listItems);
    }

    private PropertyModel buildListItemModelFromLocale(Language language) {
        String preferredLocale = mVideoTutorialService.getPreferredLocale();
        return new PropertyModel.Builder(LanguageItemProperties.ALL_KEYS)
                .with(LanguageItemProperties.LOCALE, language.locale)
                .with(LanguageItemProperties.NAME, language.name)
                .with(LanguageItemProperties.NATIVE_NAME, language.nativeName)
                .with(LanguageItemProperties.IS_SELECTED,
                        TextUtils.equals(language.locale, preferredLocale))
                .with(LanguageItemProperties.SELECTION_CALLBACK, this::onLanguageSelected)
                .build();
    }
}
