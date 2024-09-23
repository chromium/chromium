// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
package org.chromium.chrome.browser.ui.google_bottom_bar;

import static org.chromium.chrome.browser.ui.google_bottom_bar.GoogleBottomBarLogger.BOTTOM_BAR_CREATED_HISTOGRAM;
import static org.chromium.chrome.browser.ui.google_bottom_bar.GoogleBottomBarLogger.BOTTOM_BAR_VARIANT_CREATED_HISTOGRAM;
import static org.chromium.chrome.browser.ui.google_bottom_bar.GoogleBottomBarLogger.BUTTON_CLICKED_HISTOGRAM;
import static org.chromium.chrome.browser.ui.google_bottom_bar.GoogleBottomBarLogger.BUTTON_SHOWN_HISTOGRAM;
import static org.chromium.chrome.browser.ui.google_bottom_bar.GoogleBottomBarLogger.BUTTON_UPDATED_HISTOGRAM;

import org.junit.After;
import org.junit.Test;
import org.junit.runner.RunWith;
import org.robolectric.annotation.Config;

import org.chromium.base.test.BaseRobolectricTestRunner;
import org.chromium.base.test.util.HistogramWatcher;
import org.chromium.chrome.browser.ui.google_bottom_bar.GoogleBottomBarLogger.GoogleBottomBarButtonEvent;
import org.chromium.chrome.browser.ui.google_bottom_bar.GoogleBottomBarLogger.GoogleBottomBarCreatedEvent;
import org.chromium.chrome.browser.ui.google_bottom_bar.GoogleBottomBarLogger.GoogleBottomBarVariantCreatedEvent;

/** Unit tests for {@link GoogleBottomBarLogger}. */
@RunWith(BaseRobolectricTestRunner.class)
@Config(manifest = Config.NONE)
public class GoogleBottomBarLoggerTest {

    private HistogramWatcher mHistogramWatcher;

    @After
    public void tearDown() {
        if (mHistogramWatcher != null) {
            mHistogramWatcher.assertExpected();
            mHistogramWatcher.close();
            mHistogramWatcher = null;
        }
    }

    @Test
    public void logCreatedEvent_evenLayout_logsEvenLayout() {
        mHistogramWatcher =
                HistogramWatcher.newSingleRecordWatcher(
                        BOTTOM_BAR_CREATED_HISTOGRAM, GoogleBottomBarCreatedEvent.EVEN_LAYOUT);

        GoogleBottomBarLogger.logCreatedEvent(GoogleBottomBarCreatedEvent.EVEN_LAYOUT);
    }

    @Test
    public void logCreatedEvent_spotlightLayout_logsSpotlightLayout() {
        mHistogramWatcher =
                HistogramWatcher.newSingleRecordWatcher(
                        BOTTOM_BAR_CREATED_HISTOGRAM, GoogleBottomBarCreatedEvent.SPOTLIGHT_LAYOUT);

        GoogleBottomBarLogger.logCreatedEvent(GoogleBottomBarCreatedEvent.SPOTLIGHT_LAYOUT);
    }

    @Test(expected = AssertionError.class)
    public void logCreatedEvent_unsupportedLayout_doesNotLogEvent() {
        mHistogramWatcher =
                HistogramWatcher.newBuilder().expectNoRecords(BOTTOM_BAR_CREATED_HISTOGRAM).build();

        GoogleBottomBarLogger.logCreatedEvent(GoogleBottomBarCreatedEvent.COUNT);
    }

    @Test
    public void logVariantCreatedEvent_unknownVariant_logsUnknownVariant() {
        mHistogramWatcher =
                HistogramWatcher.newSingleRecordWatcher(
                        BOTTOM_BAR_VARIANT_CREATED_HISTOGRAM,
                        GoogleBottomBarVariantCreatedEvent.UNKNOWN_VARIANT);

        GoogleBottomBarLogger.logVariantCreatedEvent(
                GoogleBottomBarVariantCreatedEvent.UNKNOWN_VARIANT);
    }

    @Test
    public void logVariantCreatedEvent_noVariant_logsNoVariant() {
        mHistogramWatcher =
                HistogramWatcher.newSingleRecordWatcher(
                        BOTTOM_BAR_VARIANT_CREATED_HISTOGRAM,
                        GoogleBottomBarVariantCreatedEvent.NO_VARIANT);

        GoogleBottomBarLogger.logVariantCreatedEvent(GoogleBottomBarVariantCreatedEvent.NO_VARIANT);
    }

    @Test
    public void logVariantCreatedEvent_doubleDeckerVariant_logsDoubleDecker() {
        mHistogramWatcher =
                HistogramWatcher.newSingleRecordWatcher(
                        BOTTOM_BAR_VARIANT_CREATED_HISTOGRAM,
                        GoogleBottomBarVariantCreatedEvent.DOUBLE_DECKER);

        GoogleBottomBarLogger.logVariantCreatedEvent(
                GoogleBottomBarVariantCreatedEvent.DOUBLE_DECKER);
    }

    @Test
    public void logVariantCreatedEvent_singleDeckerVariant_logsSingleDecker() {
        mHistogramWatcher =
                HistogramWatcher.newSingleRecordWatcher(
                        BOTTOM_BAR_VARIANT_CREATED_HISTOGRAM,
                        GoogleBottomBarVariantCreatedEvent.SINGLE_DECKER);

        GoogleBottomBarLogger.logVariantCreatedEvent(
                GoogleBottomBarVariantCreatedEvent.SINGLE_DECKER);
    }

    @Test
    public void
            logVariantCreatedEvent_singleDeckerWithRightButtonsVariant_logsSingleDeckerWithRightButtons() {
        mHistogramWatcher =
                HistogramWatcher.newSingleRecordWatcher(
                        BOTTOM_BAR_VARIANT_CREATED_HISTOGRAM,
                        GoogleBottomBarVariantCreatedEvent.SINGLE_DECKER_WITH_RIGHT_BUTTONS);

        GoogleBottomBarLogger.logVariantCreatedEvent(
                GoogleBottomBarVariantCreatedEvent.SINGLE_DECKER_WITH_RIGHT_BUTTONS);
    }

    @Test(expected = AssertionError.class)
    public void logVariantCreatedEvent_unsupportedVariant_doesNotLogEvent() {
        mHistogramWatcher =
                HistogramWatcher.newBuilder()
                        .expectNoRecords(BOTTOM_BAR_VARIANT_CREATED_HISTOGRAM)
                        .build();

        GoogleBottomBarLogger.logVariantCreatedEvent(GoogleBottomBarVariantCreatedEvent.COUNT);
    }

    @Test
    public void logButtonShown_unknownButton_logsUnknownButton() {
        mHistogramWatcher =
                HistogramWatcher.newSingleRecordWatcher(
                        BUTTON_SHOWN_HISTOGRAM, GoogleBottomBarButtonEvent.UNKNOWN);

        GoogleBottomBarLogger.logButtonShown(GoogleBottomBarButtonEvent.UNKNOWN);
    }

    @Test
    public void logButtonShown_pihChromeButton_logsPihChromeButton() {
        mHistogramWatcher =
                HistogramWatcher.newSingleRecordWatcher(
                        BUTTON_SHOWN_HISTOGRAM, GoogleBottomBarButtonEvent.PIH_CHROME);

        GoogleBottomBarLogger.logButtonShown(GoogleBottomBarButtonEvent.PIH_CHROME);
    }

    @Test
    public void logButtonShown_pihEmbedderButton_logsPihEmbedderButton() {
        mHistogramWatcher =
                HistogramWatcher.newSingleRecordWatcher(
                        BUTTON_SHOWN_HISTOGRAM, GoogleBottomBarButtonEvent.PIH_EMBEDDER);

        GoogleBottomBarLogger.logButtonShown(GoogleBottomBarButtonEvent.PIH_EMBEDDER);
    }

    @Test
    public void logButtonShown_saveDisabledButton_logsSaveDisabledButton() {
        mHistogramWatcher =
                HistogramWatcher.newSingleRecordWatcher(
                        BUTTON_SHOWN_HISTOGRAM, GoogleBottomBarButtonEvent.SAVE_DISABLED);

        GoogleBottomBarLogger.logButtonShown(GoogleBottomBarButtonEvent.SAVE_DISABLED);
    }

    @Test
    public void logButtonShown_saveEmbedderButton_logsSaveEmbedderButton() {
        mHistogramWatcher =
                HistogramWatcher.newSingleRecordWatcher(
                        BUTTON_SHOWN_HISTOGRAM, GoogleBottomBarButtonEvent.SAVE_EMBEDDER);

        GoogleBottomBarLogger.logButtonShown(GoogleBottomBarButtonEvent.SAVE_EMBEDDER);
    }

    @Test
    public void logButtonShown_shareChromeButton_logsShareChromeButton() {
        mHistogramWatcher =
                HistogramWatcher.newSingleRecordWatcher(
                        BUTTON_SHOWN_HISTOGRAM, GoogleBottomBarButtonEvent.SHARE_CHROME);

        GoogleBottomBarLogger.logButtonShown(GoogleBottomBarButtonEvent.SHARE_CHROME);
    }

    @Test
    public void logButtonShown_shareEmbedderButton_logsShareEmbedderButton() {
        mHistogramWatcher =
                HistogramWatcher.newSingleRecordWatcher(
                        BUTTON_SHOWN_HISTOGRAM, GoogleBottomBarButtonEvent.SHARE_EMBEDDER);

        GoogleBottomBarLogger.logButtonShown(GoogleBottomBarButtonEvent.SHARE_EMBEDDER);
    }

    @Test
    public void logButtonShown_customEmbedderButton_logsCustomEmbedderButton() {
        mHistogramWatcher =
                HistogramWatcher.newSingleRecordWatcher(
                        BUTTON_SHOWN_HISTOGRAM, GoogleBottomBarButtonEvent.CUSTOM_EMBEDDER);

        GoogleBottomBarLogger.logButtonShown(GoogleBottomBarButtonEvent.CUSTOM_EMBEDDER);
    }

    @Test
    public void logButtonShown_searchEmbedderButton_logsSearchEmbedderButton() {
        mHistogramWatcher =
                HistogramWatcher.newSingleRecordWatcher(
                        BUTTON_SHOWN_HISTOGRAM, GoogleBottomBarButtonEvent.SEARCH_EMBEDDER);

        GoogleBottomBarLogger.logButtonShown(GoogleBottomBarButtonEvent.SEARCH_EMBEDDER);
    }

    @Test
    public void logButtonShown_searchChromeButton_logsSearchChromeButton() {
        mHistogramWatcher =
                HistogramWatcher.newSingleRecordWatcher(
                        BUTTON_SHOWN_HISTOGRAM, GoogleBottomBarButtonEvent.SEARCH_CHROME);

        GoogleBottomBarLogger.logButtonShown(GoogleBottomBarButtonEvent.SEARCH_CHROME);
    }

    @Test
    public void logButtonShown_homeEmbedderButton_logsHomeEmbedderButton() {
        mHistogramWatcher =
                HistogramWatcher.newSingleRecordWatcher(
                        BUTTON_SHOWN_HISTOGRAM, GoogleBottomBarButtonEvent.HOME_EMBEDDER);

        GoogleBottomBarLogger.logButtonShown(GoogleBottomBarButtonEvent.HOME_EMBEDDER);
    }

    @Test
    public void logButtonShown_homeChromeButton_logsHomeChromeButton() {
        mHistogramWatcher =
                HistogramWatcher.newSingleRecordWatcher(
                        BUTTON_SHOWN_HISTOGRAM, GoogleBottomBarButtonEvent.HOME_CHROME);

        GoogleBottomBarLogger.logButtonShown(GoogleBottomBarButtonEvent.HOME_CHROME);
    }

    @Test
    public void logButtonShown_searchboxHomeButton_logsSearchboxHomeButton() {
        mHistogramWatcher =
                HistogramWatcher.newSingleRecordWatcher(
                        BUTTON_SHOWN_HISTOGRAM, GoogleBottomBarButtonEvent.SEARCHBOX_HOME);

        GoogleBottomBarLogger.logButtonShown(GoogleBottomBarButtonEvent.SEARCHBOX_HOME);
    }

    @Test
    public void logButtonShown_searchboxSearchButton_logsSearchboxSearchButton() {
        mHistogramWatcher =
                HistogramWatcher.newSingleRecordWatcher(
                        BUTTON_SHOWN_HISTOGRAM, GoogleBottomBarButtonEvent.SEARCHBOX_SEARCH);

        GoogleBottomBarLogger.logButtonShown(GoogleBottomBarButtonEvent.SEARCHBOX_SEARCH);
    }

    @Test
    public void logButtonShown_searchboxSearchVoiceButton_logsSearchboxSearchVoiceButton() {
        mHistogramWatcher =
                HistogramWatcher.newSingleRecordWatcher(
                        BUTTON_SHOWN_HISTOGRAM, GoogleBottomBarButtonEvent.SEARCHBOX_VOICE_SEARCH);

        GoogleBottomBarLogger.logButtonShown(GoogleBottomBarButtonEvent.SEARCHBOX_VOICE_SEARCH);
    }

    @Test
    public void logButtonShown_searchboxLensButton_logsSearchboxLensButton() {
        mHistogramWatcher =
                HistogramWatcher.newSingleRecordWatcher(
                        BUTTON_SHOWN_HISTOGRAM, GoogleBottomBarButtonEvent.SEARCHBOX_LENS);

        GoogleBottomBarLogger.logButtonShown(GoogleBottomBarButtonEvent.SEARCHBOX_LENS);
    }

    @Test(expected = AssertionError.class)
    public void logButtonShown_unsupportedButton_doesNotLogEvent() {
        mHistogramWatcher =
                HistogramWatcher.newBuilder().expectNoRecords(BUTTON_SHOWN_HISTOGRAM).build();

        GoogleBottomBarLogger.logButtonShown(GoogleBottomBarButtonEvent.COUNT);
    }

    @Test
    public void logButtonClicked_unknownButton_logsUnknownButton() {
        mHistogramWatcher =
                HistogramWatcher.newSingleRecordWatcher(
                        BUTTON_CLICKED_HISTOGRAM, GoogleBottomBarButtonEvent.UNKNOWN);

        GoogleBottomBarLogger.logButtonClicked(GoogleBottomBarButtonEvent.UNKNOWN);
    }

    @Test
    public void logButtonClicked_pihChromeButton_logsPihChromeButton() {
        mHistogramWatcher =
                HistogramWatcher.newSingleRecordWatcher(
                        BUTTON_CLICKED_HISTOGRAM, GoogleBottomBarButtonEvent.PIH_CHROME);

        GoogleBottomBarLogger.logButtonClicked(GoogleBottomBarButtonEvent.PIH_CHROME);
    }

    @Test
    public void logButtonClicked_pihEmbedderButton_logsPihEmbedderButton() {
        mHistogramWatcher =
                HistogramWatcher.newSingleRecordWatcher(
                        BUTTON_CLICKED_HISTOGRAM, GoogleBottomBarButtonEvent.PIH_EMBEDDER);

        GoogleBottomBarLogger.logButtonClicked(GoogleBottomBarButtonEvent.PIH_EMBEDDER);
    }

    @Test
    public void logButtonClicked_saveDisabledButton_logsSaveDisabledButton() {
        mHistogramWatcher =
                HistogramWatcher.newSingleRecordWatcher(
                        BUTTON_CLICKED_HISTOGRAM, GoogleBottomBarButtonEvent.SAVE_DISABLED);

        GoogleBottomBarLogger.logButtonClicked(GoogleBottomBarButtonEvent.SAVE_DISABLED);
    }

    @Test
    public void logButtonClicked_saveEmbedderButton_logsSaveEmbedderButton() {
        mHistogramWatcher =
                HistogramWatcher.newSingleRecordWatcher(
                        BUTTON_CLICKED_HISTOGRAM, GoogleBottomBarButtonEvent.SAVE_EMBEDDER);

        GoogleBottomBarLogger.logButtonClicked(GoogleBottomBarButtonEvent.SAVE_EMBEDDER);
    }

    @Test
    public void logButtonClicked_shareChromeButton_logsShareChromeButton() {
        mHistogramWatcher =
                HistogramWatcher.newSingleRecordWatcher(
                        BUTTON_CLICKED_HISTOGRAM, GoogleBottomBarButtonEvent.SHARE_CHROME);

        GoogleBottomBarLogger.logButtonClicked(GoogleBottomBarButtonEvent.SHARE_CHROME);
    }

    @Test
    public void logButtonClicked_shareEmbedderButton_logsShareEmbedderButton() {
        mHistogramWatcher =
                HistogramWatcher.newSingleRecordWatcher(
                        BUTTON_CLICKED_HISTOGRAM, GoogleBottomBarButtonEvent.SHARE_EMBEDDER);

        GoogleBottomBarLogger.logButtonClicked(GoogleBottomBarButtonEvent.SHARE_EMBEDDER);
    }

    @Test
    public void logButtonClicked_customEmbedderButton_logsCustomEmbedderButton() {
        mHistogramWatcher =
                HistogramWatcher.newSingleRecordWatcher(
                        BUTTON_CLICKED_HISTOGRAM, GoogleBottomBarButtonEvent.CUSTOM_EMBEDDER);

        GoogleBottomBarLogger.logButtonClicked(GoogleBottomBarButtonEvent.CUSTOM_EMBEDDER);
    }

    @Test
    public void logButtonClicked_searchEmbedderButton_logsSearchEmbedderButton() {
        mHistogramWatcher =
                HistogramWatcher.newSingleRecordWatcher(
                        BUTTON_CLICKED_HISTOGRAM, GoogleBottomBarButtonEvent.SEARCH_EMBEDDER);

        GoogleBottomBarLogger.logButtonClicked(GoogleBottomBarButtonEvent.SEARCH_EMBEDDER);
    }

    @Test
    public void logButtonClicked_searchChromeButton_logsSearchChromeButton() {
        mHistogramWatcher =
                HistogramWatcher.newSingleRecordWatcher(
                        BUTTON_CLICKED_HISTOGRAM, GoogleBottomBarButtonEvent.SEARCH_CHROME);

        GoogleBottomBarLogger.logButtonClicked(GoogleBottomBarButtonEvent.SEARCH_CHROME);
    }

    @Test
    public void logButtonClicked_homeEmbedderButton_logsHomeEmbedderButton() {
        mHistogramWatcher =
                HistogramWatcher.newSingleRecordWatcher(
                        BUTTON_CLICKED_HISTOGRAM, GoogleBottomBarButtonEvent.HOME_EMBEDDER);

        GoogleBottomBarLogger.logButtonClicked(GoogleBottomBarButtonEvent.HOME_EMBEDDER);
    }

    @Test
    public void logButtonClicked_homeChromeButton_logsHomeChromeButton() {
        mHistogramWatcher =
                HistogramWatcher.newSingleRecordWatcher(
                        BUTTON_CLICKED_HISTOGRAM, GoogleBottomBarButtonEvent.HOME_CHROME);

        GoogleBottomBarLogger.logButtonClicked(GoogleBottomBarButtonEvent.HOME_CHROME);
    }

    @Test
    public void logButtonClicked_searchboxHomeButton_logsSearchboxHomeButton() {
        mHistogramWatcher =
                HistogramWatcher.newSingleRecordWatcher(
                        BUTTON_CLICKED_HISTOGRAM, GoogleBottomBarButtonEvent.SEARCHBOX_HOME);

        GoogleBottomBarLogger.logButtonClicked(GoogleBottomBarButtonEvent.SEARCHBOX_HOME);
    }

    @Test
    public void logButtonClicked_searchboxSearchButton_logsSearchboxSearchButton() {
        mHistogramWatcher =
                HistogramWatcher.newSingleRecordWatcher(
                        BUTTON_CLICKED_HISTOGRAM, GoogleBottomBarButtonEvent.SEARCHBOX_SEARCH);

        GoogleBottomBarLogger.logButtonClicked(GoogleBottomBarButtonEvent.SEARCHBOX_SEARCH);
    }

    @Test
    public void logButtonClicked_searchboxSearchVoiceButton_logsSearchboxSearchVoiceButton() {
        mHistogramWatcher =
                HistogramWatcher.newSingleRecordWatcher(
                        BUTTON_CLICKED_HISTOGRAM,
                        GoogleBottomBarButtonEvent.SEARCHBOX_VOICE_SEARCH);

        GoogleBottomBarLogger.logButtonClicked(GoogleBottomBarButtonEvent.SEARCHBOX_VOICE_SEARCH);
    }

    @Test
    public void logButtonClicked_searchboxLensButton_logsSearchboxLensButton() {
        mHistogramWatcher =
                HistogramWatcher.newSingleRecordWatcher(
                        BUTTON_CLICKED_HISTOGRAM, GoogleBottomBarButtonEvent.SEARCHBOX_LENS);

        GoogleBottomBarLogger.logButtonClicked(GoogleBottomBarButtonEvent.SEARCHBOX_LENS);
    }

    @Test(expected = AssertionError.class)
    public void logButtonClicked_unsupportedButton_doesNotLogEvent() {
        mHistogramWatcher =
                HistogramWatcher.newBuilder().expectNoRecords(BUTTON_CLICKED_HISTOGRAM).build();

        GoogleBottomBarLogger.logButtonClicked(GoogleBottomBarButtonEvent.COUNT);
    }

    @Test
    public void logButtonUpdated_unknownButton_logsUnknownButton() {
        mHistogramWatcher =
                HistogramWatcher.newSingleRecordWatcher(
                        BUTTON_UPDATED_HISTOGRAM, GoogleBottomBarButtonEvent.UNKNOWN);

        GoogleBottomBarLogger.logButtonUpdated(GoogleBottomBarButtonEvent.UNKNOWN);
    }

    @Test
    public void logButtonUpdated_pihChromeButton_logsPihChromeButton() {
        mHistogramWatcher =
                HistogramWatcher.newSingleRecordWatcher(
                        BUTTON_UPDATED_HISTOGRAM, GoogleBottomBarButtonEvent.PIH_CHROME);

        GoogleBottomBarLogger.logButtonUpdated(GoogleBottomBarButtonEvent.PIH_CHROME);
    }

    @Test
    public void logButtonUpdated_pihEmbedderButton_logsPihEmbedderButton() {
        mHistogramWatcher =
                HistogramWatcher.newSingleRecordWatcher(
                        BUTTON_UPDATED_HISTOGRAM, GoogleBottomBarButtonEvent.PIH_EMBEDDER);

        GoogleBottomBarLogger.logButtonUpdated(GoogleBottomBarButtonEvent.PIH_EMBEDDER);
    }

    @Test
    public void logButtonUpdated_saveDisabledButton_logsSaveDisabledButton() {
        mHistogramWatcher =
                HistogramWatcher.newSingleRecordWatcher(
                        BUTTON_UPDATED_HISTOGRAM, GoogleBottomBarButtonEvent.SAVE_DISABLED);

        GoogleBottomBarLogger.logButtonUpdated(GoogleBottomBarButtonEvent.SAVE_DISABLED);
    }

    @Test
    public void logButtonUpdated_saveEmbedderButton_logsSaveEmbedderButton() {
        mHistogramWatcher =
                HistogramWatcher.newSingleRecordWatcher(
                        BUTTON_UPDATED_HISTOGRAM, GoogleBottomBarButtonEvent.SAVE_EMBEDDER);

        GoogleBottomBarLogger.logButtonUpdated(GoogleBottomBarButtonEvent.SAVE_EMBEDDER);
    }

    @Test
    public void logButtonUpdated_shareChromeButton_logsShareChromeButton() {
        mHistogramWatcher =
                HistogramWatcher.newSingleRecordWatcher(
                        BUTTON_UPDATED_HISTOGRAM, GoogleBottomBarButtonEvent.SHARE_CHROME);

        GoogleBottomBarLogger.logButtonUpdated(GoogleBottomBarButtonEvent.SHARE_CHROME);
    }

    @Test
    public void logButtonUpdated_shareEmbedderButton_logsShareEmbedderButton() {
        mHistogramWatcher =
                HistogramWatcher.newSingleRecordWatcher(
                        BUTTON_UPDATED_HISTOGRAM, GoogleBottomBarButtonEvent.SHARE_EMBEDDER);

        GoogleBottomBarLogger.logButtonUpdated(GoogleBottomBarButtonEvent.SHARE_EMBEDDER);
    }

    @Test
    public void logButtonUpdated_customEmbedderButton_logsCustomEmbedderButton() {
        mHistogramWatcher =
                HistogramWatcher.newSingleRecordWatcher(
                        BUTTON_UPDATED_HISTOGRAM, GoogleBottomBarButtonEvent.CUSTOM_EMBEDDER);

        GoogleBottomBarLogger.logButtonUpdated(GoogleBottomBarButtonEvent.CUSTOM_EMBEDDER);
    }

    @Test
    public void logButtonUpdated_searchEmbedderButton_logsSearchEmbedderButton() {
        mHistogramWatcher =
                HistogramWatcher.newSingleRecordWatcher(
                        BUTTON_UPDATED_HISTOGRAM, GoogleBottomBarButtonEvent.SEARCH_EMBEDDER);

        GoogleBottomBarLogger.logButtonUpdated(GoogleBottomBarButtonEvent.SEARCH_EMBEDDER);
    }

    @Test
    public void logButtonUpdated_searchChromeButton_logsSearchChromeButton() {
        mHistogramWatcher =
                HistogramWatcher.newSingleRecordWatcher(
                        BUTTON_UPDATED_HISTOGRAM, GoogleBottomBarButtonEvent.SEARCH_CHROME);

        GoogleBottomBarLogger.logButtonUpdated(GoogleBottomBarButtonEvent.SEARCH_CHROME);
    }

    @Test
    public void logButtonUpdated_homeEmbedderButton_logsHomeEmbedderButton() {
        mHistogramWatcher =
                HistogramWatcher.newSingleRecordWatcher(
                        BUTTON_UPDATED_HISTOGRAM, GoogleBottomBarButtonEvent.HOME_EMBEDDER);

        GoogleBottomBarLogger.logButtonUpdated(GoogleBottomBarButtonEvent.HOME_EMBEDDER);
    }

    @Test
    public void logButtonUpdated_homeChromeButton_logsHomeChromeButton() {
        mHistogramWatcher =
                HistogramWatcher.newSingleRecordWatcher(
                        BUTTON_UPDATED_HISTOGRAM, GoogleBottomBarButtonEvent.HOME_CHROME);

        GoogleBottomBarLogger.logButtonUpdated(GoogleBottomBarButtonEvent.HOME_CHROME);
    }

    @Test
    public void logButtonUpdated_searchboxHomeButton_logsSearchboxHomeButton() {
        mHistogramWatcher =
                HistogramWatcher.newSingleRecordWatcher(
                        BUTTON_UPDATED_HISTOGRAM, GoogleBottomBarButtonEvent.SEARCHBOX_HOME);

        GoogleBottomBarLogger.logButtonUpdated(GoogleBottomBarButtonEvent.SEARCHBOX_HOME);
    }

    @Test
    public void logButtonUpdated_searchboxSearchButton_logsSearchboxSearchButton() {
        mHistogramWatcher =
                HistogramWatcher.newSingleRecordWatcher(
                        BUTTON_UPDATED_HISTOGRAM, GoogleBottomBarButtonEvent.SEARCHBOX_SEARCH);

        GoogleBottomBarLogger.logButtonUpdated(GoogleBottomBarButtonEvent.SEARCHBOX_SEARCH);
    }

    @Test
    public void logButtonUpdated_searchboxSearchVoiceButton_logsSearchboxSearchVoiceButton() {
        mHistogramWatcher =
                HistogramWatcher.newSingleRecordWatcher(
                        BUTTON_UPDATED_HISTOGRAM,
                        GoogleBottomBarButtonEvent.SEARCHBOX_VOICE_SEARCH);

        GoogleBottomBarLogger.logButtonUpdated(GoogleBottomBarButtonEvent.SEARCHBOX_VOICE_SEARCH);
    }

    @Test
    public void logButtonUpdated_searchboxLensButton_logsSearchboxLensButton() {
        mHistogramWatcher =
                HistogramWatcher.newSingleRecordWatcher(
                        BUTTON_UPDATED_HISTOGRAM, GoogleBottomBarButtonEvent.SEARCHBOX_LENS);

        GoogleBottomBarLogger.logButtonUpdated(GoogleBottomBarButtonEvent.SEARCHBOX_LENS);
    }

    @Test(expected = AssertionError.class)
    public void logButtonUpdated_unsupportedButton_doesNotLogEvent() {
        mHistogramWatcher =
                HistogramWatcher.newBuilder().expectNoRecords(BUTTON_UPDATED_HISTOGRAM).build();

        GoogleBottomBarLogger.logButtonUpdated(GoogleBottomBarButtonEvent.COUNT);
    }
}
