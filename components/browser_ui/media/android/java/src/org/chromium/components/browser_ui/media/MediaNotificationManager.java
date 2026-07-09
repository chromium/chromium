// Copyright 2015 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.components.browser_ui.media;

import android.app.Service;
import android.util.Pair;
import android.util.SparseArray;
import android.util.SparseIntArray;

import androidx.annotation.VisibleForTesting;

import org.chromium.build.annotations.NullMarked;
import org.chromium.build.annotations.Nullable;

import java.lang.ref.WeakReference;
import java.util.ArrayList;
import java.util.HashMap;
import java.util.List;
import java.util.Map;
import java.util.function.BiFunction;

/**
 * A class that manages the services/notifications for various media types. Each notification is
 * associated with a different {@link MediaNotificationController}.
 */
@NullMarked
public class MediaNotificationManager {
    // Maps the notification ids to their corresponding notification managers.
    private static final SparseArray<MediaNotificationController> sControllers;

    // Maps (tabId, mediaTypeId) to a unique notification ID.
    // The mediaTypeId (e.g. R.id.media_playback_notification) represents the TYPE of media
    // notification (playback, cast, presentation) and corresponds to the specific
    // Foreground Service class that manages it.
    // When multiple notifications are enabled, we generate a uniqueId for each tab's
    // notification of the same type, so they don't overwrite each other in the UI.
    private static final Map<Pair<Integer, Integer>, Integer> sUniqueIdMap = new HashMap<>();

    // Seeded with currentTimeMillis.
    private static int sIdCounter = (int) (System.currentTimeMillis() % Integer.MAX_VALUE);

    // Maps mediaTypeId to the notification ID currently associated with its foreground service.
    private static final SparseIntArray sActiveNotificationIds = new SparseIntArray();

    // Maps mediaTypeId to its foreground service reference.
    private static final SparseArray<WeakReference<Service>> sServices = new SparseArray<>();

    public static void setService(int mediaTypeId, @Nullable Service service) {
        if (service == null) {
            sServices.remove(mediaTypeId);
        } else {
            sServices.put(mediaTypeId, new WeakReference<>(service));
        }
    }

    public static @Nullable Service getService(int mediaTypeId) {
        WeakReference<Service> ref = sServices.get(mediaTypeId);
        return ref == null ? null : ref.get();
    }

    static {
        sControllers = new SparseArray<>();
    }

    private static boolean sMultipleMediaNotificationsEnabled;

    /**
     * Sets whether multiple media notifications are enabled.
     *
     * @param enabled True if multiple media notifications should be enabled.
     */
    public static void setMultipleMediaNotificationsEnabled(boolean enabled) {
        sMultipleMediaNotificationsEnabled = enabled;
    }

    /**
     * @return True if multiple media notifications are enabled.
     */
    public static boolean isMultipleMediaNotificationsEnabled() {
        return sMultipleMediaNotificationsEnabled;
    }

    private MediaNotificationManager() {}

    /**
     * Checks if the notification with the given ID is the active foreground one for its media type.
     */
    public static boolean isNotificationActive(int notificationId) {
        int mediaTypeId = getMediaTypeId(notificationId);
        return sActiveNotificationIds.get(mediaTypeId, MediaNotificationInfo.INVALID_ID)
                == notificationId;
    }

    private static synchronized int getOrCreateUniqueId(int tabId, int mediaTypeId) {
        Pair<Integer, Integer> key = Pair.create(tabId, mediaTypeId);
        Integer uniqueId = sUniqueIdMap.get(key);
        if (uniqueId == null) {
            // Incrementing counter with bound checking
            sIdCounter = (sIdCounter == Integer.MAX_VALUE) ? 1 : sIdCounter + 1;
            uniqueId = sIdCounter;
            sUniqueIdMap.put(key, uniqueId);
        }
        return uniqueId;
    }

    @VisibleForTesting
    public static int getUniqueId(int tabId, int mediaTypeId) {
        Pair<Integer, Integer> key = Pair.create(tabId, mediaTypeId);
        Integer uniqueId = sUniqueIdMap.get(key);
        return uniqueId != null ? uniqueId : mediaTypeId;
    }

    private static int getFallbackPlayingControllerId(int mediaTypeId, int excludeId) {
        for (int i = 0; i < sControllers.size(); i++) {
            int id = sControllers.keyAt(i);
            if (id == excludeId || getMediaTypeId(id) != mediaTypeId) continue;
            MediaNotificationController c = sControllers.valueAt(i);
            if (c != null && !c.isPaused()) {
                return id;
            }
        }
        return MediaNotificationInfo.INVALID_ID;
    }

    /**
     * Shows the notification with media controls with the specified media info. Replaces/updates
     * the current notification if already showing. Does nothing if |mediaNotificationInfo| hasn't
     * changed from the last one. If |mediaNotificationInfo.isPaused| is true and the tabId
     * mismatches |mMediaNotificationInfo.isPaused|, it is also no-op.
     *
     * @param notificationInfo information to show in the notification
     * @param delegateFactory a factory function for the delegate passed to new {@link
     *     MediaNotificationController} instances.
     */
    public static void show(
            MediaNotificationInfo notificationInfo,
            BiFunction<Integer, Integer, MediaNotificationController.Delegate> delegateFactory) {
        MediaNotificationInfo infoToShow = notificationInfo;
        int targetId = notificationInfo.id;

        if (isMultipleMediaNotificationsEnabled()) {
            targetId = getOrCreateUniqueId(notificationInfo.instanceId, notificationInfo.id);
            infoToShow =
                    new MediaNotificationInfo.Builder(notificationInfo).setId(targetId).build();
        }

        // Create the final delegate with the resolved targetId and mediaTypeId.
        MediaNotificationController.Delegate delegate =
                delegateFactory.apply(targetId, notificationInfo.id);

        MediaNotificationController controller = sControllers.get(targetId);
        boolean wasPaused = (controller == null) || controller.isPaused();

        if (controller == null) {
            controller = new MediaNotificationController(delegate);
            sControllers.put(targetId, controller);
        }

        int mediaTypeId = notificationInfo.id;
        int activeNotificationId =
                sActiveNotificationIds.get(mediaTypeId, MediaNotificationInfo.INVALID_ID);

        // A playing notification is shown. Determine if it should take over as the active FGS.
        if (!notificationInfo.isPaused) {
            boolean shouldPromote = false;
            if (activeNotificationId == MediaNotificationInfo.INVALID_ID) {
                shouldPromote = true;
            } else {
                MediaNotificationController activeController =
                        sControllers.get(activeNotificationId);
                // Promote to FGS if the current FGS is paused, or this controller was previously
                // paused and is now starting to play.
                boolean activeFgsIsPaused = activeController == null || activeController.isPaused();
                boolean thisControllerWasPaused = wasPaused;
                shouldPromote = activeFgsIsPaused || thisControllerWasPaused;
            }
            if (shouldPromote) {
                int previousActiveId = activeNotificationId;
                sActiveNotificationIds.put(mediaTypeId, targetId);
                // Demote the previous active FGS if it is different.
                if (previousActiveId != MediaNotificationInfo.INVALID_ID
                        && previousActiveId != targetId) {
                    MediaNotificationController previousController =
                            sControllers.get(previousActiveId);
                    if (previousController != null) {
                        previousController.demote(/* stopFgs= */ false);
                    }
                }
            }
        } else if (targetId == activeNotificationId) {
            // Active controller is pausing. Look for another playing controller.
            int newActiveId = getFallbackPlayingControllerId(mediaTypeId, targetId);
            if (newActiveId != MediaNotificationInfo.INVALID_ID) {
                sActiveNotificationIds.put(mediaTypeId, newActiveId);
                MediaNotificationController activeController = sControllers.get(newActiveId);
                if (activeController != null) {
                    // Fallback to the other playing notification and promote it to FGS.
                    activeController.promote();
                }
            } else {
                sActiveNotificationIds.delete(mediaTypeId);
            }
        }

        controller.queueNotification(infoToShow);
    }

    /**
     * Hides the notification for the specified tabId and mediaTypeId.
     *
     * @param tabId the id of the tab that showed the notification or invalid tab id.
     * @param mediaTypeId the media type ID of the notification.
     */
    public static void hide(int tabId, int mediaTypeId) {
        int targetId = getUniqueId(tabId, mediaTypeId);
        MediaNotificationController controller = getController(targetId);
        if (controller == null) return;

        // In single-notification mode, multiple tabs share the same controller. Only hide
        // the notification if it is currently showing for the tab that is requesting to hide it.
        if (controller.mMediaNotificationInfo != null
                && controller.mMediaNotificationInfo.instanceId != tabId) {
            return;
        }

        controller.hideNotification(tabId);

        sControllers.remove(targetId);
        Pair<Integer, Integer> mapKey = Pair.create(tabId, mediaTypeId);
        sUniqueIdMap.remove(mapKey);

        // If the active FGS notification is being hidden, look for another playing notification
        // of the same media type to promote to FGS.
        int activeNotificationId =
                sActiveNotificationIds.get(mediaTypeId, MediaNotificationInfo.INVALID_ID);
        if (targetId == activeNotificationId) {
            sActiveNotificationIds.delete(mediaTypeId);
            int newActiveId = getFallbackPlayingControllerId(mediaTypeId, targetId);
            if (newActiveId != MediaNotificationInfo.INVALID_ID) {
                sActiveNotificationIds.put(mediaTypeId, newActiveId);
                MediaNotificationController activeController = sControllers.get(newActiveId);
                if (activeController != null) {
                    // Fallback to the other playing notification and promote it to FGS.
                    activeController.promote();
                }
            }
        }
    }

    /**
     * Hides notifications with the specified media type ID for all tabs if shown.
     *
     * @param mediaTypeId the media type ID of the notification.
     */
    public static void hideForAllTabs(int mediaTypeId) {
        // Clear all unique (tab-specific) controllers of this media type.
        List<Pair<Integer, Integer>> keysToRemove = new ArrayList<>();
        for (Map.Entry<Pair<Integer, Integer>, Integer> entry : sUniqueIdMap.entrySet()) {
            if (entry.getKey().second == mediaTypeId) {
                int uniqueId = entry.getValue();
                MediaNotificationController controller = sControllers.get(uniqueId);
                if (controller != null) {
                    controller.clearNotification();
                    sControllers.remove(uniqueId);
                }
                keysToRemove.add(entry.getKey());
            }
        }
        for (Pair<Integer, Integer> key : keysToRemove) {
            sUniqueIdMap.remove(key);
        }

        // Clear the base (shared) controller of this media type if it exists.
        MediaNotificationController baseController = sControllers.get(mediaTypeId);
        if (baseController != null) {
            baseController.clearNotification();
            sControllers.remove(mediaTypeId);
        }

        // If the active controller was removed during hideForAllTabs(), reset the active ID.
        int activeId = sActiveNotificationIds.get(mediaTypeId, MediaNotificationInfo.INVALID_ID);
        if (activeId != MediaNotificationInfo.INVALID_ID && sControllers.get(activeId) == null) {
            sActiveNotificationIds.delete(mediaTypeId);
        }
    }

    /**
     * Notifies all controllers associated with the specified notification ID that the service is
     * being destroyed.
     *
     * @param mediaTypeId the media type ID of the notification.
     */
    public static void onServiceDestroyed(int mediaTypeId) {
        sServices.remove(mediaTypeId);
        for (Map.Entry<Pair<Integer, Integer>, Integer> entry : sUniqueIdMap.entrySet()) {
            if (entry.getKey().second == mediaTypeId) {
                int uniqueId = entry.getValue();
                MediaNotificationController controller = sControllers.get(uniqueId);
                if (controller != null) {
                    controller.onServiceDestroyed();
                }
            }
        }

        MediaNotificationController baseController = sControllers.get(mediaTypeId);
        if (baseController != null) {
            baseController.onServiceDestroyed();
        }
    }

    /**
     * Activates the Android MediaSession. This method is used to activate Android MediaSession more
     * often because some old version of Android might send events to the latest active session
     * based on when setActive(true) was called and regardless of the current playback state.
     *
     * @param tabId the id of the tab requesting to reactivate the Android MediaSession.
     * @param mediaTypeId the media type ID of the notification.
     */
    public static void activateAndroidMediaSession(int tabId, int mediaTypeId) {
        int targetId = getUniqueId(tabId, mediaTypeId);
        MediaNotificationController controller = getController(targetId);
        if (controller == null) return;
        controller.activateAndroidMediaSession(tabId);
    }

    public static int getMediaTypeId(int uniqueId) {
        MediaNotificationController controller = sControllers.get(uniqueId);
        return controller != null ? controller.getMediaTypeId() : uniqueId;
    }

    /**
     * Checks if the foreground service is still needed by any active (non-paused) controller of the
     * specified media type, excluding a specific controller.
     *
     * @param mediaTypeId The media type ID of the service to check.
     * @param excludeUniqueId The unique ID of the controller to exclude from the check (usually the
     *     one that is being paused or destroyed).
     * @return True if there is at least one other non-paused controller of the same media type,
     *     false otherwise.
     */
    public static boolean isServiceNeeded(int mediaTypeId, int excludeUniqueId) {
        return getFallbackPlayingControllerId(mediaTypeId, excludeUniqueId)
                != MediaNotificationInfo.INVALID_ID;
    }

    public static boolean hasPlayingController(int mediaTypeId) {
        return getFallbackPlayingControllerId(mediaTypeId, MediaNotificationInfo.INVALID_ID)
                != MediaNotificationInfo.INVALID_ID;
    }

    public static void resetForTesting() {
        sControllers.clear();
        sUniqueIdMap.clear();
        sActiveNotificationIds.clear();
        sServices.clear();
    }

    public static @Nullable MediaNotificationController getController(int notificationId) {
        return sControllers.get(notificationId);
    }

    public static void setControllerForTesting(
            int notificationId, MediaNotificationController controller) {
        sControllers.put(notificationId, controller);
    }
}
