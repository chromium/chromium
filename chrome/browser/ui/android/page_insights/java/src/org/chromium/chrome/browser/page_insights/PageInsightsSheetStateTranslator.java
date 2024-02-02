package org.chromium.chrome.browser.page_insights;

import androidx.annotation.IntDef;

import org.chromium.components.browser_ui.bottomsheet.BottomSheetController.SheetState;

import java.lang.annotation.Retention;
import java.lang.annotation.RetentionPolicy;

/**
 * Class that translates Page Insights Hub terminology into {@link SheetState} terminology to ensure
 * that there is no confusion when interpreting the codebase.
 *
 * TODO - add COLLAPSE state once it is supported.
 * Page Insights Hub        Bottom Sheet
 * PEEK                     PEEK
 * EXPANDED                 FULL
 */
public class PageInsightsSheetStateTranslator {

    /** The different states that the PageInsightsHub can have. */
    @IntDef({
        PageInsightsSheetState.NONE,
        PageInsightsSheetState.HIDDEN,
        PageInsightsSheetState.COLLAPSED,
        PageInsightsSheetState.PEEK,
        PageInsightsSheetState.EXPANDED,
    })
    @Retention(RetentionPolicy.SOURCE)
    @interface PageInsightsSheetState {
        int NONE = -1; // Default state
        int HIDDEN = 0; // Page Insights Hub is not visible
        int COLLAPSED = 1; // The state where just the bottom bar is visible
        int PEEK = 2; // Page Insights Hub is in this state when auto-peek is triggered
        int EXPANDED = 3; // Page Insights Hub is fully expanded
    }

    @SheetState
    static int convertToBottomSheetState(@PageInsightsSheetState int sheet) {
        return switch (sheet) {
            case PageInsightsSheetState.HIDDEN -> SheetState.HIDDEN;
                // TODO - support COLLAPSED once we migrate BottomSheet PEEK state to HALF state
            case PageInsightsSheetState.PEEK -> SheetState.PEEK;
            case PageInsightsSheetState.EXPANDED -> SheetState.FULL;
            default -> SheetState.NONE;
        };
    }

    @PageInsightsSheetState
    static int convertToPageInsightsSheetState(@SheetState int sheet) {
        return switch (sheet) {
            case SheetState.HIDDEN -> PageInsightsSheetState.HIDDEN;
                // TODO - support COLLAPSED once we migrate BottomSheet PEEK state to HALF state
            case SheetState.PEEK -> PageInsightsSheetState.PEEK;
            case SheetState.FULL -> PageInsightsSheetState.EXPANDED;
            default -> PageInsightsSheetState.NONE;
        };
    }
}
