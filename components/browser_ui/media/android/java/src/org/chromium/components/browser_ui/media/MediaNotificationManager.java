// Copyright 2015 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.components.browser_ui.media;

import android.util.SparseArray;

import org.chromium.base.supplier.Supplier;

/**
 * A class that manages the services/notifications for various media types.
 * Each notification is associated with a different {@link MediaNotificationController}.
 */
public class MediaNotificationManager {
    // Maps the notification ids to their corresponding notification managers.
    private static SparseArray<MediaNotificationController> sControllers;

    static {
        sControllers = new SparseArray<MediaNotificationController>();
    }

    private MediaNotificationManager() {}

    /**
     * Shows the notification with media controls with the specified media info. Replaces/updates
     * the current notification if already showing. Does nothing if |mediaNotificationInfo| hasn't
     * changed from the last one. If |mediaNotificationInfo.isPaused| is true and the tabId
     * mismatches |mMediaNotificationInfo.isPaused|, it is also no-op.
     *
     * @param notificationInfo information to show in the notification
     * @param delegate a factory function for the delegate passed to new {@link
     *         MediaNotificatonController} instances.
     */
    public static void show(
            MediaNotificationInfo notificationInfo,
            Supplier<MediaNotificationController.Delegate> delegateFactory) {
        MediaNotificationController controller = sControllers.get(notificationInfo.id);
        if (controller == null) {
            controller = new MediaNotificationController(delegateFactory.get());
            sControllers.put(notificationInfo.id, controller);
        }

        controller.queueNotification(notificationInfo);
    }

    /**
     * Hides the notification for the specified tabId and notificationId
     *
     * @param tabId the id of the tab that showed the notification or invalid tab id.
     * @param notificationId the id of the notification to hide for this tab.
     */
    public static void hide(int tabId, int notificationId) {
        MediaNotificationController controller = getController(notificationId);
        if (controller == null) return;

        controller.hideNotification(tabId);
    }

    /**
     * Hides notifications with the specified id for all tabs if shown.
     *
     * @param notificationId the id of the notification to hide for all tabs.
     */
    public static void clear(int notificationId) {
        MediaNotificationController controller = getController(notificationId);
        if (controller == null) return;

        controller.clearNotification();
        sControllers.remove(notificationId);
    }

    /**
     * Activates the Android MediaSession. This method is used to activate Android MediaSession more
     * often because some old version of Android might send events to the latest active session
     * based on when setActive(true) was called and regardless of the current playback state.
     * @param tabId the id of the tab requesting to reactivate the Android MediaSession.
     * @param notificationId the id of the notification to reactivate Android MediaSession for.
     */
    public static void activateAndroidMediaSession(int tabId, int notificationId) {
        MediaNotificationController controller = getController(notificationId);
        if (controller == null) return;
        controller.activateAndroidMediaSession(tabId);
    }

    public static MediaNotificationController getController(int notificationId) {
        return sControllers.get(notificationId);
    }

    public static void setControllerForTesting(
            int notificationId, MediaNotificationController controller) {
        sControllers.put(notificationId, controller);
    }
}
