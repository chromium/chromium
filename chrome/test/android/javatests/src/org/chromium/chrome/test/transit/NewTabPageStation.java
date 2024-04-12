// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.test.transit;

import org.chromium.base.test.transit.Elements;

/**
 * The New Tab Page screen, with an omnibox, most visited tiles, and the Feed instead of the
 * WebContents.
 */
public class NewTabPageStation extends PageStation {
    /** Use {@link #newPageStationBuilder()} or the PageStation's subclass |newBuilder()|. */
    protected NewTabPageStation(Builder<NewTabPageStation> builder) {
        super(builder);
    }

    public static Builder<NewTabPageStation> newBuilder() {
        return new Builder<>(NewTabPageStation::new);
    }

    @Override
    public void declareElements(Elements.Builder elements) {
        super.declareElements(elements);

        elements.declareEnterCondition(new NtpLoadedCondition(mPageLoadedEnterCondition));
    }
}
