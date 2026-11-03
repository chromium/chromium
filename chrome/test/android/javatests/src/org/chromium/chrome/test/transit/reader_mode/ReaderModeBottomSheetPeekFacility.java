// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.test.transit.reader_mode;

import static androidx.test.espresso.matcher.ViewMatchers.withId;

import android.view.View;

import org.chromium.base.test.transit.ViewElement;
import org.chromium.chrome.R;
import org.chromium.chrome.test.transit.page.CtaPageStation;
import org.chromium.chrome.test.transit.ui.BottomSheetFacility;

/** Facility representing the peeked Reader Mode bottom sheet. */
public class ReaderModeBottomSheetPeekFacility extends BottomSheetFacility<CtaPageStation> {
    public final ViewElement<View> titleElement;

    public ReaderModeBottomSheetPeekFacility() {
        super();
        titleElement = declareDescendantView(withId(R.id.title));
    }

    /** Expand the bottom sheet to show the preferences. */
    public ReaderModeBottomSheetExpandedFacility expand() {
        return titleElement.clickTo().enterFacility(new ReaderModeBottomSheetExpandedFacility());
    }
}
