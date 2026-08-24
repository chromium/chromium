// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.test.transit.omnibox;

/** Represents the Omnibox when focused in drafting mode (DisplayState.DRAFTING). */
public class OmniboxDraftingFacility extends OmniboxEnteredTextFacility {
    public OmniboxDraftingFacility(OmniboxFacility omniboxFacility, String text) {
        super(omniboxFacility, text);

        declareNoView(LOCATION_BAR_POPPED_OUT);
        declareNoView(SUGGESTIONS_DROPDOWN);
    }
}
