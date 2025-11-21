// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.omnibox.fusebox;

import android.annotation.SuppressLint;

import androidx.annotation.IntDef;

import org.chromium.base.metrics.RecordHistogram;
import org.chromium.build.annotations.NullMarked;
import org.chromium.chrome.browser.flags.ChromeFeatureList;
import org.chromium.components.omnibox.AutocompleteRequestType;
import org.chromium.ui.modelutil.PropertyModel;

import java.lang.annotation.ElementType;
import java.lang.annotation.Retention;
import java.lang.annotation.RetentionPolicy;
import java.lang.annotation.Target;
import java.util.Arrays;

public class FuseboxMetrics {

    // LINT.IfChange(AiModeActivationSource)
    @IntDef({
        AiModeActivationSource.TOOL_MENU,
        AiModeActivationSource.DEDICATED_BUTTON,
        AiModeActivationSource.NTP_BUTTON,
        AiModeActivationSource.IMPLICIT
    })
    @Retention(RetentionPolicy.SOURCE)
    @Target({ElementType.TYPE_USE})
    @NullMarked
    public @interface AiModeActivationSource {
        int TOOL_MENU = 0;
        int DEDICATED_BUTTON = 1;
        int NTP_BUTTON = 2;
        int IMPLICIT = 3;
        int COUNT = 4;
    }

    // LINT.ThenChange(//tools/metrics/histograms/metadata/omnibox/enums.xml:AiModeActivationSource)

    // LINT.IfChange(FuseboxAttachmentButtonType)
    @IntDef({
        FuseboxAttachmentButtonType.CURRENT_TAB,
        FuseboxAttachmentButtonType.TAB_PICKER,
        FuseboxAttachmentButtonType.CAMERA,
        FuseboxAttachmentButtonType.GALLERY,
        FuseboxAttachmentButtonType.FILES,
        FuseboxAttachmentButtonType.CLIPBOARD,
        FuseboxAttachmentButtonType.COUNT
    })
    @Retention(RetentionPolicy.SOURCE)
    public @interface FuseboxAttachmentButtonType {
        int CURRENT_TAB = 0;
        int TAB_PICKER = 1;
        int CAMERA = 2;
        int GALLERY = 3;
        int FILES = 4;
        int CLIPBOARD = 5;
        int COUNT = 6;
    }

    // LINT.ThenChange(//tools/metrics/histograms/metadata/omnibox/enums.xml:FuseboxAttachmentButtonType)

    private FuseboxMetrics() {}

    private static boolean sSessionStarted;
    private static boolean sAttachmentsPopupButtonUsedInSession;

    private static final boolean[] sAttachmentButtonsShownInSession =
            new boolean[FuseboxAttachmentButtonType.COUNT];

    private static final boolean[] sAttachmentButtonsUsedInSession =
            new boolean[FuseboxAttachmentButtonType.COUNT];

    static void notifyAiModeActivated(@AiModeActivationSource int aiModeActivationSource) {
        RecordHistogram.recordEnumeratedHistogram(
                "Omnibox.MobileFusebox.AiModeActivationSource",
                aiModeActivationSource,
                AiModeActivationSource.COUNT);
    }

    static void notifyAttachmentsPopupToggled(boolean toShowPopup, PropertyModel model) {
        RecordHistogram.recordBooleanHistogram(
                "Omnibox.MobileFusebox.AttachmentsPopupToggled", toShowPopup);
        if (toShowPopup) {
            for (int buttonType = 0; buttonType < FuseboxAttachmentButtonType.COUNT; buttonType++) {
                if (isAttachmentButtonShown(model, buttonType)) {
                    notifyAttachmentButtonShown(buttonType);
                }
            }
        }

        sAttachmentsPopupButtonUsedInSession = true;
    }

    private static void notifyAttachmentButtonShown(
            @FuseboxAttachmentButtonType int attachmentType) {
        RecordHistogram.recordEnumeratedHistogram(
                "Omnibox.MobileFusebox.AttachmentButtonShown",
                attachmentType,
                FuseboxAttachmentButtonType.COUNT);
        sAttachmentButtonsShownInSession[attachmentType] = true;
    }

    static void notifyAttachmentButtonUsed(@FuseboxAttachmentButtonType int attachmentType) {
        RecordHistogram.recordEnumeratedHistogram(
                "Omnibox.MobileFusebox.AttachmentButtonUsed",
                attachmentType,
                FuseboxAttachmentButtonType.COUNT);
        sAttachmentButtonsUsedInSession[attachmentType] = true;
    }

    static void notifyOmniboxSessionStarted() {
        sSessionStarted = true;
    }

    static void notifyOmniboxSessionEnded(
            boolean userDidNavigate, @AutocompleteRequestType int autocompleteRequestType) {
        if (!sSessionStarted) return;
        RecordHistogram.recordBooleanHistogram(
                "Omnibox.MobileFusebox.AttachmentsPopupButtonClickedInSession",
                sAttachmentsPopupButtonUsedInSession);
        for (int attachmentType = 0;
                attachmentType < FuseboxAttachmentButtonType.COUNT;
                attachmentType++) {
            if (!sAttachmentButtonsShownInSession[attachmentType]) {
                continue;
            }
            RecordHistogram.recordBooleanHistogram(
                    "Omnibox.MobileFusebox.AttachmentButtonUsedInSession."
                            + getStringForAttachmentType(attachmentType),
                    sAttachmentButtonsUsedInSession[attachmentType]);
        }

        if (userDidNavigate) {
            RecordHistogram.recordEnumeratedHistogram(
                    "Omnibox.MobileFusebox.AutocompleteRequestTypeAtNavigation",
                    autocompleteRequestType,
                    AutocompleteRequestType.COUNT);
        } else {
            RecordHistogram.recordEnumeratedHistogram(
                    "Omnibox.MobileFusebox.AutocompleteRequestTypeAtAbandon",
                    autocompleteRequestType,
                    AutocompleteRequestType.COUNT);
        }

        sSessionStarted = false;
        sAttachmentsPopupButtonUsedInSession = false;
        Arrays.fill(sAttachmentButtonsShownInSession, false);
        Arrays.fill(sAttachmentButtonsUsedInSession, false);
    }

    @SuppressLint("SwitchIntDef") // COUNT entry missing
    private static String getStringForAttachmentType(
            @FuseboxAttachmentButtonType int attachmentType) {
        return switch (attachmentType) {
            case FuseboxAttachmentButtonType.CURRENT_TAB -> "CurrentTab";
            case FuseboxAttachmentButtonType.TAB_PICKER -> "TabPicker";
            case FuseboxAttachmentButtonType.CAMERA -> "Camera";
            case FuseboxAttachmentButtonType.GALLERY -> "Gallery";
            case FuseboxAttachmentButtonType.FILES -> "Files";
            case FuseboxAttachmentButtonType.CLIPBOARD -> "Clipboard";
            default -> "";
        };
    }

    @SuppressLint("SwitchIntDef")
    private static boolean isAttachmentButtonShown(
            PropertyModel model, @FuseboxAttachmentButtonType int attachmentType) {
        return switch (attachmentType) {
            case FuseboxAttachmentButtonType.CAMERA, FuseboxAttachmentButtonType.GALLERY -> true;
            case FuseboxAttachmentButtonType.TAB_PICKER ->
                    ChromeFeatureList.sChromeItemPickerUi.isEnabled();
            case FuseboxAttachmentButtonType.CURRENT_TAB ->
                    model.get(FuseboxProperties.CURRENT_TAB_BUTTON_VISIBLE);
            case FuseboxAttachmentButtonType.CLIPBOARD ->
                    model.get(FuseboxProperties.POPUP_CLIPBOARD_BUTTON_VISIBLE);
            case FuseboxAttachmentButtonType.FILES ->
                    model.get(FuseboxProperties.POPUP_FILE_BUTTON_VISIBLE);
            default -> false;
        };
    }

    static void resetForTesting() {
        sSessionStarted = false;
        sAttachmentsPopupButtonUsedInSession = false;
        Arrays.fill(sAttachmentButtonsShownInSession, false);
        Arrays.fill(sAttachmentButtonsUsedInSession, false);
    }
}
