// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.test.transit.webapps;

import static androidx.test.espresso.matcher.ViewMatchers.withText;

import org.chromium.base.test.transit.CarryOn;
import org.chromium.chrome.browser.ChromeTabbedActivity;

/** Ensures the PwaRestore dialog is hidden. */
public class PwaRestoreHiddenCarryOn extends CarryOn {
    public PwaRestoreHiddenCarryOn() {
        declareActivity(ChromeTabbedActivity.class);
        declareNoView(withText(PwaRestoreCarryOn.RESTORE_YOUR_WEB_APPS));
        declareNoView(withText(PwaReviewCarryOn.WEB_APPS_USED_IN_THE_LAST_MONTH));
    }
}
