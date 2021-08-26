// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.components.messages;

import android.os.SystemClock;

import androidx.annotation.VisibleForTesting;

import org.chromium.base.metrics.RecordHistogram;

/**
 * Static utility methods for recording messages related metrics.
 */
public class MessagesMetrics {
    private static final String ENQUEUED_HISTOGRAM_NAME = "Android.Messages.Enqueued";
    private static final String DISMISSED_HISTOGRAM_PREFIX = "Android.Messages.Dismissed.";
    private static final String TIME_TO_ACTION_HISTOGRAM_PREFIX = "Android.Messages.TimeToAction.";
    private static final String TIME_TO_ACTION_DISMISS_HISTOGRAM_PREFIX =
            "Android.Messages.TimeToAction.Dismiss.";

    /** Records metrics when a message is enqueued. */
    public static void recordMessageEnqueued(@MessageIdentifier int messageIdentifier) {
        RecordHistogram.recordEnumeratedHistogram(
                ENQUEUED_HISTOGRAM_NAME, messageIdentifier, MessageIdentifier.COUNT);
    }

    /** Records metrics when a message is dismissed. */
    public static void recordDismissReason(
            @MessageIdentifier int messageIdentifier, @DismissReason int dismissReason) {
        String histogramName =
                DISMISSED_HISTOGRAM_PREFIX + messageIdentifierToHistogramSuffix(messageIdentifier);
        RecordHistogram.recordEnumeratedHistogram(
                histogramName, dismissReason, DismissReason.COUNT);
    }

    /**
     * Records metrics with duration of time a message was visible before it was dismissed by a user
     * action.
     */
    public static void recordTimeToAction(@MessageIdentifier int messageIdentifier,
            boolean messageDismissedByGesture, long durationMs) {
        String histogramSuffix = messageIdentifierToHistogramSuffix(messageIdentifier);
        RecordHistogram.recordMediumTimesHistogram(
                TIME_TO_ACTION_HISTOGRAM_PREFIX + histogramSuffix, durationMs);
        if (messageDismissedByGesture) {
            RecordHistogram.recordMediumTimesHistogram(
                    TIME_TO_ACTION_DISMISS_HISTOGRAM_PREFIX + histogramSuffix, durationMs);
        }
    }

    /**
     * Returns current timestamp in milliseconds to be used when recording message's visible
     * duration.
     */
    public static long now() {
        return SystemClock.uptimeMillis();
    }

    /**
     * Returns a histogram suffix string that corresponds to message identifier of the current
     * message.
     * Update this function when adding a new message identifier.
     */
    static String messageIdentifierToHistogramSuffix(@MessageIdentifier int messageIdentifier) {
        switch (messageIdentifier) {
            case MessageIdentifier.TEST_MESSAGE:
                return "TestMessage";
            case MessageIdentifier.SAVE_PASSWORD:
                return "SavePassword";
            case MessageIdentifier.UPDATE_PASSWORD:
                return "UpdatePassword";
            case MessageIdentifier.GENERATED_PASSWORD_SAVED:
                return "GeneratedPasswordSaved";
            case MessageIdentifier.POPUP_BLOCKED:
                return "PopupBlocked";
            case MessageIdentifier.SAFETY_TIP:
                return "SafetyTip";
            case MessageIdentifier.SAVE_ADDRESS_PROFILE:
                return "SaveAddressProfile";
            case MessageIdentifier.MERCHANT_TRUST:
                return "MerchantTrust";
            case MessageIdentifier.ADD_TO_HOMESCREEN_IPH:
                return "AddToHomescreenIPH";
            case MessageIdentifier.SEND_TAB_TO_SELF:
                return "SendTabToSelf";
            case MessageIdentifier.READER_MODE:
                return "ReaderMode";
            case MessageIdentifier.SAVE_CARD:
                return "SaveCard";
            case MessageIdentifier.CHROME_SURVEY:
                return "ChromeSurvey";
            case MessageIdentifier.NOTIFICATION_BLOCKED:
                return "NotificationBlocked";
            case MessageIdentifier.PERMISSION_UPDATE:
                return "PermissionUpdate";
            case MessageIdentifier.ADS_BLOCKED:
                return "AdsBlocked";
            case MessageIdentifier.DOWNLOAD_PROGRESS:
                return "DownloadProgress";
            default:
                return "Unknown";
        }
    }

    @VisibleForTesting
    public static int getEnqueuedMessageCountForTesting(@MessageIdentifier int messageIdentifier) {
        return RecordHistogram.getHistogramValueCountForTesting(
                ENQUEUED_HISTOGRAM_NAME, messageIdentifier);
    }

    @VisibleForTesting
    public static int getDismissReasonForTesting(
            @MessageIdentifier int messageIdentifier, @DismissReason int dismissReason) {
        String histogramName =
                DISMISSED_HISTOGRAM_PREFIX + messageIdentifierToHistogramSuffix(messageIdentifier);
        return RecordHistogram.getHistogramValueCountForTesting(histogramName, dismissReason);
    }
}
