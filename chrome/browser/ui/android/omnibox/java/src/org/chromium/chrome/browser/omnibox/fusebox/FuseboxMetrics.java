// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.omnibox.fusebox;

import android.annotation.SuppressLint;
import android.os.SystemClock;

import androidx.annotation.IntDef;

import org.chromium.base.metrics.RecordHistogram;
import org.chromium.build.annotations.NullMarked;
import org.chromium.components.feature_engagement.EventConstants;
import org.chromium.components.feature_engagement.Tracker;
import org.chromium.components.omnibox.AutocompleteRequestType;
import org.chromium.ui.modelutil.PropertyModel;

import java.lang.annotation.ElementType;
import java.lang.annotation.Retention;
import java.lang.annotation.RetentionPolicy;
import java.lang.annotation.Target;
import java.util.Arrays;

public class FuseboxMetrics {
    private static final String ABANDONED_HISTOGRAM = "Omnibox.MobileFusebox.AttachmentAbandoned";
    private static final String FAILED_HISTOGRAM = "Omnibox.MobileFusebox.AttachmentFailed";
    private static final String SUCCEEDED_HISTOGRAM = "Omnibox.MobileFusebox.AttachmentSucceeded";
    private static final String TOKEN_SEPARATOR = ".";

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

    static void notifyAttachmentsPopupToggled(
            boolean toShowPopup, PropertyModel model, Tracker tracker) {
        RecordHistogram.recordBooleanHistogram(
                "Omnibox.MobileFusebox.AttachmentsPopupToggled", toShowPopup);
        if (toShowPopup) {
            for (int buttonType = 0; buttonType < FuseboxAttachmentButtonType.COUNT; buttonType++) {
                if (isAttachmentButtonShown(model, buttonType)) {
                    notifyAttachmentButtonShown(buttonType);
                }
            }
            tracker.notifyEvent(EventConstants.FUSEBOX_ATTACHMENT_POPUP_USED);
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

    static void notifyAttachmentAbandoned(long startTime, @FuseboxAttachmentButtonType int type) {
        notifyAttachmentTime(startTime, type, ABANDONED_HISTOGRAM);
    }

    static void notifyAttachmentFailed(long startTime, @FuseboxAttachmentButtonType int type) {
        notifyAttachmentTime(startTime, type, FAILED_HISTOGRAM);
    }

    static void notifyAttachmentSucceeded(long startTime, @FuseboxAttachmentButtonType int type) {
        notifyAttachmentTime(startTime, type, SUCCEEDED_HISTOGRAM);
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
            case FuseboxAttachmentButtonType.CURRENT_TAB ->
                    model.get(FuseboxProperties.POPUP_ATTACH_CURRENT_TAB_VISIBLE);
            case FuseboxAttachmentButtonType.TAB_PICKER ->
                    model.get(FuseboxProperties.POPUP_ATTACH_TAB_PICKER_VISIBLE);
            case FuseboxAttachmentButtonType.CAMERA ->
                    model.get(FuseboxProperties.POPUP_ATTACH_CAMERA_VISIBLE);
            case FuseboxAttachmentButtonType.GALLERY ->
                    model.get(FuseboxProperties.POPUP_ATTACH_GALLERY_VISIBLE);
            case FuseboxAttachmentButtonType.FILES ->
                    model.get(FuseboxProperties.POPUP_ATTACH_FILE_VISIBLE);
            case FuseboxAttachmentButtonType.CLIPBOARD ->
                    model.get(FuseboxProperties.POPUP_ATTACH_CLIPBOARD_VISIBLE);
            default -> false;
        };
    }

    private static void notifyAttachmentTime(
            long startTime, @FuseboxAttachmentButtonType int type, String genericHistogram) {
        long duration = SystemClock.elapsedRealtime() - startTime;
        RecordHistogram.recordMediumTimesHistogram(genericHistogram, duration);
        String typeHistogram = typeScopedHistogram(genericHistogram, type);
        RecordHistogram.recordMediumTimesHistogram(typeHistogram, duration);
    }

    private static String typeScopedHistogram(
            String baseHistogram, @FuseboxAttachmentButtonType int type) {
        return baseHistogram + TOKEN_SEPARATOR + getStringForAttachmentType(type);
    }

    static void resetForTesting() {
        sSessionStarted = false;
        sAttachmentsPopupButtonUsedInSession = false;
        Arrays.fill(sAttachmentButtonsShownInSession, false);
        Arrays.fill(sAttachmentButtonsUsedInSession, false);
    }
}
