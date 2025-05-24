// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.test.transit.settings;

import static androidx.test.espresso.matcher.ViewMatchers.hasDescendant;
import static androidx.test.espresso.matcher.ViewMatchers.withParent;
import static androidx.test.espresso.matcher.ViewMatchers.withText;

import static org.hamcrest.Matchers.instanceOf;

import static org.chromium.base.test.transit.ViewSpec.viewSpec;

import android.view.View;

import androidx.recyclerview.widget.RecyclerView;

import org.chromium.base.test.transit.Facility;
import org.chromium.base.test.transit.ViewElement;

/** The facility describing one setting preference item in the MainSettings fragment. */
public class PreferenceFacility extends Facility<SettingsStation<?>> {
    public ViewElement<View> prefViewElement;

    /**
     * Creates the facility describing one preference item in the MainSettings fragment.
     *
     * @param prefTitle The preference title. It's used to match the preference view on the screen.
     */
    public PreferenceFacility(String prefTitle) {
        prefViewElement =
                declareView(
                        viewSpec(
                                hasDescendant(withText(prefTitle)),
                                withParent(instanceOf(RecyclerView.class))));
    }
}
