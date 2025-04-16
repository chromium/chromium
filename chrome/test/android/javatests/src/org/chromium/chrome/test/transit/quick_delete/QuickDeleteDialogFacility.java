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
    public static final ViewSpec<ModalDialogView> DIALOG =
            viewSpec(ModalDialogView.class, withId(R.id.modal_dialog_view));

    private final int mTimePeriod;

    private ViewElement<ModalDialogView> mDialog;
    private ViewElement<View> mCustomView;
    private ViewElement<Spinner> mSpinner;
    private ViewElement<TextView> mHistoryInfo;
    private ViewElement<TextView> mTabsInfo;
    private ViewElement mMoreOptions;
    private ViewElement mCancelButton;
    private ViewElement mDeleteButton;

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
        mCustomView =
                elements.declareView(DIALOG.descendant(withId(R.id.custom_view_not_in_scrollable)));
        elements.declareView(DIALOG.descendant(withText(R.string.quick_delete_dialog_title)));
        mSpinner =
                elements.declareView(
                        DIALOG.descendant(Spinner.class, withId(R.id.quick_delete_spinner)));
        mHistoryInfo =
                elements.declareView(
                        DIALOG.descendant(
                                TextView.class, withId(R.id.quick_delete_history_row_title)));
        mTabsInfo =
                elements.declareView(
                        DIALOG.descendant(TextView.class, withId(R.id.quick_delete_tabs_row_title)),
                        ViewElement.allowDisabledOption());
        elements.declareView(
                DIALOG.descendant(
                        withText(
                                R.string
                                        .quick_delete_dialog_cookies_cache_and_other_site_data_text)));
        mMoreOptions =
                elements.declareView(DIALOG.descendant(withId(R.id.quick_delete_more_options)));
        mCancelButton =
                elements.declareView(
                        DIALOG.descendant(withId(R.id.negative_button), withText("Cancel")));
        mDeleteButton =
                elements.declareView(
                        DIALOG.descendant(withId(R.id.positive_button), withText("Delete data")));
        elements.declareEnterCondition(new TimePeriodSelectedCondition());
    }

    /** Click Cancel to close the dialog with no action. */
    public void clickCancel() {
        mHostStation.exitFacilitySync(this, mCancelButton.clickTrigger());
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

        mHostStation.travelToSync(tabSwitcher, mDeleteButton.clickTrigger());

        return Pair.create(tabSwitcher, snackbar);
    }

    /** Returns the parent ModalDialogView. */
    public ModalDialogView getModalDialog() {
        return mDialog.get();
    }

    /** Returns the custom View in the ModalDialog (without the button bar). */
    public View getModalDialogCustomView() {
        return mCustomView.get();
    }

    /** Returns the Spinner to select the time period to delete. */
    public Spinner getSpinner() {
        return mSpinner.get();
    }

    public TextView getHistoryInfo() {
        return mHistoryInfo.get();
    }

    public TextView getTabsInfo() {
        return mTabsInfo.get();
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
                new SettingsStation<>(ClearBrowsingDataFragment.class),
                mMoreOptions.clickTrigger());
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
            Spinner spinner = mSpinner.get();
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
            ViewSpec spec = DIALOG.descendant(withId(R.id.search_history_disambiguation));
            if (mExpectPresent) {
                elements.declareView(spec);
            } else {
                elements.declareNoView(spec);
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
            ViewSpec spec = DIALOG.descendant(withId(R.id.quick_delete_history_row_subtitle));
            if (mExpectPresent) {
                elements.declareView(spec);
            } else {
                elements.declareNoView(spec);
            }
        }
    }
}
