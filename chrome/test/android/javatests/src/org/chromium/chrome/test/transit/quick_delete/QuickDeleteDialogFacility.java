// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.test.transit.quick_delete;

import static androidx.test.espresso.matcher.ViewMatchers.withId;
import static androidx.test.espresso.matcher.ViewMatchers.withText;

import static org.chromium.base.test.transit.ViewSpec.viewSpec;

import android.util.Pair;
import android.view.View;
import android.widget.Spinner;
import android.widget.TextView;

import androidx.test.espresso.Espresso;

import org.chromium.base.ThreadUtils;
import org.chromium.base.test.transit.ConditionStatus;
import org.chromium.base.test.transit.Elements;
import org.chromium.base.test.transit.Facility;
import org.chromium.base.test.transit.Station;
import org.chromium.base.test.transit.UiThreadCondition;
import org.chromium.base.test.transit.ViewElement;
import org.chromium.base.test.transit.ViewSpec;
import org.chromium.chrome.browser.ChromeTabbedActivity;
import org.chromium.chrome.browser.browsing_data.ClearBrowsingDataFragment;
import org.chromium.chrome.browser.browsing_data.TimePeriod;
import org.chromium.chrome.browser.browsing_data.TimePeriodUtils;
import org.chromium.chrome.browser.browsing_data.TimePeriodUtils.TimePeriodSpinnerOption;
import org.chromium.chrome.test.R;
import org.chromium.chrome.test.transit.hub.RegularTabSwitcherStation;
import org.chromium.chrome.test.transit.settings.SettingsStation;
import org.chromium.components.browser_ui.modaldialog.ModalDialogView;

/** Confirmation dialog that appears when "Delete browsing data" is chosen from the app menu. */
public class QuickDeleteDialogFacility extends Facility<Station<ChromeTabbedActivity>> {
    public static final ViewSpec DIALOG = viewSpec(withId(R.id.modal_dialog_view));
    public static final ViewSpec CUSTOM_VIEW =
            DIALOG.descendant(withId(R.id.custom_view_not_in_scrollable));
    public static final ViewSpec TITLE =
            DIALOG.descendant(withText(R.string.quick_delete_dialog_title));
    public static final ViewSpec SPINNER = DIALOG.descendant(withId(R.id.quick_delete_spinner));
    public static final ViewSpec HISTORY_INFO =
            DIALOG.descendant(withId(R.id.quick_delete_history_row_title));
    public static final ViewSpec HISTORY_SYNCED_INFO =
            DIALOG.descendant(withId(R.id.quick_delete_history_row_subtitle));
    public static final ViewSpec TABS_INFO =
            DIALOG.descendant(withId(R.id.quick_delete_tabs_row_title));
    public static final ViewSpec COOKIES_INFO =
            DIALOG.descendant(
                    withText(R.string.quick_delete_dialog_cookies_cache_and_other_site_data_text));
    public static final ViewSpec MORE_OPTIONS =
            DIALOG.descendant(withId(R.id.quick_delete_more_options));
    public static final ViewSpec SEARCH_HISTORY_DISAMBIGUATION =
            DIALOG.descendant(withId(R.id.search_history_disambiguation));
    public static final ViewSpec CANCEL_BUTTON =
            DIALOG.descendant(withId(R.id.negative_button), withText("Cancel"));
    public static final ViewSpec DELETE_BUTTON =
            DIALOG.descendant(withId(R.id.positive_button), withText("Delete data"));

    private final int mTimePeriod;

    private ViewElement mDialog;
    private ViewElement mCustomView;
    private ViewElement mSpinner;
    private ViewElement mHistoryInfo;
    private ViewElement mHistorySyncedInfo;
    private ViewElement mTabsInfo;

    public QuickDeleteDialogFacility() {
        // Default selection is LAST_15_MINUTES.
        this(TimePeriod.LAST_15_MINUTES);
    }

    public QuickDeleteDialogFacility(@TimePeriod int timePeriod) {
        mTimePeriod = timePeriod;
    }

    @Override
    public void declareElements(Elements.Builder elements) {
        mDialog = elements.declareView(DIALOG);
        mCustomView = elements.declareView(CUSTOM_VIEW);
        elements.declareView(TITLE);
        mSpinner = elements.declareView(SPINNER);
        mHistoryInfo = elements.declareView(HISTORY_INFO);
        mTabsInfo = elements.declareView(TABS_INFO, ViewElement.allowDisabledOption());
        elements.declareView(COOKIES_INFO);
        elements.declareView(MORE_OPTIONS);
        elements.declareView(CANCEL_BUTTON);
        elements.declareView(DELETE_BUTTON);
        elements.declareEnterCondition(new TimePeriodSelectedCondition());
    }

    /** Click Cancel to close the dialog with no action. */
    public void clickCancel() {
        mHostStation.exitFacilitySync(this, CANCEL_BUTTON::click);
    }

    /**
     * Click Delete to delete the selected time period, go to the tab switcher and show a snackbar.
     */
    public Pair<RegularTabSwitcherStation, QuickDeleteSnackbarFacility> confirmDelete() {
        RegularTabSwitcherStation tabSwitcher =
                new RegularTabSwitcherStation(
                        /* regularTabsExist= */ true, /* incognitoTabsExist= */ false);
        QuickDeleteSnackbarFacility snackbar = new QuickDeleteSnackbarFacility(mTimePeriod);
        tabSwitcher.addInitialFacility(snackbar);

        mHostStation.travelToSync(tabSwitcher, DELETE_BUTTON::click);

        return Pair.create(tabSwitcher, snackbar);
    }

    /** Returns the parent ModalDialogView. */
    public ModalDialogView getModalDialog() {
        assertSuppliersCanBeUsed();
        return (ModalDialogView) mDialog.get();
    }

    /** Returns the custom View in the ModalDialog (without the button bar). */
    public View getModalDialogCustomView() {
        assertSuppliersCanBeUsed();
        return (View) mCustomView.get();
    }

    /** Returns the Spinner to select the time period to delete. */
    public Spinner getSpinner() {
        assertSuppliersCanBeUsed();
        return (Spinner) mSpinner.get();
    }

    public TextView getHistoryInfo() {
        assertSuppliersCanBeUsed();
        return (TextView) mHistoryInfo.get();
    }

    public TextView getTabsInfo() {
        assertSuppliersCanBeUsed();
        return (TextView) mTabsInfo.get();
    }

    /** Set the time period to delete in the Spinner. */
    public QuickDeleteDialogFacility setTimePeriodInSpinner(@TimePeriod int timePeriod) {
        TimePeriodSpinnerOption[] options =
                TimePeriodUtils.getTimePeriodSpinnerOptions(mHostStation.getActivity());
        final int positionToSet = getSpinnerPositionForTimePeriod(timePeriod, options);

        return mHostStation.swapFacilitySync(
                this,
                new QuickDeleteDialogFacility(timePeriod),
                () -> ThreadUtils.runOnUiThread(() -> getSpinner().setSelection(positionToSet)));
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

    /** Press the back button to dismiss the dialog. */
    public void pressBackToDismiss() {
        mHostStation.exitFacilitySync(this, Espresso::pressBack);
    }

    /** Click the "More options" button to open the in Settings. */
    public SettingsStation<ClearBrowsingDataFragment> clickMoreOptions() {
        return mHostStation.travelToSync(
                new SettingsStation<>(ClearBrowsingDataFragment.class), MORE_OPTIONS::click);
    }

    public SearchHistoryDisambiguiationFacility expectSearchHistoryDisambiguation(boolean shown) {
        return mHostStation.enterFacilitySync(
                new SearchHistoryDisambiguiationFacility(shown), /* trigger= */ null);
    }

    public SitesSubtitleFacility expectMoreOnSyncedDevices(boolean shown) {
        return mHostStation.enterFacilitySync(
                new SitesSubtitleFacility(shown), /* trigger= */ null);
    }

    private class TimePeriodSelectedCondition extends UiThreadCondition {
        public TimePeriodSelectedCondition() {
            dependOnSupplier(mSpinner, "Spinner View");
        }

        @Override
        protected ConditionStatus checkWithSuppliers() {
            Spinner spinner = (Spinner) mSpinner.get();
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

    public static class SearchHistoryDisambiguiationFacility
            extends Facility<Station<ChromeTabbedActivity>> {
        private final boolean mExpectPresent;

        public SearchHistoryDisambiguiationFacility(boolean expectPresent) {
            mExpectPresent = expectPresent;
        }

        @Override
        public void declareElements(Elements.Builder elements) {
            if (mExpectPresent) {
                elements.declareView(SEARCH_HISTORY_DISAMBIGUATION);
            } else {
                elements.declareNoView(SEARCH_HISTORY_DISAMBIGUATION);
            }
        }
    }

    public static class SitesSubtitleFacility extends Facility<Station<ChromeTabbedActivity>> {
        private final boolean mExpectPresent;

        public SitesSubtitleFacility(boolean expectPresent) {
            mExpectPresent = expectPresent;
        }

        @Override
        public void declareElements(Elements.Builder elements) {
            if (mExpectPresent) {
                elements.declareView(HISTORY_SYNCED_INFO);
            } else {
                elements.declareNoView(HISTORY_SYNCED_INFO);
            }
        }
    }
}
