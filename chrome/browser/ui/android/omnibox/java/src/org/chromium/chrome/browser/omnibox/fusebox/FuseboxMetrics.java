// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.omnibox.fusebox;

import android.annotation.SuppressLint;
import android.os.SystemClock;

import androidx.annotation.IntDef;
import androidx.annotation.VisibleForTesting;

import org.chromium.base.metrics.RecordHistogram;
import org.chromium.base.metrics.UmaRecorderHolder;
import org.chromium.build.annotations.NullMarked;
import org.chromium.chrome.browser.omnibox.fusebox.FuseboxProperties.PopupButtonData;
import org.chromium.chrome.browser.omnibox.fusebox.FuseboxProperties.PopupButtonType;
import org.chromium.components.browser_ui.util.ConversionUtils;
import org.chromium.components.contextual_search.ContextUploadErrorType;
import org.chromium.components.contextual_search.ContextUploadStatus;
import org.chromium.components.feature_engagement.EventConstants;
import org.chromium.components.feature_engagement.Tracker;
import org.chromium.components.omnibox.AutocompleteRequestType;
import org.chromium.components.omnibox.ToolModeUtils;
import org.chromium.ui.base.MimeTypeUtils;
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

    @VisibleForTesting
    /* package */ static final String FILE_ATTACHMENT_SIZE_HISTOGRAM =
            "Omnibox.MobileFusebox.FileAttachmentSize";

    private static final String TOKEN_SEPARATOR = ".";

    @VisibleForTesting /* package */
    static final String FILE_ATTACHMENT_SIZE_LIMIT_CHECK_HISTOGRAM =
            "Omnibox.MobileFusebox.AttachmentSizeLimitCheck";

    @VisibleForTesting /* package */ static final int TOOL_MODE_HISTOGRAM_BOUND = 11;
    @VisibleForTesting /* package */ static final int MODEL_MODE_HISTOGRAM_BOUND = 5;

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
        FuseboxAttachmentButtonType.SUGGESTED_TAB,
        FuseboxAttachmentButtonType.RECENT_TAB,
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
        int SUGGESTED_TAB = 6;
        int RECENT_TAB = 7;
        int COUNT = 8;
    }

    // LINT.ThenChange(//tools/metrics/histograms/metadata/omnibox/enums.xml:FuseboxAttachmentButtonType)

    // LINT.IfChange(FuseboxAttachmentSizeLimitCheck)
    @IntDef({
        FuseboxAttachmentSizeLimitCheck.UNDER_LIMIT_ON_METERED,
        FuseboxAttachmentSizeLimitCheck.UNDER_LIMIT_ON_UNMETERED,
        FuseboxAttachmentSizeLimitCheck.OVER_LIMIT_ON_METERED,
        FuseboxAttachmentSizeLimitCheck.OVER_LIMIT_ON_UNMETERED,
        FuseboxAttachmentSizeLimitCheck.COUNT
    })
    @Retention(RetentionPolicy.SOURCE)
    public @interface FuseboxAttachmentSizeLimitCheck {
        int UNDER_LIMIT_ON_METERED = 0;
        int UNDER_LIMIT_ON_UNMETERED = 1;
        int OVER_LIMIT_ON_METERED = 2;
        int OVER_LIMIT_ON_UNMETERED = 3;
        int COUNT = 4;
    }

    // LINT.ThenChange(//tools/metrics/histograms/metadata/omnibox/enums.xml:FuseboxAttachmentSizeLimitCheck)

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

    static void notifyAttachmentSizeLimitCheck(@FuseboxAttachmentSizeLimitCheck int result) {
        RecordHistogram.recordEnumeratedHistogram(
                FILE_ATTACHMENT_SIZE_LIMIT_CHECK_HISTOGRAM,
                result,
                FuseboxAttachmentSizeLimitCheck.COUNT);
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
            List<PopupButtonData> toolButtons =
                    model.get(FuseboxProperties.POPUP_TOOL_BUTTON_DATA_LIST);
            if (toolButtons != null) {
                for (PopupButtonData buttonData : toolButtons) {
                    assert buttonData.type == PopupButtonType.TOOL;
                    notifyToolButtonShown(buttonData.protoId);
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

    void notifyAttachmentButtonShown(@FuseboxAttachmentButtonType int attachmentType) {
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

    private static void notifyToolButtonShown(int toolMode) {
        RecordHistogram.recordEnumeratedHistogram(
                "Omnibox.MobileFusebox.ToolButtonShown", toolMode, TOOL_MODE_HISTOGRAM_BOUND);
    }

    static void notifyToolButtonSelected(int toolMode) {
        RecordHistogram.recordEnumeratedHistogram(
                "Omnibox.MobileFusebox.ToolButtonSelected", toolMode, TOOL_MODE_HISTOGRAM_BOUND);
    }

    static void notifyModelButtonSelected(int modelId) {
        RecordHistogram.recordEnumeratedHistogram(
                "Omnibox.MobileFusebox.ModelButtonSelected", modelId, MODEL_MODE_HISTOGRAM_BOUND);
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

    static void notifyFileAttachmentSize(long sizeInBytes, @MimeTypeUtils.Type int fileType) {
        int sizeInKiB = (int) ConversionUtils.bytesToKilobytes(sizeInBytes);
        recordAttachmentSizeHistogram(FILE_ATTACHMENT_SIZE_HISTOGRAM, sizeInKiB);
        recordAttachmentSizeHistogram(getFileAttachmentSizeHistogram(fileType), sizeInKiB);
    }

    private static void recordAttachmentSizeHistogram(String histogramName, int sizeInKiB) {
        UmaRecorderHolder.get()
                .recordExponentialHistogram(histogramName, sizeInKiB, 100, 100000, 100);
    }

    static void recordContextUploadStatus(@ContextUploadStatus int status) {
        RecordHistogram.recordEnumeratedHistogram(
                "Omnibox.MobileFusebox.ContextUploadStatus",
                status,
                ContextUploadStatus.MAX_VALUE + 1);
    }

    static void recordContextUploadError(@ContextUploadErrorType int errorType) {
        RecordHistogram.recordEnumeratedHistogram(
                "Omnibox.MobileFusebox.ContextUploadError",
                errorType,
                ContextUploadErrorType.MAX_VALUE + 1);
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
            case FuseboxAttachmentButtonType.SUGGESTED_TAB -> "SuggestedTab";
            case FuseboxAttachmentButtonType.RECENT_TAB -> "RecentTab";
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

            case FuseboxAttachmentButtonType.RECENT_TAB ->
                    model.get(FuseboxProperties.POPUP_RECENT_TABS_HEADER_VISIBLE);
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

    // LINT.IfChange(getHistogramExtensionForMimeType)

    private static String getHistogramExtensionForMimeType(@MimeTypeUtils.Type int fileType) {
        return switch (fileType) {
            case MimeTypeUtils.Type.TEXT -> "Text";
            case MimeTypeUtils.Type.IMAGE -> "Image";
            case MimeTypeUtils.Type.AUDIO -> "Audio";
            case MimeTypeUtils.Type.VIDEO -> "Video";
            case MimeTypeUtils.Type.PDF -> "Pdf";
            default -> "Unknown";
        };
    }

    // LINT.ThenChange(//tools/metrics/histograms/metadata/omnibox/histograms.xml:FuseboxAttachmentFileType)

    @VisibleForTesting
    /* package */ static String getFileAttachmentSizeHistogram(@MimeTypeUtils.Type int fileType) {
        return FILE_ATTACHMENT_SIZE_HISTOGRAM
                + TOKEN_SEPARATOR
                + getHistogramExtensionForMimeType(fileType);
    }
}
