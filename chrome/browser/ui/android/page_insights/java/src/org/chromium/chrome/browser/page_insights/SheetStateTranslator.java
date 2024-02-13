package org.chromium.chrome.browser.page_insights;

import androidx.annotation.IntDef;

import org.chromium.components.browser_ui.bottomsheet.BottomSheetController.SheetState;

import java.lang.annotation.Retention;
import java.lang.annotation.RetentionPolicy;

/**
 * Class that translates Page Insights Hub terminology into {@link SheetState} terminology to ensure
 * that there is no confusion when interpreting the codebase.
 *
 * <p>TODO - add COLLAPSE state once it is supported.
 * Page Insights Hub        Bottom Sheet
 * NONE                     NONE
 * HIDDEN                   HIDEEN
 * PEEK                     PEEK
 * EXPANDED                 FULL
 * SCROLLING                SCROLLING
 */
public class SheetStateTranslator {

    /** The different states that the PageInsightsHub can have. */
    @IntDef({
        PageInsightsSheetState.NONE,
        PageInsightsSheetState.HIDDEN,
        PageInsightsSheetState.COLLAPSED,
        PageInsightsSheetState.PEEK,
        PageInsightsSheetState.EXPANDED,
        PageInsightsSheetState.SCROLLING,
    })
    @Retention(RetentionPolicy.SOURCE)
    @interface PageInsightsSheetState {
        int NONE = -1; // Default state
        int HIDDEN = 0; // Page Insights Hub is not visible
        int COLLAPSED = 1; // The state where just the bottom bar is visible
        int PEEK = 2; // Page Insights Hub is in this state when auto-peek is triggered
        int EXPANDED = 3; // Page Insights Hub is fully expanded
        int SCROLLING = 4; // Page Insights Hub is in scrolling mode
    }

    @SheetState
    static int toBottomSheetState(@PageInsightsSheetState int sheet) {
        return switch (sheet) {
            case PageInsightsSheetState.NONE -> SheetState.NONE;
            case PageInsightsSheetState.HIDDEN -> SheetState.HIDDEN;
            case PageInsightsSheetState.PEEK -> SheetState.PEEK;
            case PageInsightsSheetState.EXPANDED -> SheetState.FULL;
            case PageInsightsSheetState.SCROLLING -> SheetState.SCROLLING;
                // TODO - support COLLAPSED once we migrate BottomSheet PEEK state to HALF state
            default -> throw new UnsupportedOperationException("COLLAPSED not supported");
        };
    }

    @PageInsightsSheetState
    static int toPageInsightsSheetState(@SheetState int sheet) {
        return switch (sheet) {
            case SheetState.NONE -> PageInsightsSheetState.NONE;
            case SheetState.HIDDEN -> PageInsightsSheetState.HIDDEN;
            case SheetState.PEEK -> PageInsightsSheetState.PEEK;
            case SheetState.FULL -> PageInsightsSheetState.EXPANDED;
            case SheetState.SCROLLING -> PageInsightsSheetState.SCROLLING;
                // TODO - support HALF once we migrate BottomSheet PEEK state to HALF state
            default -> throw new UnsupportedOperationException("HALF state not supported");
        };
    }
}
