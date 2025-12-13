// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.test.transit.quick_delete;

import static androidx.test.espresso.matcher.ViewMatchers.withId;
import static androidx.test.espresso.matcher.ViewMatchers.withText;

import android.util.Pair;
import android.view.View;
import android.widget.Spinner;
import android.widget.TextView;

import org.chromium.base.test.transit.ConditionStatus;
import org.chromium.base.test.transit.Facility;
import org.chromium.base.test.transit.UiThreadCondition;
import org.chromium.base.test.transit.ViewElement;
import org.chromium.base.test.transit.ViewSpec;
import org.chromium.chrome.browser.browsing_data.ClearBrowsingDataFragment;
import org.chromium.chrome.browser.browsing_data.TimePeriod;
import org.chromium.chrome.browser.browsing_data.TimePeriodUtils;
import org.chromium.chrome.browser.browsing_data.TimePeriodUtils.TimePeriodSpinnerOption;
import org.chromium.chrome.test.R;
import org.chromium.chrome.test.transit.hub.RegularTabSwitcherStation;
import org.chromium.chrome.test.transit.settings.SettingsStation;
import org.chromium.chrome.test.transit.ui.ModalDialogFacility;

/** Confirmation dialog that appears when "Delete browsing data" is chosen from the app menu. */
public class QuickDeleteDialogFacility extends ModalDialogFacility {
    private final int mTimePeriod;

    public ViewElement<Spinner> spinnerElement;
    public ViewElement<TextView> historyInfoElement;
    public ViewElement<TextView> tabsInfoElement;
    public ViewElement<View> moreOptionsElement;

    public QuickDeleteDialogFacility() {
        // Default selection is LAST_15_MINUTES.
        this(TimePeriod.LAST_15_MINUTES);
    }

    public QuickDeleteDialogFacility(@TimePeriod int timePeriod) {
        super();

        mTimePeriod = timePeriod;

        declareCustomView();
        declareNegativeButton("Cancel");
        declarePositiveButton("Delete data");

        // Not an actual ModalDialog title, so don't use declareTitle() here.
        declareView(dialogElement.descendant(withText(R.string.quick_delete_dialog_title)));
        spinnerElement =
                declareView(
                        dialogElement.descendant(Spinner.class, withId(R.id.quick_delete_spinner)));
        historyInfoElement =
                declareView(
                        dialogElement.descendant(
                                TextView.class, withId(R.id.quick_delete_history_row_title)));
        tabsInfoElement =
                declareView(
                        dialogElement.descendant(
                                TextView.class, withId(R.id.quick_delete_tabs_row_title)),
                        ViewElement.allowDisabledOption());
        declareView(
                dialogElement.descendant(
                        withText(
                                R.string
                                        .quick_delete_dialog_cookies_cache_and_other_site_data_text)));
        moreOptionsElement =
                declareView(dialogElement.descendant(withId(R.id.quick_delete_more_options)));

        declareEnterCondition(new TimePeriodSelectedCondition());
    }

    /**
     * Click Delete to delete the selected time period, go to the tab switcher and show a snackbar.
     *
     * @param regularTabsExistAfterDeletion True if there are any regular tabs to display in the tab
     *     switcher post-deletion. This should typically be false if all tabs are deleted or if the
     *     last tab in the tab switcher is closed.
     */
    public Pair<RegularTabSwitcherStation, QuickDeleteSnackbarFacility> confirmDelete(
            boolean regularTabsExistAfterDeletion) {
        RegularTabSwitcherStation tabSwitcher =
                new RegularTabSwitcherStation(
                        regularTabsExistAfterDeletion, /* incognitoTabsExist= */ false);
        QuickDeleteSnackbarFacility snackbar = new QuickDeleteSnackbarFacility(mTimePeriod);

        positiveButtonElement.clickTo().arriveAt(tabSwitcher, snackbar);
        return Pair.create(tabSwitcher, snackbar);
    }

    /** Set the time period to delete in the Spinner. */
    public QuickDeleteDialogFacility setTimePeriodInSpinner(@TimePeriod int timePeriod) {
        TimePeriodSpinnerOption[] options =
                TimePeriodUtils.getTimePeriodSpinnerOptions(mHostStation.getActivity());
        final int positionToSet = getSpinnerPositionForTimePeriod(timePeriod, options);

        return runOnUiThreadTo(() -> spinnerElement.value().setSelection(positionToSet))
                .exitFacilityAnd()
                .enterFacility(new QuickDeleteDialogFacility(timePeriod));
    }

    private static int getSpinnerPositionForTimePeriod(
            int timePeriod, TimePeriodSpinnerOption[] options) {
        for (int position = 0; position < options.length; position++) {
            if (options[position].getTimePeriod() == timePeriod) {
                return position;
            }
        }
        throw new IllegalArgumentException(
                "Time period " + timePeriod + " not found in spinner options.");
    }

    /** Click the "More options" button to open the in Settings. */
    public SettingsStation<ClearBrowsingDataFragment> clickMoreOptions() {
        return moreOptionsElement
                .clickTo()
                .arriveAt(new SettingsStation<>(ClearBrowsingDataFragment.class));
    }

    public void expectSearchHistoryDisambiguation(boolean shown) {
        var facility = new Facility<>("SearchHistoryDisambiguation" + (shown ? "Shown" : "Hidden"));
        ViewSpec<View> spec = dialogElement.descendant(withId(R.id.search_history_disambiguation));
        if (shown) {
            facility.declareView(spec);
        } else {
            facility.declareNoView(spec);
        }
        noopTo().enterFacility(facility);
    }

    public void expectMoreOnSyncedDevices(boolean shown) {
        var facility = new Facility<>("MoreOnSyncedDevices" + (shown ? "Shown" : "Hidden"));
        ViewSpec<View> spec =
                dialogElement.descendant(withId(R.id.quick_delete_history_row_subtitle));
        if (shown) {
            facility.declareView(spec);
        } else {
            facility.declareNoView(spec);
        }
        noopTo().enterFacility(facility);
    }

    private class TimePeriodSelectedCondition extends UiThreadCondition {
        public TimePeriodSelectedCondition() {
            dependOnSupplier(spinnerElement, "Spinner View");
        }

        @Override
        protected ConditionStatus checkWithSuppliers() {
            Spinner spinner = spinnerElement.value();
            var item = (TimePeriodUtils.TimePeriodSpinnerOption) spinner.getSelectedItem();
            int timePeriod = item.getTimePeriod();
            return whether(
                    timePeriod == mTimePeriod,
                    "Spinner selected TimePeriod %d, expected %d",
                    timePeriod,
                    mTimePeriod);
        }

        @Override
        public String buildDescription() {
            return "Spinner selected TimePeriod " + mTimePeriod;
        }
    }
}
