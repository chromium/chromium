// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.components.messages;


import androidx.annotation.IntDef;

import org.chromium.base.Log;
import org.chromium.base.TimeUtils;
import org.chromium.base.metrics.RecordHistogram;

/**
 * Static utility methods for recording messages related metrics. TODO(crbug.com/40877562): remove
 * logs.
 */
public class MessagesMetrics {
    private static final String TAG = "MessagesMetrics";
    private static final String ENQUEUED_HISTOGRAM_NAME = "Android.Messages.Enqueued";
    private static final String ENQUEUED_SUSPEND_HISTOGRAM_NAME =
            "Android.Messages.Enqueued.Suspended";
    private static final String ENQUEUED_RESUME_HISTOGRAM_NAME =
            "Android.Messages.Enqueued.Resumed";
    private static final String ENQUEUED_SCOPE_ACTIVE_HISTOGRAM_NAME =
            "Android.Messages.Enqueued.ScopeActive";
    private static final String ENQUEUED_SCOPE_INACTIVE_HISTOGRAM_NAME =
            "Android.Messages.Enqueued.ScopeInactive";
    private static final String ENQUEUED_VISIBLE_HISTOGRAM_NAME =
            "Android.Messages.Enqueued.Visible";
    private static final String ENQUEUED_HIDDEN_HISTOGRAM_NAME = "Android.Messages.Enqueued.Hidden";
    private static final String ENQUEUED_HIDING_HISTOGRAM_NAME = "Android.Messages.Enqueued.Hiding";
    private static final String FULLY_VISIBLE_NAME = "Android.Messages.FullyVisible";
    private static final String DISMISSED_WITHOUT_FULLY_VISIBLE =
            "Android.Messages.DismissedWithoutFullyVisible";
    private static final String DISMISSED_HISTOGRAM_PREFIX = "Android.Messages.Dismissed.";
    private static final String TIME_TO_ACTION_HISTOGRAM_PREFIX = "Android.Messages.TimeToAction.";
    private static final String TIME_TO_ACTION_DISMISS_HISTOGRAM_PREFIX =
            "Android.Messages.TimeToAction.Dismiss.";
    static final String STACKING_HISTOGRAM_NAME = "Android.Messages.Stacking";
    static final String STACKING_HIDDEN_NAME = "Android.Messages.Stacking.Hidden";
    static final String STACKING_HIDING_NAME = "Android.Messages.Stacking.Hiding";
    static final String STACKING_REQUEST_TO_SHOW_NAME =
            "Android.Messages.Stacking.RequestToFullyShow";
    static final String STACKING_BLOCKED_BY_BROWSER_CONTROL_NAME =
            "Android.Messages.Stacking.BlockedByBrowserControl";
    static final String STACKING_BLOCKED_BY_CONTAINER_INITING_NAME =
            "Android.Messages.Stacking.BlockedByContainerInitializing";
    static final String STACKING_BLOCKED_BY_CONTAINER_NOT_INITED_NAME =
            "Android.Messages.Stacking.BlockedByContainerNotInitialized";
    static final String STACKING_TIME_TO_FULLY_SHOW_PREFIX = "Android.Messages.TimeToFullyShow.";
    static final String STACKING_ACTION_HISTOGRAM_PREFIX = "Android.Messages.Stacking.";
    static final String THREE_STACKED_HISTOGRAM_NAME = "Android.Messages.Stacking.ThreeStacked";

    @IntDef({
        StackingAnimationType.SHOW_ALL,
        StackingAnimationType.SHOW_FRONT_ONLY,
        StackingAnimationType.REMOVE_FRONT_AND_SHOW_BACK,
        StackingAnimationType.REMOVE_ALL,
        StackingAnimationType.REMOVE_FRONT_ONLY,
        StackingAnimationType.REMOVE_BACK_ONLY,
        StackingAnimationType.SHOW_BACK_ONLY,
        StackingAnimationType.INSERT_AT_FRONT,
        StackingAnimationType.MAX_VALUE
    })
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

    @IntDef({
        StackingAnimationAction.INSERT_AT_FRONT,
        StackingAnimationAction.INSERT_AT_BACK,
        StackingAnimationAction.PUSH_TO_FRONT,
        StackingAnimationAction.PUSH_TO_BACK,
        StackingAnimationAction.REMOVE_FRONT,
        StackingAnimationAction.REMOVE_BACK,
        StackingAnimationAction.MAX_VALUE
    })
    public @interface StackingAnimationAction {
        int INSERT_AT_FRONT = 0;
        int INSERT_AT_BACK = 1;
        int PUSH_TO_FRONT = 2;
        int PUSH_TO_BACK = 3;
        int REMOVE_FRONT = 4;
        int REMOVE_BACK = 5;
        int MAX_VALUE = 6;
    }

    @IntDef({
        ThreeStackedScenario.HIGH_PRIORITY,
        ThreeStackedScenario.IN_SEQUENCE,
        ThreeStackedScenario.MAX_VALUE
    })
    public @interface ThreeStackedScenario {
        int HIGH_PRIORITY = 0;
        int IN_SEQUENCE = 1;
        int MAX_VALUE = 2;
    }

    /** Records metrics when a message is being enqueued. */
    static void recordMessageEnqueued(@MessageIdentifier int messageIdentifier) {
        RecordHistogram.recordEnumeratedHistogram(
                ENQUEUED_HISTOGRAM_NAME, messageIdentifier, MessageIdentifier.COUNT);
    }

    static void recordMessageEnqueuedScopeActive(
            @MessageIdentifier int messageIdentifier, boolean active) {
        String histogram =
                active
                        ? ENQUEUED_SCOPE_ACTIVE_HISTOGRAM_NAME
                        : ENQUEUED_SCOPE_INACTIVE_HISTOGRAM_NAME;
        RecordHistogram.recordEnumeratedHistogram(
                histogram, messageIdentifier, MessageIdentifier.COUNT);
    }

    static void recordMessageEnqueuedQueueSuspended(
            @MessageIdentifier int messageIdentifier, boolean suspended) {
        String histogram =
                suspended ? ENQUEUED_SUSPEND_HISTOGRAM_NAME : ENQUEUED_RESUME_HISTOGRAM_NAME;
        RecordHistogram.recordEnumeratedHistogram(
                histogram, messageIdentifier, MessageIdentifier.COUNT);
    }

    /** Records metrics when a message is visible after being enqueued. */
    static void recordMessageEnqueuedVisible(@MessageIdentifier int messageIdentifier) {
        RecordHistogram.recordEnumeratedHistogram(
                ENQUEUED_VISIBLE_HISTOGRAM_NAME, messageIdentifier, MessageIdentifier.COUNT);
    }

    /** Records metrics when a message is hidden after being enqueued.*/
    static void recordMessageEnqueuedHidden(
            @MessageIdentifier int enqueuedMessage,
            @MessageIdentifier int currentDisplayedMessage) {
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
    static void recordTimeToAction(
            @MessageIdentifier int messageIdentifier,
            boolean messageDismissedByGesture,
            long durationMs) {
        String histogramSuffix = messageIdentifierToHistogramSuffix(messageIdentifier);
        RecordHistogram.recordMediumTimesHistogram(
                TIME_TO_ACTION_HISTOGRAM_PREFIX + histogramSuffix, durationMs);
        if (messageDismissedByGesture) {
            RecordHistogram.recordMediumTimesHistogram(
                    TIME_TO_ACTION_DISMISS_HISTOGRAM_PREFIX + histogramSuffix, durationMs);
        }
    }

    /**
     * Record the id of candidate which will be displayed in the foreground.
     *
     * @param messageIdentifier The id of the next front message.
     */
    static void recordRequestToFullyShow(@MessageIdentifier int messageIdentifier) {
        RecordHistogram.recordEnumeratedHistogram(
                STACKING_REQUEST_TO_SHOW_NAME, messageIdentifier, MessageIdentifier.COUNT);
    }

    /**
     * Record the id of candidate which will be displayed in the foreground but now is waiting for
     * browser control to be ready.
     *
     * @param messageIdentifier The id of the next front message.
     */
    static void recordBlockedByBrowserControl(@MessageIdentifier int messageIdentifier) {
        RecordHistogram.recordEnumeratedHistogram(
                STACKING_BLOCKED_BY_BROWSER_CONTROL_NAME,
                messageIdentifier,
                MessageIdentifier.COUNT);
    }

    /**
     * Record the id of candidate which will be displayed in the foreground but now is waiting for
     * message container to finishing initialization.
     *
     * @param messageIdentifier The id of the next front message.
     */
    static void recordBlockedByContainerInitializing(@MessageIdentifier int messageIdentifier) {
        RecordHistogram.recordEnumeratedHistogram(
                STACKING_BLOCKED_BY_CONTAINER_INITING_NAME,
                messageIdentifier,
                MessageIdentifier.COUNT);
    }

    /**
     * Record the id of candidate which will be displayed in the foreground but container has not
     * been initialized.
     *
     * @param messageIdentifier The id of the next front message.
     */
    static void recordBlockedByContainerNotInitialized(@MessageIdentifier int messageIdentifier) {
        RecordHistogram.recordEnumeratedHistogram(
                STACKING_BLOCKED_BY_CONTAINER_NOT_INITED_NAME,
                messageIdentifier,
                MessageIdentifier.COUNT);
    }

    static void recordTimeToFullyShow(@MessageIdentifier int messageIdentifier, long durationMs) {
        String histogramSuffix = messageIdentifierToHistogramSuffix(messageIdentifier);
        RecordHistogram.recordMediumTimesHistogram(
                STACKING_TIME_TO_FULLY_SHOW_PREFIX + histogramSuffix, durationMs);
    }

    /**
     * Record the id of background message when it is stacked.
     * @param messageIdentifier The id of the background message.
     */
    static void recordStackingHidden(@MessageIdentifier int messageIdentifier) {
        RecordHistogram.recordEnumeratedHistogram(
                STACKING_HIDDEN_NAME, messageIdentifier, MessageIdentifier.COUNT);
    }

    /**
     * Record the id of the front message when there is a background message.
     * @param messageIdentifier The id of the foreground message.
     */
    static void recordStackingHiding(@MessageIdentifier int messageIdentifier) {
        RecordHistogram.recordEnumeratedHistogram(
                STACKING_HIDING_NAME, messageIdentifier, MessageIdentifier.COUNT);
    }

    static void recordStackingAnimationType(@StackingAnimationType int type) {
        Log.i(TAG, "Triggered message stacking animation type %s.", type);
        RecordHistogram.recordEnumeratedHistogram(
                STACKING_HISTOGRAM_NAME, type, StackingAnimationType.MAX_VALUE);
    }

    static void recordStackingAnimationAction(
            @StackingAnimationAction int action, @MessageIdentifier int messageIdentifier) {
        String suffix = stackingAnimationActionToHistogramSuffix(action);
        RecordHistogram.recordEnumeratedHistogram(
                STACKING_ACTION_HISTOGRAM_PREFIX + suffix,
                messageIdentifier,
                MessageIdentifier.COUNT);
    }

    static void recordThreeStackedScenario(@ThreeStackedScenario int scenario) {
        RecordHistogram.recordEnumeratedHistogram(
                THREE_STACKED_HISTOGRAM_NAME, scenario, ThreeStackedScenario.MAX_VALUE);
    }

    /** Record the message has been fully visible. */
    static void recordFullyVisible(@MessageIdentifier int messageIdentifier) {
        RecordHistogram.recordEnumeratedHistogram(
                FULLY_VISIBLE_NAME, messageIdentifier, MessageIdentifier.COUNT);
    }

    /** Record when the message is dismissed without being fully visible before. */
    static void recordDismissedWithoutFullyVisible(@MessageIdentifier int messageIdentifier) {
        RecordHistogram.recordEnumeratedHistogram(
                DISMISSED_WITHOUT_FULLY_VISIBLE, messageIdentifier, MessageIdentifier.COUNT);
    }

    /**
     * Returns current timestamp in milliseconds to be used when recording message's visible
     * duration.
     */
    static long now() {
        return TimeUtils.uptimeMillis();
    }

    private static String stackingAnimationActionToHistogramSuffix(
            @StackingAnimationAction int action) {
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
            case MessageIdentifier.CVC_SAVE:
                return "CvcSave";
            case MessageIdentifier.DESKTOP_SITE_WINDOW_SETTING:
                return "DesktopSiteWindowSetting";
            case MessageIdentifier.PROMPT_HATS_LOCATION_CUSTOM_INVITATION:
                return "PromptHatsLocationCustomInvitation";
            case MessageIdentifier.PROMPT_HATS_LOCATION_GENERIC_INVITATION:
                return "PromptHatsLocationGenericInvitation";
            case MessageIdentifier.PROMPT_HATS_CAMERA_CUSTOM_INVITATION:
                return "PromptHatsCameraCustomInvitation";
            case MessageIdentifier.PROMPT_HATS_CAMERA_GENERIC_INVITATION:
                return "PromptHatsCameraGenericInvitation";
            case MessageIdentifier.PROMPT_HATS_MICROPHONE_CUSTOM_INVITATION:
                return "PromptHatsMicrophoneCustomInvitation";
            case MessageIdentifier.PROMPT_HATS_MICROPHONE_GENERIC_INVITATION:
                return "PromptHatsMicrophoneGenericInvitation";
            case MessageIdentifier.PERMISSION_BLOCKED:
                return "PermissionBlocked";
            case MessageIdentifier.SAVE_CARD_FAILURE:
                return "SaveCardFailure";
            case MessageIdentifier.VIRTUAL_CARD_ENROLL_FAILURE:
                return "VirtualCardEnrollFailure";
            case MessageIdentifier.PROMPT_HATS_QUICK_DELETE:
                return "PromptHatsQuickDelete";
            case MessageIdentifier.PROMPT_HATS_SAFETY_HUB:
                return "PromptHatsSafetyHub";
            case MessageIdentifier.DEFAULT_BROWSER_PROMO:
                return "DefaultBrowserPromo";
            case MessageIdentifier.TAB_REMOVED_THROUGH_COLLABORATION:
                return "TabRemovedThroughCollaboration";
            case MessageIdentifier.TAB_NAVIGATED_THROUGH_COLLABORATION:
                return "TabNavigatedThroughCollaboration";
            case MessageIdentifier.COLLABORATION_USER_JOINED:
                return "CollaborationUserJoined";
            case MessageIdentifier.COLLABORATION_REMOVED:
                return "CollaborationRemoved";
            default:
                return "Unknown";
        }
    }

    static String getEnqueuedHistogramNameForTesting() {
        return ENQUEUED_HISTOGRAM_NAME;
    }

    static String getDismissHistogramNameForTesting(@MessageIdentifier int messageIdentifier) {
        return DISMISSED_HISTOGRAM_PREFIX + messageIdentifierToHistogramSuffix(messageIdentifier);
    }
}
