// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.video_tutorials;

import org.chromium.base.Callback;

import java.util.List;

/**
 * Java interface for interacting with the native video tutorial service. Responsible for
 * initializing and fetching data fo be shown on the UI.
 */
public interface VideoTutorialService {
    /**
     * Called to get the list of video tutorials available.
     */
    void getTutorials(Callback<List<Tutorial>> callback);

    /**
     * Called to get the tutorial for a given feature.
     */
    void getTutorial(@FeatureType int featureType, Callback<Tutorial> callback);

    /**
     * Called to get the list of supported languages.
     * Deprecated in favor of getAvailableLanguagesForVideo(@FeatureType).
     */
    List<String> getSupportedLanguages();

    /**
     * Called to get the list of supported languages in which a tutorial is available.
     * @param feature The associated tutorial.
     */
    List<String> getAvailableLanguagesForTutorial(@FeatureType int feature);

    /**
     * @return The user's language of choice for watching the video tutorials, or null user hasn't
     *         set it yet.
     */
    String getPreferredLocale();

    /**
     * Called to set the user's preferred {@link locale} for watching the videos. The caller
     * should make another getTutorial() method call immediately to get the new list of tutorials in
     * the newly selected language.
     */
    void setPreferredLocale(String locale);
}
