// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.omnibox.fusebox;

import android.annotation.SuppressLint;
import android.os.SystemClock;

import androidx.annotation.IntDef;

import org.chromium.base.metrics.RecordHistogram;
import org.chromium.build.annotations.CheckDiscard;
import org.chromium.build.annotations.NullMarked;
import org.chromium.chrome.browser.omnibox.fusebox.FuseboxProperties.PopupButtonData;
import org.chromium.chrome.browser.omnibox.fusebox.FuseboxProperties.PopupButtonType;
import org.chromium.components.feature_engagement.EventConstants;
import org.chromium.components.feature_engagement.Tracker;
import org.chromium.components.omnibox.AimModelsProto.ModelMode;
import org.chromium.components.omnibox.AutocompleteRequestType;
import org.chromium.components.omnibox.ToolModeUtils;
import org.chromium.ui.modelutil.PropertyModel;

import java.lang.annotation.ElementType;
import java.lang.annotation.Retention;
import java.lang.annotation.RetentionPolicy;
import java.lang.annotation.Target;
import java.util.Arrays;
import java.util.List;

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

    // LINT.IfChange(OmniboxModelMode)
    private static final int MODEL_MODE_HISTOGRAM_BOUND = 5;

    /* If adding new enums to the switch, make sure the above constant is 1 larger than new max. */
    @CheckDiscard("Compile time validation, never called or used.")
    private static void unusedCompileTimeCheckForModelMode(ModelMode mode) {
        switch (mode) {
            case MODEL_MODE_UNSPECIFIED:
            case MODEL_MODE_GEMINI_REGULAR:
            case MODEL_MODE_GEMINI_PRO:
            case MODEL_MODE_GEMINI_PRO_AUTOROUTE:
            case MODEL_MODE_GEMINI_PRO_NO_GEN_UI:
            case UNRECOGNIZED:
                break;
        }
    }

    // LINT.ThenChange(//tools/metrics/histograms/metadata/omnibox/enums.xml:OmniboxModelMode)

    private boolean mSessionStarted;
    private boolean mAttachmentsPopupButtonUsedInSession;
    private final boolean[] mAttachmentButtonsShownInSession =
            new boolean[FuseboxAttachmentButtonType.COUNT];
    private final boolean[] mAttachmentButtonsUsedInSession =
            new boolean[FuseboxAttachmentButtonType.COUNT];

    static void notifyAiModeActivated(@AiModeActivationSource int aiModeActivationSource) {
        RecordHistogram.recordEnumeratedHistogram(
                "Omnibox.MobileFusebox.AiModeActivationSource",
                aiModeActivationSource,
                AiModeActivationSource.COUNT);
    }

    void notifyAttachmentsPopupToggled(boolean toShowPopup, PropertyModel model, Tracker tracker) {
        RecordHistogram.recordBooleanHistogram(
                "Omnibox.MobileFusebox.AttachmentsPopupToggled", toShowPopup);
        if (toShowPopup) {
            for (int buttonType = 0; buttonType < FuseboxAttachmentButtonType.COUNT; buttonType++) {
                if (isAttachmentButtonShown(model, buttonType)) {
                    notifyAttachmentButtonShown(buttonType);
                }
            }
            List<PopupButtonData> popupButtons =
                    model.get(FuseboxProperties.POPUP_MODEL_BUTTON_DATA_LIST);
            if (popupButtons != null) {
                for (PopupButtonData buttonData : popupButtons) {
                    assert buttonData.type == PopupButtonType.MODEL;
                    RecordHistogram.recordEnumeratedHistogram(
                            "Omnibox.MobileFusebox.ModelButtonShown",
                            buttonData.protoId,
                            MODEL_MODE_HISTOGRAM_BOUND);
                }
            }
            tracker.notifyEvent(EventConstants.FUSEBOX_ATTACHMENT_POPUP_USED);
        }

        mAttachmentsPopupButtonUsedInSession = true;
    }

    private void notifyAttachmentButtonShown(@FuseboxAttachmentButtonType int attachmentType) {
        RecordHistogram.recordEnumeratedHistogram(
                "Omnibox.MobileFusebox.AttachmentButtonShown",
                attachmentType,
                FuseboxAttachmentButtonType.COUNT);
        mAttachmentButtonsShownInSession[attachmentType] = true;
    }

    void notifyAttachmentButtonUsed(@FuseboxAttachmentButtonType int attachmentType) {
        RecordHistogram.recordEnumeratedHistogram(
                "Omnibox.MobileFusebox.AttachmentButtonUsed",
                attachmentType,
                FuseboxAttachmentButtonType.COUNT);
        mAttachmentButtonsUsedInSession[attachmentType] = true;
    }

    static void notifyModelButtonUsed(int modelId) {
        RecordHistogram.recordEnumeratedHistogram(
                "Omnibox.MobileFusebox.ModelButtonUsed", modelId, MODEL_MODE_HISTOGRAM_BOUND);
    }

    void notifyOmniboxSessionStarted() {
        mSessionStarted = true;
    }

    void notifyOmniboxSessionEnded(
            boolean userDidNavigate,
            @AutocompleteRequestType int autocompleteRequestType,
            int modelId) {
        if (!mSessionStarted) return;
        RecordHistogram.recordBooleanHistogram(
                "Omnibox.MobileFusebox.AttachmentsPopupButtonClickedInSession",
                mAttachmentsPopupButtonUsedInSession);
        for (int attachmentType = 0;
                attachmentType < FuseboxAttachmentButtonType.COUNT;
                attachmentType++) {
            if (!mAttachmentButtonsShownInSession[attachmentType]) {
                continue;
            }
            RecordHistogram.recordBooleanHistogram(
                    "Omnibox.MobileFusebox.AttachmentButtonUsedInSession."
                            + getStringForAttachmentType(attachmentType),
                    mAttachmentButtonsUsedInSession[attachmentType]);
        }

        String requestTypeHistogram =
                userDidNavigate
                        ? "Omnibox.MobileFusebox.AutocompleteRequestTypeAtNavigation"
                        : "Omnibox.MobileFusebox.AutocompleteRequestTypeAtAbandon";
        String modelHistogram =
                userDidNavigate
                        ? "Omnibox.MobileFusebox.ModelAtNavigation"
                        : "Omnibox.MobileFusebox.ModelAtAbandon";
        RecordHistogram.recordEnumeratedHistogram(
                requestTypeHistogram, autocompleteRequestType, AutocompleteRequestType.COUNT);
        if (ToolModeUtils.isAimRequest(autocompleteRequestType)) {
            RecordHistogram.recordEnumeratedHistogram(
                    modelHistogram, modelId, MODEL_MODE_HISTOGRAM_BOUND);
        }

        mSessionStarted = false;
        mAttachmentsPopupButtonUsedInSession = false;
        Arrays.fill(mAttachmentButtonsShownInSession, false);
        Arrays.fill(mAttachmentButtonsUsedInSession, false);
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
}
