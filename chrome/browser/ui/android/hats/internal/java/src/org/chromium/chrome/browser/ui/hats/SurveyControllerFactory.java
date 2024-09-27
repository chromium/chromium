// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.ui.hats;

import org.chromium.chrome.browser.profiles.Profile;

/** Creates a {@link SurveyController}. */
public interface SurveyControllerFactory {
    /** Return a SurveyController associated with the given profile. */
    SurveyController create(Profile profile);
}
