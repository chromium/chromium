// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.video_tutorials.test;

import org.chromium.base.Callback;
import org.chromium.chrome.browser.video_tutorials.FeatureType;
import org.chromium.chrome.browser.video_tutorials.Language;
import org.chromium.chrome.browser.video_tutorials.Tutorial;
import org.chromium.chrome.browser.video_tutorials.VideoTutorialService;

import java.util.ArrayList;
import java.util.List;

/** A video tutorial service implementation for tests. */
public class TestVideoTutorialService implements VideoTutorialService {
    public static final Language HINDI = new Language("hi", "Hindi", "Hindi Native");
    public static final Language TAMIL = new Language("ta", "Tamil", "Tamil Native");
    public static final Language ENGLISH = new Language("en", "English", "English Native");

    private final List<Tutorial> mTutorials = new ArrayList<>();
    private final List<Language> mLanguages = new ArrayList<>();
    private String mPreferredLocale;

    public TestVideoTutorialService() {
        initializeLanguages();
        mPreferredLocale = HINDI.locale;
        initializeTutorialList();
    }

    @Override
    public void getTutorials(Callback<List<Tutorial>> callback) {
        callback.onResult(mTutorials);
    }

    @Override
    public void getTutorial(int featureType, Callback<Tutorial> callback) {
        for (Tutorial tutorial : mTutorials) {
            if (tutorial.featureType == featureType) callback.onResult(tutorial);
        }
    }

    @Override
    public List<Language> getSupportedLanguages() {
        return mLanguages;
    }

    @Override
    public String getPreferredLocale() {
        return mPreferredLocale;
    }

    @Override
    public void setPreferredLocale(String locale) {
        mPreferredLocale = locale;
    }

    public List<Language> getTestLanguages() {
        return mLanguages;
    }

    public List<Tutorial> getTestTutorials() {
        return mTutorials;
    }

    private void initializeTutorialList() {
        mTutorials.add(new Tutorial(FeatureType.DOWNLOAD,
                "How to use Google Chrome's download functionality",
                "https://storage.googleapis.com/stock-wizard.appspot.com/portrait.jpg",
                "https://storage.googleapis.com/stock-wizard.appspot.com/portrait.jpg",
                "caption url", "share url", 35));

        mTutorials.add(
                new Tutorial(FeatureType.SEARCH, "How to efficiently search with Google Chrome",
                        "https://storage.googleapis.com/stock-wizard.appspot.com/elephant.jpg ",
                        "https://storage.googleapis.com/stock-wizard.appspot.com/elephant.jpg",
                        "caption url", "share url", 35));
    }

    private void initializeLanguages() {
        mLanguages.add(HINDI);
        mLanguages.add(TAMIL);
        mLanguages.add(ENGLISH);
    }
}
