// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.video_tutorials.languages;

import static org.hamcrest.CoreMatchers.equalTo;
import static org.hamcrest.MatcherAssert.assertThat;
import static org.mockito.Mockito.verify;

import android.content.Context;

import org.junit.Before;
import org.junit.Test;
import org.junit.runner.RunWith;
import org.mockito.Mock;
import org.mockito.Mockito;
import org.mockito.MockitoAnnotations;

import org.chromium.base.metrics.UmaRecorderHolder;
import org.chromium.base.test.BaseRobolectricTestRunner;
import org.chromium.chrome.browser.video_tutorials.FeatureType;
import org.chromium.chrome.browser.video_tutorials.LanguageInfoProvider;
import org.chromium.chrome.browser.video_tutorials.test.TestVideoTutorialService;
import org.chromium.ui.modelutil.MVCListAdapter.ListItem;
import org.chromium.ui.modelutil.MVCListAdapter.ModelList;
import org.chromium.ui.modelutil.PropertyKey;
import org.chromium.ui.modelutil.PropertyModel;
import org.chromium.ui.modelutil.PropertyObservable;

/**
 * Tests for {@link LanguagePickerMediator}.
 */
@RunWith(BaseRobolectricTestRunner.class)
public class LanguagePickerMediatorUnitTest {
    @Mock
    Context mContext;
    private TestVideoTutorialService mTestVideoTutorialService;
    private PropertyModel mModel;
    private ModelList mListModel;
    private LanguagePickerMediator mMediator;
    @Mock
    private PropertyObservable.PropertyObserver<PropertyKey> mPropertyObserver;
    @Mock
    private LanguageInfoProvider mLanguageProvider;

    @Before
    public void setUp() {
        UmaRecorderHolder.resetForTesting();
        MockitoAnnotations.initMocks(this);

        mModel = new PropertyModel(LanguagePickerProperties.ALL_KEYS);
        mModel.addObserver(mPropertyObserver);

        mListModel = new ModelList();
        mTestVideoTutorialService = new TestVideoTutorialService();
        mMediator = new LanguagePickerMediator(
                mContext, mModel, mListModel, mTestVideoTutorialService, mLanguageProvider);
    }

    @Test
    public void checkCallbacks() {
        mMediator.showLanguagePicker(FeatureType.CHROME_INTRO, () -> {}, () -> {});
        verify(mPropertyObserver)
                .onPropertyChanged(mModel, LanguagePickerProperties.CLOSE_CALLBACK);
        verify(mPropertyObserver)
                .onPropertyChanged(mModel, LanguagePickerProperties.WATCH_CALLBACK);
    }

    @Test
    public void loadsLanguagesInTheList() {
        Mockito.when(mLanguageProvider.getLanguageInfo("hi"))
                .thenReturn(TestVideoTutorialService.HINDI);
        Mockito.when(mLanguageProvider.getLanguageInfo("ta"))
                .thenReturn(TestVideoTutorialService.TAMIL);
        Mockito.when(mLanguageProvider.getLanguageInfo("en"))
                .thenReturn(TestVideoTutorialService.ENGLISH);

        mMediator.showLanguagePicker(FeatureType.CHROME_INTRO, () -> {}, () -> {});

        assertThat(mListModel.size(), equalTo(mTestVideoTutorialService.getTestLanguages().size()));
        ListItem listItem = mListModel.get(0);
        assertThat(listItem.model.get(LanguageItemProperties.NAME),
                equalTo(TestVideoTutorialService.HINDI.name));
        assertThat(listItem.model.get(LanguageItemProperties.NATIVE_NAME),
                equalTo(TestVideoTutorialService.HINDI.nativeName));
    }
}
