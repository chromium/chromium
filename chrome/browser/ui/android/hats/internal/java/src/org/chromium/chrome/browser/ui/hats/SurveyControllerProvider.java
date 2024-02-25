// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.ui.hats;

import org.chromium.chrome.browser.profiles.Profile;

/** Util class that creates a new SurveyController. */
// TODO(crbug/1400731): Change to package private once public references are removed.
public class SurveyControllerProvider {
    private SurveyControllerProvider() {}

    /** Return a SurveyController associated with the given profile. */
    public static SurveyController create(Profile profile) {
        return new SurveyController() {};
    }
}
