// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.ui.hats;

/**
 * Util class that creates a new SurveyController.
 */
class SurveyControllerProvider {
    private SurveyControllerProvider() {}

    /**
     * @return A new instance of survey controller.
     */
    static SurveyController create() {
        return new SurveyController() {};
    }
}
