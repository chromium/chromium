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

/** Facility representing the expanded Reader Mode bottom sheet. */
public class ReaderModeBottomSheetExpandedFacility extends BottomSheetFacility<CtaPageStation> {
    public final ViewElement<View> lightModeButton;
    public final ViewElement<View> sepiaModeButton;
    public final ViewElement<View> darkModeButton;
    public final ViewElement<View> fontSizeSlider;
    public final ViewElement<View> fontSansSerifButton;
    public final ViewElement<View> fontSerifButton;
    public final ViewElement<View> fontMonospaceButton;

    public ReaderModeBottomSheetExpandedFacility() {
        super();
        lightModeButton = declareDescendantView(withId(R.id.light_mode));
        sepiaModeButton = declareDescendantView(withId(R.id.sepia_mode));
        darkModeButton = declareDescendantView(withId(R.id.dark_mode));
        fontSizeSlider = declareDescendantView(withId(R.id.font_size_slider));
        fontSansSerifButton = declareDescendantView(withId(R.id.font_sans_serif));
        fontSerifButton = declareDescendantView(withId(R.id.font_serif));
        fontMonospaceButton = declareDescendantView(withId(R.id.font_monospace));
    }

    /** Collapse the bottom sheet back to peek state. */
    public ReaderModeBottomSheetPeekFacility collapse() {
        recheckActiveConditions();
        return pressBackTo()
                .exitFacilityAnd()
                .enterFacility(new ReaderModeBottomSheetPeekFacility());
    }
}
