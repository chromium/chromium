// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.test.transit.hub;

import static androidx.test.espresso.matcher.ViewMatchers.withContentDescription;
import static androidx.test.espresso.matcher.ViewMatchers.withId;

import static org.hamcrest.Matchers.allOf;
import static org.hamcrest.Matchers.anyOf;

import android.view.View;

import org.chromium.base.test.transit.Facility;
import org.chromium.base.test.transit.ViewElement;
import org.chromium.chrome.browser.ChromeTabbedActivity;
import org.chromium.chrome.test.R;
import org.chromium.chrome.test.transit.ChromeActivityTabModelBoundStation;
import org.chromium.chrome.test.transit.page.CtaPageStation;
import org.chromium.components.tab_groups.TabGroupColorId;

/**
 * The color picker palette that is opened from the {@link TabGroupDialogFacility}.
 *
 * @param <HostStationT> the station where the Tab Group Dialog is opened from. Should be {@link
 *     TabSwitcherStation} or {@link CtaPageStation}.
 */
public class TabGroupColorPickerFacility<
                HostStationT extends ChromeActivityTabModelBoundStation<ChromeTabbedActivity>>
        extends Facility<HostStationT> {

    private final TabGroupDialogFacility<HostStationT> mTabGroupDialog;
    private ViewElement<View>[] mChipElements;

    public TabGroupColorPickerFacility(TabGroupDialogFacility<HostStationT> tabGroupDialog) {
        mTabGroupDialog = tabGroupDialog;
        declareChipElements();
    }

    /**
     * Selects a color from the palette by finding a view with a matching content description.
     *
     * @param colorId The {@link TabGroupColorId} to select.
     * @return The previous {@link TabGroupDialogFacility} with the new color selected.
     */
    public TabGroupDialogFacility<TabSwitcherStation> selectColor(@TabGroupColorId int colorId) {
        ViewElement<View> chipElement = mChipElements[colorId];

        // The destination is a new instance of the dialog facility with the updated color state.
        TabGroupDialogFacility<TabSwitcherStation> destination =
                new TabGroupDialogFacility<>(
                        mTabGroupDialog.getTabIdsInGroup(), mTabGroupDialog.getTitle(), colorId);

        chipElement.clickTo().exitFacilitiesAnd(mTabGroupDialog, this).enterFacility(destination);

        return destination;
    }

    /** Initializes and declares all the {@link ViewElement}s for the color chips in the palette. */
    private void declareChipElements() {
        mChipElements = new ViewElement[TabGroupColorId.NUM_ENTRIES];
        for (int i = 0; i < TabGroupColorId.NUM_ENTRIES; i++) {
            String colorName = getColorNameString(i);
            mChipElements[i] =
                    declareView(
                            allOf(
                                    withId(R.id.color_picker_icon),
                                    anyOf(
                                            withContentDescription(colorName + " Selected"),
                                            withContentDescription(colorName + " Not selected"))));
        }
    }

    /**
     * Converts a TabGroupColorId to its string name for content descriptions. This must match the
     * string used when the content descriptions are set on the views.
     */
    private String getColorNameString(@TabGroupColorId int colorId) {
        return switch (colorId) {
            case TabGroupColorId.GREY -> "Grey";
            case TabGroupColorId.BLUE -> "Blue";
            case TabGroupColorId.RED -> "Red";
            case TabGroupColorId.YELLOW -> "Yellow";
            case TabGroupColorId.GREEN -> "Green";
            case TabGroupColorId.PINK -> "Pink";
            case TabGroupColorId.PURPLE -> "Purple";
            case TabGroupColorId.CYAN -> "Cyan";
            case TabGroupColorId.ORANGE -> "Orange";
            default -> throw new IllegalArgumentException("Invalid colorId: " + colorId);
        };
    }
}
