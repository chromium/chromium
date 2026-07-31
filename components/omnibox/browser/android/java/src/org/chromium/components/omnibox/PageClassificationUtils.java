// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.components.omnibox;

import org.chromium.build.annotations.NullMarked;
import org.chromium.components.metrics.OmniboxEventProtos.OmniboxEventProto.PageClassification;

/** Utility methods for checking page classifications. */
@NullMarked
public class PageClassificationUtils {
    private PageClassificationUtils() {}

    /**
     * Returns whether the given page classification represents Hub Search or Tab Search Overlay.
     *
     * @param pageClassification The page classification to check.
     */
    public static boolean isHubOrTabSearch(int pageClassification) {
        return pageClassification == PageClassification.ANDROID_HUB_VALUE
                || pageClassification == PageClassification.ANDROID_TAB_SEARCH_OVERLAY_VALUE;
    }
}
