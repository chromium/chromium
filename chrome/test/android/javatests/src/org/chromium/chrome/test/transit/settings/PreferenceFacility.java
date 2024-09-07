// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.test.transit.settings;

import static androidx.test.espresso.matcher.ViewMatchers.hasDescendant;
import static androidx.test.espresso.matcher.ViewMatchers.withParent;
import static androidx.test.espresso.matcher.ViewMatchers.withText;

import static org.chromium.base.test.transit.ViewSpec.viewSpec;

import android.view.View;

import androidx.recyclerview.widget.RecyclerView;

import org.hamcrest.Matchers;

import org.chromium.base.test.transit.Elements;
import org.chromium.base.test.transit.Facility;
import org.chromium.base.test.transit.ViewElement;
import org.chromium.base.test.transit.ViewSpec;

/** The facility describing one setting preference item in the MainSettings fragment. */
public class PreferenceFacility extends Facility<SettingsStation> {
    private final ViewSpec mPrefViewSpec;

    private ViewElement mPrefView;

    /**
     * Creates the facility describing one preference item in the MainSettings fragment.
     *
     * @param prefTitle The preference title. It's used to match the preference view on the screen.
     */
    public PreferenceFacility(String prefTitle) {
        mPrefViewSpec =
                viewSpec(
                        hasDescendant(withText(prefTitle)),
                        withParent(Matchers.instanceOf(RecyclerView.class)));
    }

    @Override
    public void declareElements(Elements.Builder elements) {
        mPrefView = elements.declareView(mPrefViewSpec);
    }

    /**
     * @return The preference's view.
     */
    public View getPrefView() {
        assertSuppliersCanBeUsed();
        return mPrefView.get();
    }
}
