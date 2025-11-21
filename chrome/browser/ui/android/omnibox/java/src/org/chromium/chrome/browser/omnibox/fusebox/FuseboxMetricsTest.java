// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.omnibox.fusebox;

import org.junit.Before;
import org.junit.Test;
import org.junit.runner.RunWith;

import org.chromium.base.test.BaseRobolectricTestRunner;
import org.chromium.base.test.util.Features.DisableFeatures;
import org.chromium.base.test.util.Features.EnableFeatures;
import org.chromium.base.test.util.HistogramWatcher;
import org.chromium.chrome.browser.flags.ChromeFeatureList;
import org.chromium.chrome.browser.omnibox.fusebox.FuseboxMetrics.AiModeActivationSource;
import org.chromium.chrome.browser.omnibox.fusebox.FuseboxMetrics.FuseboxAttachmentButtonType;
import org.chromium.components.omnibox.AutocompleteRequestType;
import org.chromium.ui.modelutil.PropertyModel;

@RunWith(BaseRobolectricTestRunner.class)
public class FuseboxMetricsTest {

    private final PropertyModel mPropertyModel = new PropertyModel(FuseboxProperties.ALL_KEYS);

    @Before
    public void setUp() {
        FuseboxMetrics.resetForTesting();
    }

    @Test
    public void testNotifyAiModeActivated() {
        var histogramWatcher =
                HistogramWatcher.newSingleRecordWatcher(
                        "Omnibox.MobileFusebox.AiModeActivationSource",
                        FuseboxMetrics.AiModeActivationSource.DEDICATED_BUTTON);
        FuseboxMetrics.notifyAiModeActivated(
                FuseboxMetrics.AiModeActivationSource.DEDICATED_BUTTON);
        histogramWatcher.assertExpected();

        histogramWatcher =
                HistogramWatcher.newSingleRecordWatcher(
                        "Omnibox.MobileFusebox.AiModeActivationSource",
                        FuseboxMetrics.AiModeActivationSource.TOOL_MENU);
        FuseboxMetrics.notifyAiModeActivated(AiModeActivationSource.TOOL_MENU);
        histogramWatcher.assertExpected();

        histogramWatcher =
                HistogramWatcher.newSingleRecordWatcher(
                        "Omnibox.MobileFusebox.AiModeActivationSource",
                        AiModeActivationSource.IMPLICIT);
        FuseboxMetrics.notifyAiModeActivated(AiModeActivationSource.IMPLICIT);
        histogramWatcher.assertExpected();
    }

    @Test
    public void testNotifyAttachmentButtonUsed() {
        var histogramWatcher =
                HistogramWatcher.newSingleRecordWatcher(
                        "Omnibox.MobileFusebox.AttachmentButtonUsed",
                        FuseboxMetrics.FuseboxAttachmentButtonType.CLIPBOARD);

        FuseboxMetrics.notifyAttachmentButtonUsed(
                FuseboxMetrics.FuseboxAttachmentButtonType.CLIPBOARD);

        histogramWatcher.assertExpected();
    }

    @Test
    @EnableFeatures(ChromeFeatureList.CHROME_ITEM_PICKER_UI)
    public void testNotifyAttachmentsPopupToggled_ShowPopup_AllButtonsVisible() {

        mPropertyModel.set(FuseboxProperties.CURRENT_TAB_BUTTON_VISIBLE, true);
        mPropertyModel.set(FuseboxProperties.POPUP_CLIPBOARD_BUTTON_VISIBLE, true);
        mPropertyModel.set(FuseboxProperties.POPUP_FILE_BUTTON_VISIBLE, true);

        var histogramWatcher =
                HistogramWatcher.newBuilder()
                        .expectBooleanRecord("Omnibox.MobileFusebox.AttachmentsPopupToggled", true)
                        .expectIntRecord(
                                "Omnibox.MobileFusebox.AttachmentButtonShown",
                                FuseboxMetrics.FuseboxAttachmentButtonType.CAMERA)
                        .expectIntRecord(
                                "Omnibox.MobileFusebox.AttachmentButtonShown",
                                FuseboxMetrics.FuseboxAttachmentButtonType.GALLERY)
                        .expectIntRecord(
                                "Omnibox.MobileFusebox.AttachmentButtonShown",
                                FuseboxMetrics.FuseboxAttachmentButtonType.TAB_PICKER)
                        .expectIntRecord(
                                "Omnibox.MobileFusebox.AttachmentButtonShown",
                                FuseboxMetrics.FuseboxAttachmentButtonType.CURRENT_TAB)
                        .expectIntRecord(
                                "Omnibox.MobileFusebox.AttachmentButtonShown",
                                FuseboxMetrics.FuseboxAttachmentButtonType.CLIPBOARD)
                        .expectIntRecord(
                                "Omnibox.MobileFusebox.AttachmentButtonShown",
                                FuseboxMetrics.FuseboxAttachmentButtonType.FILES)
                        .build();

        FuseboxMetrics.notifyAttachmentsPopupToggled(true, mPropertyModel);

        histogramWatcher.assertExpected();
    }

    @Test
    @DisableFeatures(ChromeFeatureList.CHROME_ITEM_PICKER_UI)
    public void testNotifyAttachmentsPopupToggled_ShowPopup_SomeButtonsHidden() {
        mPropertyModel.set(FuseboxProperties.CURRENT_TAB_BUTTON_VISIBLE, false);
        mPropertyModel.set(FuseboxProperties.POPUP_CLIPBOARD_BUTTON_VISIBLE, false);
        mPropertyModel.set(FuseboxProperties.POPUP_FILE_BUTTON_VISIBLE, false);

        var histogramWatcher =
                HistogramWatcher.newBuilder()
                        .expectBooleanRecord("Omnibox.MobileFusebox.AttachmentsPopupToggled", true)
                        // CAMERA
                        .expectIntRecord(
                                "Omnibox.MobileFusebox.AttachmentButtonShown",
                                FuseboxMetrics.FuseboxAttachmentButtonType.CAMERA)
                        // GALLERY
                        .expectIntRecord(
                                "Omnibox.MobileFusebox.AttachmentButtonShown",
                                FuseboxMetrics.FuseboxAttachmentButtonType.GALLERY)
                        .build();

        FuseboxMetrics.notifyAttachmentsPopupToggled(true, mPropertyModel);

        histogramWatcher.assertExpected();
    }

    @Test
    public void testNotifyAttachmentsPopupToggled_HidePopup() {
        var histogramWatcher =
                HistogramWatcher.newSingleRecordWatcher(
                        "Omnibox.MobileFusebox.AttachmentsPopupToggled", false);

        // When hiding the popup, no other metrics should be recorded.
        FuseboxMetrics.notifyAttachmentsPopupToggled(false, mPropertyModel);

        histogramWatcher.assertExpected();
    }

    @Test
    public void testNotifyOmniboxSessionEnded_SessionStarted_Navigation_NoAttachments() {
        FuseboxMetrics.notifyOmniboxSessionStarted();

        var histogramWatcher =
                HistogramWatcher.newBuilder()
                        .expectBooleanRecord(
                                "Omnibox.MobileFusebox.AttachmentsPopupButtonClickedInSession",
                                false)
                        .expectIntRecord(
                                "Omnibox.MobileFusebox.AutocompleteRequestTypeAtNavigation",
                                AutocompleteRequestType.SEARCH)
                        // No attachment button usage/shown metrics should be recorded.
                        .build();

        FuseboxMetrics.notifyOmniboxSessionEnded(true, AutocompleteRequestType.SEARCH);

        histogramWatcher.assertExpected();
    }

    @Test
    @EnableFeatures(ChromeFeatureList.CHROME_ITEM_PICKER_UI)
    public void testNotifyOmniboxSessionEnded_SessionStarted_Abandon_AttachmentsUsed() {
        FuseboxMetrics.notifyOmniboxSessionStarted();

        mPropertyModel.set(FuseboxProperties.CURRENT_TAB_BUTTON_VISIBLE, true);
        mPropertyModel.set(FuseboxProperties.POPUP_CLIPBOARD_BUTTON_VISIBLE, true);
        mPropertyModel.set(FuseboxProperties.POPUP_FILE_BUTTON_VISIBLE, true);

        var histogramWatcher =
                HistogramWatcher.newBuilder()
                        .expectBooleanRecord("Omnibox.MobileFusebox.AttachmentsPopupToggled", true)
                        .expectIntRecord(
                                "Omnibox.MobileFusebox.AttachmentButtonShown",
                                FuseboxAttachmentButtonType.CURRENT_TAB)
                        .expectIntRecord(
                                "Omnibox.MobileFusebox.AttachmentButtonShown",
                                FuseboxAttachmentButtonType.TAB_PICKER)
                        .expectIntRecord(
                                "Omnibox.MobileFusebox.AttachmentButtonShown",
                                FuseboxMetrics.FuseboxAttachmentButtonType.CAMERA)
                        .expectIntRecord(
                                "Omnibox.MobileFusebox.AttachmentButtonShown",
                                FuseboxMetrics.FuseboxAttachmentButtonType.GALLERY)
                        .expectIntRecord(
                                "Omnibox.MobileFusebox.AttachmentButtonShown",
                                FuseboxMetrics.FuseboxAttachmentButtonType.CLIPBOARD)
                        .expectIntRecord(
                                "Omnibox.MobileFusebox.AttachmentButtonShown",
                                FuseboxMetrics.FuseboxAttachmentButtonType.FILES)
                        .expectIntRecord(
                                "Omnibox.MobileFusebox.AttachmentButtonUsed",
                                FuseboxMetrics.FuseboxAttachmentButtonType.CAMERA)
                        .expectIntRecord(
                                "Omnibox.MobileFusebox.AttachmentButtonUsed",
                                FuseboxMetrics.FuseboxAttachmentButtonType.TAB_PICKER)

                        // Session End Metrics:
                        .expectBooleanRecord(
                                "Omnibox.MobileFusebox.AttachmentsPopupButtonClickedInSession",
                                true)
                        .expectBooleanRecord(
                                "Omnibox.MobileFusebox.AttachmentButtonUsedInSession.CurrentTab",
                                false)
                        .expectBooleanRecord(
                                "Omnibox.MobileFusebox.AttachmentButtonUsedInSession.TabPicker",
                                true)
                        .expectBooleanRecord(
                                "Omnibox.MobileFusebox.AttachmentButtonUsedInSession.Camera", true)
                        .expectBooleanRecord(
                                "Omnibox.MobileFusebox.AttachmentButtonUsedInSession.Gallery",
                                false)
                        .expectBooleanRecord(
                                "Omnibox.MobileFusebox.AttachmentButtonUsedInSession.Clipboard",
                                false)
                        .expectBooleanRecord(
                                "Omnibox.MobileFusebox.AttachmentButtonUsedInSession.Files", false)
                        .expectIntRecord(
                                "Omnibox.MobileFusebox.AutocompleteRequestTypeAtAbandon",
                                AutocompleteRequestType.AI_MODE)
                        .build();

        FuseboxMetrics.notifyAttachmentsPopupToggled(true, mPropertyModel);

        FuseboxMetrics.notifyAttachmentButtonUsed(
                FuseboxMetrics.FuseboxAttachmentButtonType.CAMERA);
        FuseboxMetrics.notifyAttachmentButtonUsed(
                FuseboxMetrics.FuseboxAttachmentButtonType.TAB_PICKER);

        FuseboxMetrics.notifyOmniboxSessionEnded(false, AutocompleteRequestType.AI_MODE);

        histogramWatcher.assertExpected();
    }
}
