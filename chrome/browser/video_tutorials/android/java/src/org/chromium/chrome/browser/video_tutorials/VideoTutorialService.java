// Copyright 2020 The Chromium Authors. All rights reserved.
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
     * Called to get the {@link Tutorial} associated with a feature. Returns null if a matching
     * tutorial doesn't exist.
     */
    void getTutorial(@FeatureType int feature, Callback<Tutorial> callback);

    /**
     * @return The list of locales in which video tutorials are available.
     */
    List<String> getSupportedLocales();

    /**
     * @return The preferred locale to show the video tutorial.
     */
    String getPreferredLocale();

    /**
     * Called to set the preferred locale for showing the video tutorials.
     */
    void setPreferredLocale(String locale);
}
