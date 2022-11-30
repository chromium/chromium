// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.video_tutorials.test;

import org.chromium.base.Callback;
import org.chromium.chrome.browser.video_tutorials.FeatureType;
import org.chromium.chrome.browser.video_tutorials.Language;
import org.chromium.chrome.browser.video_tutorials.Tutorial;
import org.chromium.chrome.browser.video_tutorials.VideoTutorialService;

import java.util.ArrayList;
import java.util.Arrays;
import java.util.List;

/** A video tutorial service implementation for tests. */
public class TestVideoTutorialService implements VideoTutorialService {
    public static final Language HINDI = new Language("hi", "hindi", "हिंदी");
    public static final Language TAMIL = new Language("ta", "Tamil", "தமிழ்");
    public static final Language ENGLISH = new Language("en", "English", "English");

    private final List<Tutorial> mTutorials = new ArrayList<>();
    private final List<String> mLanguages = new ArrayList<>();
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
        if (featureType == FeatureType.SUMMARY) {
            Tutorial summary = new Tutorial(FeatureType.SUMMARY, "Videos on how to use chrome",
                    "https://www.gstatic.com/chrome/video-tutorials/images/1_Search_english.mp4",
                    "https://www.gstatic.com/chrome/video-tutorials/images/1_Search_english.png",
                    "https://www.gstatic.com/chrome/video-tutorials/gif/sample_anim.gif",
                    "https://www.gstatic.com/chrome/video-tutorials/images/1_Search_english.png",
                    "caption url", "share url", 25);
            callback.onResult(summary);
            return;
        }

        for (Tutorial tutorial : mTutorials) {
            if (tutorial.featureType == featureType) callback.onResult(tutorial);
        }
    }

    @Override
    public List<String> getSupportedLanguages() {
        return mLanguages;
    }

    @Override
    public List<String> getAvailableLanguagesForTutorial(int feature) {
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

    public List<String> getTestLanguages() {
        return mLanguages;
    }

    public List<Tutorial> getTestTutorials() {
        return mTutorials;
    }

    private void initializeTutorialList() {
        mTutorials.add(new Tutorial(FeatureType.CHROME_INTRO, "Introduction to chrome",
                "https://www.gstatic.com/chrome/video-tutorials/webm/1_Search_english.mp4",
                "https://www.gstatic.com/chrome/video-tutorials/images/1_Search_english.png",
                "https://www.gstatic.com/chrome/video-tutorials/gif/sample_anim.gif",
                "https://www.gstatic.com/chrome/video-tutorials/images/1_Search_english.png",
                "caption url", "share url", 25));

        mTutorials.add(new Tutorial(FeatureType.DOWNLOAD,
                "How to use Google Chrome's download functionality",
                "https://www.gstatic.com/chrome/video-tutorials/webm/1_Search_english.mp4",
                "https://www.gstatic.com/chrome/video-tutorials/images/1_Search_english.png",
                "https://www.gstatic.com/chrome/video-tutorials/gif/sample_anim.gif",
                "https://www.gstatic.com/chrome/video-tutorials/images/1_Search_english.png",
                "caption url", "share url", 35));

        mTutorials.add(new Tutorial(FeatureType.SEARCH,
                "How to efficiently search with Google Chrome",
                "https://www.gstatic.com/chrome/video-tutorials/webm/1_Search_english.mp4",
                "https://www.gstatic.com/chrome/video-tutorials/images/1_Search_english.png",
                "https://www.gstatic.com/chrome/video-tutorials/gif/sample_anim.gif",
                "https://www.gstatic.com/chrome/video-tutorials/images/1_Search_english.png",
                "caption url", "share url", 335));
    }

    private void initializeLanguages() {
        mLanguages.add("hi");
        mLanguages.add("ta");
        mLanguages.add("en");
    }

    /** Initialized to a set of test languages. */
    public void initializeTestLanguages(String[] languages) {
        mLanguages.clear();
        mLanguages.addAll(Arrays.asList(languages));
    }
}
