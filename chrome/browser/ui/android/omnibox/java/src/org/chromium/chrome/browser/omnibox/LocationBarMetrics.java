// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
package org.chromium.chrome.browser.omnibox;

import org.chromium.base.metrics.RecordUserAction;
import org.chromium.build.annotations.NullMarked;

@NullMarked
public class LocationBarMetrics {
    /** Record when the delete button is shown on the omnibox. */
    public static void recordDeleteButtonShown(boolean isShown) {
        RecordUserAction.record(isShown ? "MobileOmniboxDeleteShown" : "MobileOmniboxDeleteHidden");
    }

    /** Record when the install button is shown on the omnibox. */
    public static void recordInstallButtonShown(boolean isShown) {
        RecordUserAction.record(
                isShown ? "MobileOmniboxInstallShown" : "MobileOmniboxInstallHidden");
    }

    /** Record when the mic button is shown on the omnibox. */
    public static void recordMicButtonShown(boolean isShown) {
        RecordUserAction.record(isShown ? "MobileOmniboxMicShown" : "MobileOmniboxMicHidden");
    }

    /** Record when the bookmark button is shown on the omnibox. */
    public static void recordBookmarkButtonShown(boolean isShown) {
        RecordUserAction.record(
                isShown ? "MobileOmniboxBookmarkShown" : "MobileOmniboxBookmarkHidden");
    }

    /** Record when the zoom button is shown on the omnibox. */
    public static void recordZoomButtonShown(boolean isShown) {
        RecordUserAction.record(isShown ? "MobileOmniboxZoomShown" : "MobileOmniboxZoomHidden");
    }
}
