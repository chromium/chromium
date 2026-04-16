// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.omnibox.fusebox;

import static com.google.common.truth.Truth.assertThat;

import org.junit.Before;
import org.junit.Rule;
import org.junit.Test;
import org.junit.runner.RunWith;
import org.mockito.Mock;
import org.mockito.junit.MockitoJUnit;
import org.mockito.junit.MockitoRule;

import org.chromium.base.test.BaseRobolectricTestRunner;
import org.chromium.base.test.util.Features.DisableFeatures;
import org.chromium.base.test.util.Features.EnableFeatures;
import org.chromium.base.test.util.HistogramWatcher;
import org.chromium.chrome.browser.flags.ChromeFeatureList;
import org.chromium.chrome.browser.omnibox.fusebox.FuseboxMetrics.AiModeActivationSource;
import org.chromium.chrome.browser.omnibox.fusebox.FuseboxMetrics.FuseboxAttachmentButtonType;
import org.chromium.chrome.browser.omnibox.fusebox.FuseboxProperties.PopupButtonData;
import org.chromium.chrome.browser.omnibox.fusebox.FuseboxProperties.PopupButtonType;
import org.chromium.components.feature_engagement.Tracker;
import org.chromium.components.omnibox.AimModelsProto.ModelMode;
import org.chromium.components.omnibox.AutocompleteRequestType;
import org.chromium.components.omnibox.ToolModeProto.ToolMode;
import org.chromium.ui.modelutil.PropertyModel;

import java.util.Arrays;

@RunWith(BaseRobolectricTestRunner.class)
public class FuseboxMetricsTest {
    public @Rule MockitoRule mMockitoRule = MockitoJUnit.rule();

    private final PropertyModel mPropertyModel = new PropertyModel(FuseboxProperties.ALL_KEYS);
    private @Mock Tracker mTracker;
    private FuseboxMetrics mMetrics;

    @Before
    public void setUp() {
        mMetrics = new FuseboxMetrics();
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

        mMetrics.notifyAttachmentButtonUsed(FuseboxMetrics.FuseboxAttachmentButtonType.CLIPBOARD);

        histogramWatcher.assertExpected();
    }

    @Test
    public void testNotifyModelButtonSelected() {
        HistogramWatcher histogramWatcher =
                HistogramWatcher.newSingleRecordWatcher(
                        "Omnibox.MobileFusebox.ModelButtonSelected",
                        ModelMode.MODEL_MODE_GEMINI_PRO_VALUE);

        FuseboxMetrics.notifyModelButtonSelected(ModelMode.MODEL_MODE_GEMINI_PRO_VALUE);

        histogramWatcher.assertExpected();
    }

    @Test
    public void testNotifyToolButtonSelected() {
        var histogramWatcher =
                HistogramWatcher.newSingleRecordWatcher(
                        "Omnibox.MobileFusebox.ToolButtonSelected",
                        ToolMode.TOOL_MODE_IMAGE_GEN_VALUE);

        FuseboxMetrics.notifyToolButtonSelected(ToolMode.TOOL_MODE_IMAGE_GEN_VALUE);

        histogramWatcher.assertExpected();
    }

    @Test
    public void testToolModeHistogramBound() {
        // When this test fails, it means the proto added a new tool mode, and
        // TOOL_MODE_HISTOGRAM_BOUND needs to be updated.
        for (ToolMode mode : ToolMode.values()) {
            if (mode == ToolMode.UNRECOGNIZED) continue;
            assertThat(mode.getNumber()).isLessThan(FuseboxMetrics.TOOL_MODE_HISTOGRAM_BOUND);
        }
    }

    @Test
    public void testModelModeHistogramBound() {
        // When this test fails, it means the proto added a new model mode, and
        // MODEL_MODE_HISTOGRAM_BOUND needs to be updated.
        for (ModelMode mode : ModelMode.values()) {
            if (mode == ModelMode.UNRECOGNIZED) continue;
            assertThat(mode.getNumber()).isLessThan(FuseboxMetrics.MODEL_MODE_HISTOGRAM_BOUND);
        }
    }

    @Test
    public void testNotifyAttachmentsPopupToggled_ShowPopup_WithModelButtons() {
        PopupButtonData data1 =
                new PopupButtonData(
                        (data) -> {},
                        "Pro",
                        /* iconId= */ 0,
                        /* enabled= */ true,
                        /* selected= */ false,
                        PopupButtonType.MODEL,
                        ModelMode.MODEL_MODE_GEMINI_PRO_VALUE);
        PopupButtonData data2 =
                new PopupButtonData(
                        (data) -> {},
                        "Flash",
                        /* iconId= */ 0,
                        /* enabled= */ true,
                        /* selected= */ false,
                        PopupButtonType.MODEL,
                        ModelMode.MODEL_MODE_GEMINI_PRO_AUTOROUTE_VALUE);
        mPropertyModel.set(
                FuseboxProperties.POPUP_MODEL_BUTTON_DATA_LIST, Arrays.asList(data1, data2));

        HistogramWatcher histogramWatcher =
                HistogramWatcher.newBuilder()
                        .expectBooleanRecord("Omnibox.MobileFusebox.AttachmentsPopupToggled", true)
                        .expectIntRecord(
                                "Omnibox.MobileFusebox.ModelButtonShown",
                                ModelMode.MODEL_MODE_GEMINI_PRO_VALUE)
                        .expectIntRecord(
                                "Omnibox.MobileFusebox.ModelButtonShown",
                                ModelMode.MODEL_MODE_GEMINI_PRO_AUTOROUTE_VALUE)
                        .build();

        mMetrics.notifyAttachmentsPopupToggled(true, mPropertyModel, mTracker);

        histogramWatcher.assertExpected();
    }

    @Test
    @EnableFeatures(ChromeFeatureList.CHROME_ITEM_PICKER_UI)
    public void testNotifyAttachmentsPopupToggled_ShowPopup_AllButtonsVisible() {
        mPropertyModel.set(FuseboxProperties.POPUP_ATTACH_CAMERA_VISIBLE, true);
        mPropertyModel.set(FuseboxProperties.POPUP_ATTACH_GALLERY_VISIBLE, true);
        mPropertyModel.set(FuseboxProperties.POPUP_ATTACH_CURRENT_TAB_VISIBLE, true);
        mPropertyModel.set(FuseboxProperties.POPUP_ATTACH_TAB_PICKER_VISIBLE, true);
        mPropertyModel.set(FuseboxProperties.POPUP_ATTACH_CLIPBOARD_VISIBLE, true);
        mPropertyModel.set(FuseboxProperties.POPUP_ATTACH_FILE_VISIBLE, true);

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

        mMetrics.notifyAttachmentsPopupToggled(true, mPropertyModel, mTracker);

        histogramWatcher.assertExpected();
    }

    @Test
    @DisableFeatures(ChromeFeatureList.CHROME_ITEM_PICKER_UI)
    public void testNotifyAttachmentsPopupToggled_ShowPopup_SomeButtonsHidden() {
        mPropertyModel.set(FuseboxProperties.POPUP_ATTACH_CAMERA_VISIBLE, true);
        mPropertyModel.set(FuseboxProperties.POPUP_ATTACH_GALLERY_VISIBLE, true);
        mPropertyModel.set(FuseboxProperties.POPUP_ATTACH_CURRENT_TAB_VISIBLE, false);
        mPropertyModel.set(FuseboxProperties.POPUP_ATTACH_TAB_PICKER_VISIBLE, false);
        mPropertyModel.set(FuseboxProperties.POPUP_ATTACH_CLIPBOARD_VISIBLE, false);
        mPropertyModel.set(FuseboxProperties.POPUP_ATTACH_FILE_VISIBLE, false);

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

        mMetrics.notifyAttachmentsPopupToggled(true, mPropertyModel, mTracker);

        histogramWatcher.assertExpected();
    }

    @Test
    public void testNotifyAttachmentsPopupToggled_ShowPopup_ToolButtonsVisible() {
        mPropertyModel.set(FuseboxProperties.POPUP_TOOL_AI_MODE_VISIBLE, true);
        mPropertyModel.set(FuseboxProperties.POPUP_TOOL_CREATE_IMAGE_VISIBLE, true);
        mPropertyModel.set(FuseboxProperties.POPUP_TOOL_DEEP_SEARCH_VISIBLE, true);
        mPropertyModel.set(FuseboxProperties.POPUP_TOOL_CANVAS_VISIBLE, false);

        var histogramWatcher =
                HistogramWatcher.newBuilder()
                        .expectBooleanRecord("Omnibox.MobileFusebox.AttachmentsPopupToggled", true)
                        .expectIntRecord(
                                "Omnibox.MobileFusebox.ToolButtonShown",
                                ToolMode.TOOL_MODE_UNSPECIFIED_VALUE)
                        .expectIntRecord(
                                "Omnibox.MobileFusebox.ToolButtonShown",
                                ToolMode.TOOL_MODE_IMAGE_GEN_VALUE)
                        .expectIntRecord(
                                "Omnibox.MobileFusebox.ToolButtonShown",
                                ToolMode.TOOL_MODE_DEEP_SEARCH_VALUE)
                        .build();

        mMetrics.notifyAttachmentsPopupToggled(true, mPropertyModel, mTracker);

        histogramWatcher.assertExpected();
    }

    @Test
    public void testNotifyAttachmentsPopupToggled_HidePopup() {
        var histogramWatcher =
                HistogramWatcher.newSingleRecordWatcher(
                        "Omnibox.MobileFusebox.AttachmentsPopupToggled", false);

        // When hiding the popup, no other metrics should be recorded.
        mMetrics.notifyAttachmentsPopupToggled(false, mPropertyModel, mTracker);

        histogramWatcher.assertExpected();
    }

    @Test
    public void testNotifyOmniboxSessionEnded_SessionStarted_Navigation_NoAttachments() {
        mMetrics.notifyOmniboxSessionStarted();

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

        mMetrics.notifyOmniboxSessionEnded(
                true, AutocompleteRequestType.SEARCH, ModelMode.MODEL_MODE_GEMINI_REGULAR_VALUE);

        histogramWatcher.assertExpected();
    }

    @Test
    public void testNotifyOmniboxSessionEnded_SessionStarted_Navigation_AimRequest() {
        mMetrics.notifyOmniboxSessionStarted();

        var histogramWatcher =
                HistogramWatcher.newBuilder()
                        .expectBooleanRecord(
                                "Omnibox.MobileFusebox.AttachmentsPopupButtonClickedInSession",
                                false)
                        .expectIntRecord(
                                "Omnibox.MobileFusebox.AutocompleteRequestTypeAtNavigation",
                                AutocompleteRequestType.AI_MODE)
                        .expectIntRecord(
                                "Omnibox.MobileFusebox.ModelAtNavigation",
                                ModelMode.MODEL_MODE_GEMINI_REGULAR_VALUE)
                        .build();

        mMetrics.notifyOmniboxSessionEnded(
                true, AutocompleteRequestType.AI_MODE, ModelMode.MODEL_MODE_GEMINI_REGULAR_VALUE);

        histogramWatcher.assertExpected();
    }

    @Test
    @EnableFeatures(ChromeFeatureList.CHROME_ITEM_PICKER_UI)
    public void testNotifyOmniboxSessionEnded_SessionStarted_Abandon_AttachmentsUsed() {
        mMetrics.notifyOmniboxSessionStarted();

        mPropertyModel.set(FuseboxProperties.POPUP_ATTACH_CAMERA_VISIBLE, true);
        mPropertyModel.set(FuseboxProperties.POPUP_ATTACH_GALLERY_VISIBLE, true);
        mPropertyModel.set(FuseboxProperties.POPUP_ATTACH_CURRENT_TAB_VISIBLE, true);
        mPropertyModel.set(FuseboxProperties.POPUP_ATTACH_TAB_PICKER_VISIBLE, true);
        mPropertyModel.set(FuseboxProperties.POPUP_ATTACH_CLIPBOARD_VISIBLE, true);
        mPropertyModel.set(FuseboxProperties.POPUP_ATTACH_FILE_VISIBLE, true);

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
                        .expectIntRecord(
                                "Omnibox.MobileFusebox.ModelAtAbandon",
                                ModelMode.MODEL_MODE_GEMINI_PRO_VALUE)
                        .build();

        mMetrics.notifyAttachmentsPopupToggled(true, mPropertyModel, mTracker);

        mMetrics.notifyAttachmentButtonUsed(FuseboxMetrics.FuseboxAttachmentButtonType.CAMERA);
        mMetrics.notifyAttachmentButtonUsed(FuseboxMetrics.FuseboxAttachmentButtonType.TAB_PICKER);

        mMetrics.notifyOmniboxSessionEnded(
                false, AutocompleteRequestType.AI_MODE, ModelMode.MODEL_MODE_GEMINI_PRO_VALUE);

        histogramWatcher.assertExpected();
    }
}
