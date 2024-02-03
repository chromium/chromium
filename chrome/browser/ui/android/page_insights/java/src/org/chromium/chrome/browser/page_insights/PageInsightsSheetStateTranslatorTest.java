package org.chromium.chrome.browser.page_insights;

import static com.google.common.truth.Truth.assertThat;

import org.junit.Test;
import org.junit.runner.RunWith;
import org.junit.runners.BlockJUnit4ClassRunner;
import org.robolectric.annotation.LooperMode;
import org.robolectric.annotation.LooperMode.Mode;

import org.chromium.chrome.browser.page_insights.PageInsightsSheetStateTranslator.PageInsightsSheetState;
import org.chromium.components.browser_ui.bottomsheet.BottomSheetController.SheetState;

/** Tests for {@link PageInsightsSheetStateTranslator}. */
@LooperMode(Mode.PAUSED)
@RunWith(BlockJUnit4ClassRunner.class)
public class PageInsightsSheetStateTranslatorTest {

    @Test
    public void testTranslatorConversionsCorrect() {
        assertThat(translates(PageInsightsSheetState.EXPANDED, SheetState.FULL)).isTrue();
        assertThat(translates(PageInsightsSheetState.NONE, SheetState.NONE)).isTrue();
        assertThat(translates(PageInsightsSheetState.PEEK, SheetState.FULL)).isFalse();
    }

    private boolean translates(
            @PageInsightsSheetState int pageInsightsSheetState, @SheetState int bottomSheetState) {
        return PageInsightsSheetStateTranslator.convertToPageInsightsSheetState(bottomSheetState)
                        == pageInsightsSheetState
                && PageInsightsSheetStateTranslator.convertToBottomSheetState(
                                pageInsightsSheetState)
                        == bottomSheetState;
    }
}
