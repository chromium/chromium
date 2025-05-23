// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.test.transit.hub;

import static androidx.test.espresso.Espresso.onView;
import static androidx.test.espresso.assertion.ViewAssertions.doesNotExist;
import static androidx.test.espresso.matcher.ViewMatchers.withId;
import static androidx.test.espresso.matcher.ViewMatchers.withText;

import static org.hamcrest.CoreMatchers.instanceOf;

import android.view.View;
import android.widget.EditText;

import org.chromium.base.test.transit.Facility;
import org.chromium.base.test.transit.ViewElement;
import org.chromium.chrome.browser.history.HistoryItemView;
import org.chromium.chrome.browser.hub.PaneId;
import org.chromium.chrome.test.R;
import org.chromium.chrome.test.transit.page.PageStation;
import org.chromium.chrome.test.transit.page.WebPageStation;

/** The History pane station. */
public class HistoryPaneStation extends HubBaseStation {
    public HistoryPaneStation(boolean regularTabsExist, boolean incognitoTabsExist) {
        super(regularTabsExist, incognitoTabsExist, /* hasMenuButton= */ false);
    }

    @Override
    public @PaneId int getPaneId() {
        return PaneId.HISTORY;
    }

    /** Expect history entries to be displayed in the history pane. */
    public HistoryWithEntriesFacility expectEntries() {
        return enterFacilitySync(new HistoryWithEntriesFacility(), /* trigger= */ null);
    }

    /** Expect no history to be displayed in the history pane. */
    public void expectEmptyState() {
        var emptyHistory = new Facility<>("EmptyState");
        emptyHistory.declareView(withText("You’ll find your history here"));
        emptyHistory.declareView(
                withText("You can see the pages you’ve visited or delete them from your history"));
        emptyHistory.declareNoView(withId(R.id.history_page_recycler_view));
        enterFacilitySync(emptyHistory, /* trigger= */ null);
    }

    /** Non-empty state of the history pane. */
    public class HistoryWithEntriesFacility extends Facility<HistoryPaneStation> {
        public final ViewElement<View> recyclerViewElement;
        public final ViewElement<View> searchButtonElement;

        public HistoryWithEntriesFacility() {
            recyclerViewElement = declareView(withId(R.id.history_page_recycler_view));
            searchButtonElement = declareView(withId(R.id.search_menu_id));
        }

        /** Expect an entry to be displayed in the history pane. */
        public HistoryEntryFacility expectEntry(String text) {
            return mHostStation.enterFacilitySync(
                    new HistoryEntryFacility(this, text), /* trigger= */ null);
        }

        /** Expect an entry to be not displayed in the history pane. */
        public void expectNoEntry(String text) {
            onView(withText(text)).check(doesNotExist());
        }

        /** Open the history search. */
        public HistorySearchFacility openSearch() {
            return enterFacilitySync(
                    new HistorySearchFacility(), searchButtonElement.getClickTrigger());
        }
    }

    /** One history entry in the history pane. */
    public class HistoryEntryFacility extends Facility<HistoryPaneStation> {
        public final ViewElement<HistoryItemView> itemElement;
        public final ViewElement<View> titleElement;
        public final ViewElement<View> iconElement;
        public final ViewElement<View> removeButtonElement;

        public HistoryEntryFacility(HistoryWithEntriesFacility resultsFacility, String text) {
            titleElement =
                    declareView(
                            resultsFacility.recyclerViewElement.descendant(
                                    withText(text), withId(R.id.title)));
            itemElement =
                    declareView(
                            titleElement.ancestor(
                                    HistoryItemView.class, instanceOf(HistoryItemView.class)));
            iconElement = declareView(itemElement.descendant(withId(R.id.start_icon)));
            removeButtonElement = declareView(itemElement.descendant(withId(R.id.end_button)));
        }

        /** Select the entry to open. */
        public WebPageStation selectToOpenWebPage(PageStation previousPage, String url) {
            return travelToSync(
                    WebPageStation.newBuilder()
                            .initFrom(previousPage)
                            .withExpectedUrlSubstring(url)
                            .build(),
                    itemElement.getClickTrigger());
        }
    }

    /** Search state in the history pane. */
    public static class HistorySearchFacility extends Facility<HistoryPaneStation> {
        public final ViewElement<EditText> editTextElement;

        public HistorySearchFacility() {
            editTextElement = declareView(EditText.class, withId(R.id.search_text));
        }

        public void typeSearchTerm(String text) {
            editTextElement.getTypeTextTrigger(text).triggerTransition();
        }
    }
}
