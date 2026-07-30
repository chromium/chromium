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

import java.lang.annotation.ElementType;
import java.lang.annotation.Retention;
import java.lang.annotation.RetentionPolicy;
import java.lang.annotation.Target;
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
    /**
     * Annotation for media type IDs (e.g. R.id.media_playback_notification) to distinguish them
     * from unique notification IDs.
     */
    @Target({ElementType.TYPE_USE})
    @Retention(RetentionPolicy.SOURCE)
    public @interface MediaTypeId {}

    // Maps notification IDs to their corresponding MediaNotificationController instances.
    // When multiple media notifications are enabled, each (tabId, mediaTypeId) pair is assigned
    // a dynamically generated unique integer notification ID. In legacy single-notification mode,
    // the notification ID is the static mediaTypeId (e.g. R.id.media_playback_notification).
    private static final SparseArray<MediaNotificationController> sControllers;

    // Maps (tabId, mediaTypeId) to a unique notification ID.
    // The mediaTypeId (e.g. R.id.media_playback_notification) represents the TYPE of media
    // notification (playback, cast, presentation) and corresponds to the specific
    // Foreground Service class that manages it.
    // When multiple notifications are enabled, we generate a unique notification ID for each tab's
    // notification of the same type, so they don't overwrite each other in the UI.
    private static final Map<Pair<Integer, Integer>, Integer> sUniqueIdMap = new HashMap<>();

    // Seeded with currentTimeMillis.
    private static int sIdCounter = (int) (System.currentTimeMillis() % Integer.MAX_VALUE);

    // Maps mediaTypeId to the unique notification ID currently associated with its foreground
    // service.
    private static final SparseIntArray sActiveNotificationIds = new SparseIntArray();

    // Maps mediaTypeId to its foreground service reference.
    private static final SparseArray<WeakReference<Service>> sServices = new SparseArray<>();

    public static void setService(@MediaTypeId int mediaTypeId, @Nullable Service service) {
        if (service == null) {
            sServices.remove(mediaTypeId);
        } else {
            sServices.put(mediaTypeId, new WeakReference<>(service));
        }
    }

    public static @Nullable Service getService(@MediaTypeId int mediaTypeId) {
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

    private static synchronized int getOrCreateUniqueId(int tabId, @MediaTypeId int mediaTypeId) {
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
    public static int getUniqueId(int tabId, @MediaTypeId int mediaTypeId) {
        Pair<Integer, Integer> key = Pair.create(tabId, mediaTypeId);
        Integer uniqueId = sUniqueIdMap.get(key);
        return uniqueId != null ? uniqueId : mediaTypeId;
    }

    private static int getFallbackPlayingControllerId(@MediaTypeId int mediaTypeId, int excludeId) {
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

    private static boolean tryFallbackPromotion(@MediaTypeId int mediaTypeId, int excludeId) {
        // Explicitly detach the old controller from FGS before searching for/promoting a new one
        // to prevent the OS from archiving the old notification.
        MediaNotificationController oldController = sControllers.get(excludeId);
        if (oldController != null) {
            oldController.demote(/* stopFgs= */ true);
        }

        int newActiveId = getFallbackPlayingControllerId(mediaTypeId, excludeId);
        if (newActiveId == MediaNotificationInfo.INVALID_ID) {
            return false;
        }

        MediaNotificationController activeController = sControllers.get(newActiveId);
        if (activeController == null) {
            return false;
        }

        boolean success = activeController.promote();
        if (success) {
            sActiveNotificationIds.put(mediaTypeId, newActiveId);
            return true;
        }
        return false;
    }

    private static void handleFallbackOnPause(
            MediaNotificationController controller,
            MediaNotificationInfo notificationInfo,
            @MediaTypeId int mediaTypeId,
            int notificationId) {
        if (tryFallbackPromotion(mediaTypeId, notificationId)) {
            return;
        }

        // Fallback promotion did not succeed (no fallback candidate or promotion failed).
        if (notificationInfo.supportsSwipeAway()) {
            // Swipeable paused media should not hold the FGS slot, so clear the active ID.
            sActiveNotificationIds.delete(mediaTypeId);
        } else {
            // Non-swipeable media (e.g. Cast) must stay in FGS even when paused to prevent
            // process termination, so restore the active ID and re-promote to FGS.
            sActiveNotificationIds.put(mediaTypeId, notificationId);
            controller.promote();
        }
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
        int notificationId = notificationInfo.id;

        if (isMultipleMediaNotificationsEnabled()) {
            notificationId = getOrCreateUniqueId(notificationInfo.instanceId, notificationInfo.id);
            infoToShow =
                    new MediaNotificationInfo.Builder(notificationInfo)
                            .setId(notificationId)
                            .build();
        }

        // Create the final delegate with the resolved notificationId and mediaTypeId.
        MediaNotificationController.Delegate delegate =
                delegateFactory.apply(notificationId, notificationInfo.id);

        MediaNotificationController controller = sControllers.get(notificationId);
        boolean wasPaused = (controller == null) || controller.isPaused();

        if (controller == null) {
            controller = new MediaNotificationController(delegate);
            sControllers.put(notificationId, controller);
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
                sActiveNotificationIds.put(mediaTypeId, notificationId);
                // Demote the previous active FGS if it is different.
                if (previousActiveId != MediaNotificationInfo.INVALID_ID
                        && previousActiveId != notificationId) {
                    MediaNotificationController previousController =
                            sControllers.get(previousActiveId);
                    if (previousController != null) {
                        previousController.demote(/* stopFgs= */ true);
                    }
                }
            }
        } else if (notificationId == activeNotificationId) {
            // Active controller is pausing. Look for another playing controller.
            handleFallbackOnPause(controller, notificationInfo, mediaTypeId, notificationId);
        }

        controller.queueNotification(infoToShow);
    }

    /**
     * Hides the notification for the specified tabId and mediaTypeId.
     *
     * @param tabId the id of the tab that showed the notification or invalid tab id.
     * @param mediaTypeId the media type ID of the notification.
     */
    public static void hide(int tabId, @MediaTypeId int mediaTypeId) {
        int notificationId = getUniqueId(tabId, mediaTypeId);
        MediaNotificationController controller = getControllerByNotificationId(notificationId);
        if (controller == null) return;

        // In single-notification mode, multiple tabs share the same controller. Only hide
        // the notification if it is currently showing for the tab that is requesting to hide it.
        if (controller.mMediaNotificationInfo != null
                && controller.mMediaNotificationInfo.instanceId != tabId) {
            return;
        }

        controller.hideNotification(tabId);

        sControllers.remove(notificationId);
        Pair<Integer, Integer> mapKey = Pair.create(tabId, mediaTypeId);
        sUniqueIdMap.remove(mapKey);

        // Clear the active ID; tryFallbackPromotion will set sActiveNotificationIds correctly if
        // there is a fallback playing controller that already is or becomes promoted to FGS.
        sActiveNotificationIds.delete(mediaTypeId);
        tryFallbackPromotion(mediaTypeId, notificationId);
    }

    /**
     * Hides notifications with the specified media type ID for all tabs if shown.
     *
     * @param mediaTypeId the media type ID of the notification.
     */
    public static void hideForAllTabs(@MediaTypeId int mediaTypeId) {
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
    public static void onServiceDestroyed(@MediaTypeId int mediaTypeId) {
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
    public static void activateAndroidMediaSession(int tabId, @MediaTypeId int mediaTypeId) {
        int notificationId = getUniqueId(tabId, mediaTypeId);
        MediaNotificationController controller = getControllerByNotificationId(notificationId);
        if (controller == null) return;
        controller.activateAndroidMediaSession(tabId);
    }

    public static @MediaTypeId int getMediaTypeId(int notificationId) {
        MediaNotificationController controller = sControllers.get(notificationId);
        return controller != null ? controller.getMediaTypeId() : notificationId;
    }

    /**
     * Checks if the foreground service is still needed by any active (non-paused) controller of the
     * specified media type, excluding a specific controller.
     *
     * @param mediaTypeId The media type ID of the service to check.
     * @param excludeNotificationId The unique notification ID of the controller to exclude from the
     *     check (usually the one that is being paused or destroyed).
     * @return True if there is at least one other non-paused controller of the same media type,
     *     false otherwise.
     */
    public static boolean isServiceNeeded(@MediaTypeId int mediaTypeId, int excludeNotificationId) {
        return getFallbackPlayingControllerId(mediaTypeId, excludeNotificationId)
                != MediaNotificationInfo.INVALID_ID;
    }

    public static boolean hasPlayingController(@MediaTypeId int mediaTypeId) {
        return getFallbackPlayingControllerId(mediaTypeId, MediaNotificationInfo.INVALID_ID)
                != MediaNotificationInfo.INVALID_ID;
    }

    public static void resetForTesting() {
        sControllers.clear();
        sUniqueIdMap.clear();
        sActiveNotificationIds.clear();
        sServices.clear();
    }

    public static @Nullable MediaNotificationController getControllerByNotificationId(
            int notificationId) {
        return sControllers.get(notificationId);
    }

    /**
     * Gets the active {@link MediaNotificationController} for the specified media type ID.
     *
     * <p>This method is used when handling external system Intents (e.g., Bluetooth/headset media
     * keys dispatched directly to the Service) where no {@code EXTRA_NOTIFICATION_ID} or {@code
     * tabId} is available.
     *
     * <p>1. If a controller of this media type is currently promoted to the foreground service, its
     * unique notification ID is retrieved from {@link #sActiveNotificationIds} and its controller
     * is returned.
     *
     * <p>2. If multiple media notifications are disabled (single-notification mode), the controller
     * is keyed directly by {@code mediaTypeId}.
     *
     * <p>3. If multiple media notifications are enabled and no controller is currently promoted, it
     * falls back to returning any remaining controller matching this {@code mediaTypeId}.
     *
     * @param mediaTypeId The media type ID of the notification.
     * @return The active {@link MediaNotificationController}, or null if none exist.
     */
    public static @Nullable MediaNotificationController getActiveOrFallbackControllerByMediaTypeId(
            @MediaTypeId int mediaTypeId) {
        int activeNotificationId =
                sActiveNotificationIds.get(mediaTypeId, MediaNotificationInfo.INVALID_ID);
        if (activeNotificationId != MediaNotificationInfo.INVALID_ID) {
            MediaNotificationController controller = sControllers.get(activeNotificationId);
            if (controller != null) return controller;
        }
        if (!isMultipleMediaNotificationsEnabled()) {
            return sControllers.get(mediaTypeId);
        }
        for (int i = 0; i < sControllers.size(); i++) {
            MediaNotificationController c = sControllers.valueAt(i);
            if (c != null && c.getMediaTypeId() == mediaTypeId) {
                return c;
            }
        }
        return null;
    }

    public static void setControllerForTesting(
            int notificationId, MediaNotificationController controller) {
        sControllers.put(notificationId, controller);
    }
}
