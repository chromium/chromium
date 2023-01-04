// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.components.messages;

import android.os.SystemClock;

import androidx.annotation.IntDef;
import androidx.annotation.VisibleForTesting;

import org.chromium.base.Log;
import org.chromium.base.metrics.RecordHistogram;

/**
 * Static utility methods for recording messages related metrics.
 * TODO(https://crbug.com/1382967): remove logs.
 */
public class MessagesMetrics {
    private static final String TAG = "MessagesMetrics";
    private static final String ENQUEUED_HISTOGRAM_NAME = "Android.Messages.Enqueued";
    private static final String ENQUEUED_VISIBLE_HISTOGRAM_NAME =
            "Android.Messages.Enqueued.Visible";
    private static final String ENQUEUED_HIDDEN_HISTOGRAM_NAME = "Android.Messages.Enqueued.Hidden";
    private static final String ENQUEUED_HIDING_HISTOGRAM_NAME = "Android.Messages.Enqueued.Hiding";
    private static final String DISMISSED_HISTOGRAM_PREFIX = "Android.Messages.Dismissed.";
    private static final String TIME_TO_ACTION_HISTOGRAM_PREFIX = "Android.Messages.TimeToAction.";
    private static final String TIME_TO_ACTION_DISMISS_HISTOGRAM_PREFIX =
            "Android.Messages.TimeToAction.Dismiss.";
    static final String STACKING_HISTOGRAM_NAME = "Android.Messages.Stacking";
    static final String STACKING_ACTION_HISTOGRAM_PREFIX = "Android.Messages.Stacking.";
    static final String THREE_STACKED_HISTOGRAM_NAME = "Android.Messages.Stacking.ThreeStacked";

    @IntDef({StackingAnimationType.SHOW_ALL, StackingAnimationType.SHOW_FRONT_ONLY,
            StackingAnimationType.REMOVE_FRONT_AND_SHOW_BACK, StackingAnimationType.REMOVE_ALL,
            StackingAnimationType.REMOVE_FRONT_ONLY, StackingAnimationType.REMOVE_BACK_ONLY,
            StackingAnimationType.SHOW_BACK_ONLY, StackingAnimationType.INSERT_AT_FRONT,
            StackingAnimationType.MAX_VALUE})
    public @interface StackingAnimationType {
        int SHOW_ALL = 0;
        int SHOW_FRONT_ONLY = 1;
        int REMOVE_FRONT_AND_SHOW_BACK = 2;
        int REMOVE_ALL = 3;
        int REMOVE_FRONT_ONLY = 4;
        int REMOVE_BACK_ONLY = 5;
        int SHOW_BACK_ONLY = 6;
        int INSERT_AT_FRONT = 7;
        int MAX_VALUE = 8;
    }

    @IntDef({StackingAnimationAction.INSERT_AT_FRONT, StackingAnimationAction.INSERT_AT_BACK,
            StackingAnimationAction.PUSH_TO_FRONT, StackingAnimationAction.PUSH_TO_BACK,
            StackingAnimationAction.REMOVE_FRONT, StackingAnimationAction.REMOVE_BACK,
            StackingAnimationAction.MAX_VALUE})
    public @interface StackingAnimationAction {
        int INSERT_AT_FRONT = 0;
        int INSERT_AT_BACK = 1;
        int PUSH_TO_FRONT = 2;
        int PUSH_TO_BACK = 3;
        int REMOVE_FRONT = 4;
        int REMOVE_BACK = 5;
        int MAX_VALUE = 6;
    }

    @IntDef({ThreeStackedScenario.HIGH_PRIORITY, ThreeStackedScenario.IN_SEQUENCE,
            ThreeStackedScenario.MAX_VALUE})
    public @interface ThreeStackedScenario {
        int HIGH_PRIORITY = 0;
        int IN_SEQUENCE = 1;
        int MAX_VALUE = 2;
    }

    /** Records metrics when a message is enqueued. */
    static void recordMessageEnqueuedVisible(@MessageIdentifier int messageIdentifier) {
        RecordHistogram.recordEnumeratedHistogram(
                ENQUEUED_HISTOGRAM_NAME, messageIdentifier, MessageIdentifier.COUNT);
        RecordHistogram.recordEnumeratedHistogram(
                ENQUEUED_VISIBLE_HISTOGRAM_NAME, messageIdentifier, MessageIdentifier.COUNT);
    }

    /** Records metrics when a message is enqueued. */
    static void recordMessageEnqueuedHidden(@MessageIdentifier int enqueuedMessage,
            @MessageIdentifier int currentDisplayedMessage) {
        RecordHistogram.recordEnumeratedHistogram(
                ENQUEUED_HISTOGRAM_NAME, enqueuedMessage, MessageIdentifier.COUNT);
        RecordHistogram.recordEnumeratedHistogram(
                ENQUEUED_HIDDEN_HISTOGRAM_NAME, enqueuedMessage, MessageIdentifier.COUNT);
        RecordHistogram.recordEnumeratedHistogram(
                ENQUEUED_HIDING_HISTOGRAM_NAME, currentDisplayedMessage, MessageIdentifier.COUNT);
    }

    /** Records metrics when a message is dismissed. */
    static void recordDismissReason(
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
    static void recordTimeToAction(@MessageIdentifier int messageIdentifier,
            boolean messageDismissedByGesture, long durationMs) {
        String histogramSuffix = messageIdentifierToHistogramSuffix(messageIdentifier);
        RecordHistogram.recordMediumTimesHistogram(
                TIME_TO_ACTION_HISTOGRAM_PREFIX + histogramSuffix, durationMs);
        if (messageDismissedByGesture) {
            RecordHistogram.recordMediumTimesHistogram(
                    TIME_TO_ACTION_DISMISS_HISTOGRAM_PREFIX + histogramSuffix, durationMs);
        }
    }

    static void recordStackingAnimationType(@StackingAnimationType int type) {
        Log.i(TAG, "Triggered message stacking animation type %s.", type);
        RecordHistogram.recordEnumeratedHistogram(
                STACKING_HISTOGRAM_NAME, type, StackingAnimationType.MAX_VALUE);
    }

    static void recordStackingAnimationAction(
            @StackingAnimationAction int action, @MessageIdentifier int messageIdentifier) {
        String suffix = stackingAnimationActionToHistogramSuffix(action);
        RecordHistogram.recordEnumeratedHistogram(STACKING_ACTION_HISTOGRAM_PREFIX + suffix,
                messageIdentifier, StackingAnimationType.MAX_VALUE);
    }

    static void recordThreeStackedScenario(@ThreeStackedScenario int scenario) {
        RecordHistogram.recordEnumeratedHistogram(
                THREE_STACKED_HISTOGRAM_NAME, scenario, ThreeStackedScenario.MAX_VALUE);
    }

    /**
     * Returns current timestamp in milliseconds to be used when recording message's visible
     * duration.
     */
    static long now() {
        return SystemClock.uptimeMillis();
    }

    static String stackingAnimationActionToHistogramSuffix(@StackingAnimationAction int action) {
        if (action == StackingAnimationAction.INSERT_AT_FRONT) {
            return "InsertAtFront";
        } else if (action == StackingAnimationAction.INSERT_AT_BACK) {
            return "InsertAtBack";
        } else if (action == StackingAnimationAction.PUSH_TO_FRONT) {
            return "PushToFront";
        } else if (action == StackingAnimationAction.PUSH_TO_BACK) {
            return "PushToBack";
        } else if (action == StackingAnimationAction.REMOVE_FRONT) {
            return "RemoveFront";
        } else {
            return "RemoveBack";
        }
    }

    /**
     * Returns a histogram suffix string that corresponds to message identifier of the current
     * message.
     * Update this function when adding a new message identifier.
     */
    public static String messageIdentifierToHistogramSuffix(
            @MessageIdentifier int messageIdentifier) {
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
            case MessageIdentifier.SYNC_ERROR:
                return "SyncError";
            case MessageIdentifier.SHARED_HIGHLIGHTING:
                return "SharedHighlighting";
            case MessageIdentifier.NEAR_OOM_REDUCTION:
                return "NearOomReduction";
            case MessageIdentifier.INSTALLABLE_AMBIENT_BADGE:
                return "InstallableAmbientBadge";
            case MessageIdentifier.AUTO_DARK_WEB_CONTENTS:
                return "AutoDarkWebContents";
            case MessageIdentifier.TAILORED_SECURITY_ENABLED:
                return "TailoredSecurityEnabled";
            case MessageIdentifier.TAILORED_SECURITY_DISABLED:
                return "TailoredSecurityDisabled";
            case MessageIdentifier.VR_SERVICES_UPGRADE:
                return "VrServicesUpgrade";
            case MessageIdentifier.AR_CORE_UPGRADE:
                return "ArCoreUpgrade";
            case MessageIdentifier.ABOUT_THIS_SITE:
                return "AboutThisSite";
            case MessageIdentifier.TRANSLATE:
                return "Translate";
            case MessageIdentifier.OFFER_NOTIFICATION:
                return "OfferNotification";
            case MessageIdentifier.EXTERNAL_NAVIGATION:
                return "ExternalNavigation";
            case MessageIdentifier.FRAMEBUST_BLOCKED:
                return "FramebustBlocked";
            case MessageIdentifier.INVALID_MESSAGE:
                return "InvalidMessage";
            case MessageIdentifier.DESKTOP_SITE_GLOBAL_DEFAULT_OPT_OUT:
                return "DesktopSiteGlobalDefaultOptOut";
            case MessageIdentifier.DESKTOP_SITE_GLOBAL_OPT_IN:
                return "DesktopSiteGlobalOptIn";
            case MessageIdentifier.DOWNLOAD_INCOGNITO_WARNING:
                return "DownloadIncognitoWarning";
            case MessageIdentifier.RESTORE_CUSTOM_TAB:
                return "RestoreCustomTab";
            case MessageIdentifier.UNDO_CUSTOM_TAB_RESTORATION:
                return "UndoCustomTabRestoration";
            default:
                return "Unknown";
        }
    }

    @VisibleForTesting
    static String getEnqueuedHistogramNameForTesting() {
        return ENQUEUED_HISTOGRAM_NAME;
    }

    @VisibleForTesting
    static String getDismissHistogramNameForTesting(@MessageIdentifier int messageIdentifier) {
        return DISMISSED_HISTOGRAM_PREFIX + messageIdentifierToHistogramSuffix(messageIdentifier);
    }
}
