package org.chromium.chrome.browser.page_insights;

import static com.google.common.truth.Truth.assertThat;

import static org.junit.Assert.assertThrows;

import org.junit.Test;
import org.junit.runner.RunWith;
import org.junit.runners.BlockJUnit4ClassRunner;
import org.robolectric.annotation.LooperMode;
import org.robolectric.annotation.LooperMode.Mode;

import org.chromium.chrome.browser.page_insights.SheetStateTranslator.PageInsightsSheetState;
import org.chromium.components.browser_ui.bottomsheet.BottomSheetController.SheetState;

/** Tests for {@link SheetStateTranslator}. */
@LooperMode(Mode.PAUSED)
@RunWith(BlockJUnit4ClassRunner.class)
public class SheetStateTranslatorTest {

    @Test
    public void testTranslatorConversionsCorrect() throws Exception {
        assertThat(translates(PageInsightsSheetState.EXPANDED, SheetState.FULL)).isTrue();
        assertThat(translates(PageInsightsSheetState.NONE, SheetState.NONE)).isTrue();
        assertThat(translates(PageInsightsSheetState.PEEK, SheetState.FULL)).isFalse();
    }

    @Test
    public void testUnsupportedSheetStateThrowsException() throws Exception {
        assertThrows(
                UnsupportedOperationException.class,
                () ->
                        SheetStateTranslator.toBottomSheetState(
                                PageInsightsSheetState.COLLAPSED));
    }

    private boolean translates(
            @PageInsightsSheetState int pageInsightsSheetState, @SheetState int bottomSheetState)
            throws Exception {
        return SheetStateTranslator.toPageInsightsSheetState(bottomSheetState)
                        == pageInsightsSheetState
                && SheetStateTranslator.toBottomSheetState(
                                pageInsightsSheetState)
                        == bottomSheetState;
    }
}
