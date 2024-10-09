// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.components.browser_ui.media;

import android.app.PendingIntent;
import android.app.Service;
import android.content.Context;
import android.content.Intent;
import android.content.pm.ServiceInfo;
import android.graphics.Bitmap;
import android.media.AudioManager;
import android.os.Build;
import android.os.Handler;
import android.os.Looper;
import android.support.v4.media.MediaMetadataCompat;
import android.support.v4.media.session.MediaSessionCompat;
import android.support.v4.media.session.PlaybackStateCompat;
import android.text.TextUtils;
import android.util.SparseArray;

import androidx.annotation.NonNull;
import androidx.annotation.VisibleForTesting;
import androidx.core.app.NotificationCompat;
import androidx.core.app.NotificationManagerCompat;

import org.chromium.base.CollectionUtil;
import org.chromium.base.ContextUtils;
import org.chromium.base.IntentUtils;
import org.chromium.base.Log;
import org.chromium.components.browser_ui.notifications.BaseNotificationManagerProxy;
import org.chromium.components.browser_ui.notifications.BaseNotificationManagerProxyFactory;
import org.chromium.components.browser_ui.notifications.ForegroundServiceUtils;
import org.chromium.components.browser_ui.notifications.NotificationWrapper;
import org.chromium.components.browser_ui.notifications.NotificationWrapperBuilder;
import org.chromium.components.browser_ui.notifications.PendingIntentProvider;
import org.chromium.media_session.mojom.MediaSessionAction;
import org.chromium.services.media_session.MediaMetadata;

import java.util.ArrayList;
import java.util.HashSet;
import java.util.List;
import java.util.Set;

/**
 * A class that manages the notification, foreground service, and {@link MediaSessionCompat} for a
 * specific type of media.
 */
public class MediaNotificationController {
    private static final String TAG = "MediaNotification";

    // The maximum number of actions in CompactView media notification.
    private static final int COMPACT_VIEW_ACTIONS_COUNT = 3;

    // The maximum number of actions in BigView media notification.
    private static final int BIG_VIEW_ACTIONS_COUNT = 5;

    // Pending intent for `ACTION_SWIPE`. This intent is scheduled to be created when the UI thread
    // message queue is idle, the first time it is needed, with the goal of reducing input handling
    // delay.
    @VisibleForTesting public PendingIntentProvider mPendingIntentActionSwipe;

    // Used to help initialize `mPendingIntentActionSwipe`.
    @VisibleForTesting public PendingIntentInitializer mPendingIntentInitializer;

    public static final String ACTION_PLAY = "org.chromium.components.browser_ui.media.ACTION_PLAY";
    public static final String ACTION_PAUSE =
            "org.chromium.components.browser_ui.media.ACTION_PAUSE";
    public static final String ACTION_STOP = "org.chromium.components.browser_ui.media.ACTION_STOP";
    public static final String ACTION_SWIPE =
            "org.chromium.components.browser_ui.media.ACTION_SWIPE";
    public static final String ACTION_CANCEL =
            "org.chromium.components.browser_ui.media.ACTION_CANCEL";
    public static final String ACTION_PREVIOUS_TRACK =
            "org.chromium.components.browser_ui.media.ACTION_PREVIOUS_TRACK";
    public static final String ACTION_NEXT_TRACK =
            "org.chromium.components.browser_ui.media.ACTION_NEXT_TRACK";
    public static final String ACTION_SEEK_FORWARD =
            "org.chromium.components.browser_ui.media.ACTION_SEEK_FORWARD";
    public static final String ACTION_SEEK_BACKWARD =
            "MediaNotificationmanager.ListenerService.SEEK_BACKWARD";

    // TODO(xingliu): These must match NotificationUmaTracker's action id. Remove these when
    // notification code is modularized.
    public static final int MEDIA_ACTION_PLAY = 17;
    public static final int MEDIA_ACTION_PAUSE = 18;
    public static final int MEDIA_ACTION_STOP = 19;
    public static final int MEDIA_ACTION_PREVIOUS_TRACK = 20;
    public static final int MEDIA_ACTION_NEXT_TRACK = 21;
    public static final int MEDIA_ACTION_SEEK_FORWARD = 22;
    public static final int MEDIA_ACTION_SEEK_BACKWARD = 23;

    // ListenerService running for the notification. Only non-null when showing.
    @VisibleForTesting public Service mService;

    @VisibleForTesting public Delegate mDelegate;

    private SparseArray<MediaButtonInfo> mActionToButtonInfo;

    @VisibleForTesting public NotificationWrapperBuilder mNotificationBuilder;

    @VisibleForTesting public Bitmap mDefaultNotificationLargeIcon;

    // |mMediaNotificationInfo| should be not null if and only if the notification is showing.
    @VisibleForTesting public MediaNotificationInfo mMediaNotificationInfo;

    @VisibleForTesting public MediaSessionCompat mMediaSession;

    @VisibleForTesting public Throttler mThrottler;

    /** Helper class to prevent spamming notification updates. */
    @VisibleForTesting
    public static class Throttler {
        @VisibleForTesting public static final int THROTTLE_MILLIS = 500;

        @VisibleForTesting public MediaNotificationController mController;

        private final Handler mHandler;

        @VisibleForTesting
        public Throttler(@NonNull MediaNotificationController manager) {
            mController = manager;
            mHandler = new Handler();
        }

        // When |mThrottleTask| is non-null, it will always be queued in mHandler. When
        // |mThrottleTask| is non-null, all notification updates will be throttled and their info
        // will be stored as mLastPendingInfo. When |mThrottleTask| fires, it will call {@link
        // showNotification()} with the latest queued notification info.
        @VisibleForTesting public Runnable mThrottleTask;

        // The last pending info. If non-null, it will be the latest notification info.
        // Otherwise, the latest notification info will be |mController.mMediaNotificationInfo|.
        //
        // If `mLastPendingInfo` or `mPendingIntentActionSwipe` are null, no notification will be
        // shown. When `mThrottleTask` fires and `mLastPendingInfo` is null, the throttled state
        // will end.
        @VisibleForTesting public MediaNotificationInfo mLastPendingInfo;

        /**
         * Queue `mediaNotificationInfo` for update.
         *
         * @param mediaNotificationInfo The notification info to be queued.
         */
        public void queueNotification(MediaNotificationInfo mediaNotificationInfo) {
            assert mediaNotificationInfo != null;
            mController.schedulePendingIntentConstructionIfNeeded();
            MediaNotificationInfo latestMediaNotificationInfo =
                    mLastPendingInfo != null
                            ? mLastPendingInfo
                            : mController.mMediaNotificationInfo;

            if (shouldIgnoreMediaNotificationInfo(
                    latestMediaNotificationInfo, mediaNotificationInfo)) {
                return;
            }

            showNotificationImmediately(mediaNotificationInfo);
        }

        /**
         * Clears the pending notification and `PendingIntentInitializer` task, and enter
         * unthrottled state.
         */
        public void clearPendingNotifications() {
            mHandler.removeCallbacks(mThrottleTask);
            mLastPendingInfo = null;
            mThrottleTask = null;

            if (mController.mPendingIntentInitializer != null) {
                mController.mPendingIntentInitializer.clearDelayedTask();
            }
        }

        /**
         * Shows notification immediately if no notification has been updated in the last
         * THROTTLE_MILLIS, and queue a task for blocking further updates.
         *
         * <p>In unthrottled state (i.e. `mThrottleTask` == null), the notification will be updated
         * immediately and enter the throttled state. In throttled state, the method will only
         * update the pending notification info, which will be used for updating the notification
         * when `mThrottleTask` is fired.
         */
        @VisibleForTesting
        public void showNotificationImmediately(MediaNotificationInfo mediaNotificationInfo) {
            // Keep `mLastPendingInfo` up to date with the latest notification info.
            mLastPendingInfo = mediaNotificationInfo;

            // Return if we are in a throttled state.
            if (mThrottleTask != null) {
                return;
            }

            // Show the notification and clear `mLastPendingInfo` to prevent the next scheduled task
            // from showing a notification, if no new notifications are received.
            if (mController.mPendingIntentActionSwipe != null) {
                mController.showNotification(mediaNotificationInfo);
                mLastPendingInfo = null;
            }

            // Create a task to show a notification for the latest `mLastPendingInfo` that is queued
            // while the task is waiting to run. The task will fire after `THROTTLE_MILLIS` has
            // elapsed.
            //
            // `mThrottleTask` takes care of clearing itself and `mLastPendingInfo` controls when to
            // exit the throttled state.
            mThrottleTask =
                    new Runnable() {
                        @Override
                        public void run() {
                            mThrottleTask = null;
                            if (mLastPendingInfo != null) {
                                showNotificationImmediately(mLastPendingInfo);
                            }
                        }
                    };

            // Enter throttled state.
            if (!mHandler.postDelayed(mThrottleTask, THROTTLE_MILLIS)) {
                Log.w(TAG, "Failed to post the throttler task.");
                mThrottleTask = null;
            }
        }
    }

    /**
     * Helper class to initialize the `mPendingIntentActionSwipe` pending intent when the UI thread
     * message queue is idle.
     *
     * <p>This class will add an `IdleHandler` to the UI thread message queue and schedule a delayed
     * task. If the UI thread is not idle before `MAX_INIT_WAIT_TIME_MILLIS`, then
     * `mPendingIntentActionSwipe` will be initialized by the delayed task.
     *
     * <p>If the idle task runs before `MAX_INIT_WAIT_TIME_MILLIS`, it will cancel the delayed task.
     */
    public static class PendingIntentInitializer {
        @VisibleForTesting public static final int MAX_INIT_WAIT_TIME_MILLIS = 2000;

        @VisibleForTesting public MediaNotificationController mController;

        private final Handler mHandler;

        // Task to perform initialization if the pending intent has not been initialized after
        // `MAX_INIT_WAIT_TIME_MILLIS`.
        @VisibleForTesting public Runnable mSwipeInitTask;

        // Indicates whether the tasks to initialize the pending intent have been scheduled or not.
        private boolean mTasksScheduled;

        @VisibleForTesting
        public PendingIntentInitializer(@NonNull MediaNotificationController controller) {
            mController = controller;
            mHandler = new Handler();
        }

        /** Schedules the `mPendingIntentActionSwipe` construction if needed. */
        @VisibleForTesting
        public void schedulePendingIntentConstructionIfNeeded() {
            if (mController.mPendingIntentActionSwipe != null || mTasksScheduled) {
                return;
            }

            postDelayedTask();
            scheduleIdleTask();

            mTasksScheduled = true;
        }

        @VisibleForTesting
        public void createPendingIntentActionSwipeIfNeeded() {
            if (mController.mPendingIntentActionSwipe != null) {
                return;
            }

            clearDelayedTask();
            mController.mPendingIntentActionSwipe = mController.createPendingIntent(ACTION_SWIPE);
            mController.mPendingIntentInitializer = null;
        }

        /**
         * Schedules a delayed task to initialize `mPendingIntentActionSwipe`, if the pending intent
         * has not been initialized after `MAX_INIT_WAIT_TIME_MILLIS`.
         */
        @VisibleForTesting
        public void postDelayedTask() {
            mSwipeInitTask =
                    new Runnable() {
                        @Override
                        public void run() {
                            createPendingIntentActionSwipeIfNeeded();
                        }
                    };
            mHandler.postDelayed(mSwipeInitTask, MAX_INIT_WAIT_TIME_MILLIS);
        }

        /**
         * Adds a new `IdleHandler` to initialize `mPendingIntentActionSwipe` whenever the UI thread
         * message queue is idle.
         */
        @VisibleForTesting
        public void scheduleIdleTask() {
            Looper.myQueue()
                    .addIdleHandler(
                            () -> {
                                createPendingIntentActionSwipeIfNeeded();
                                return false;
                            });
        }

        /** Clears the `mIdleSwipeInitTask` delayed task */
        @VisibleForTesting
        public void clearDelayedTask() {
            if (mSwipeInitTask != null) {
                mHandler.removeCallbacks(mSwipeInitTask);
                mSwipeInitTask = null;
            }
        }
    }

    private final MediaSessionCompat.Callback mMediaSessionCallback =
            new MediaSessionCompat.Callback() {
                @Override
                public void onPlay() {
                    MediaNotificationController.this.onPlay(
                            MediaNotificationListener.ACTION_SOURCE_MEDIA_SESSION);
                }

                @Override
                public void onPause() {
                    MediaNotificationController.this.onPause(
                            MediaNotificationListener.ACTION_SOURCE_MEDIA_SESSION);
                }

                @Override
                public void onSkipToPrevious() {
                    MediaNotificationController.this.onMediaSessionAction(
                            MediaSessionAction.PREVIOUS_TRACK);
                }

                @Override
                public void onSkipToNext() {
                    MediaNotificationController.this.onMediaSessionAction(
                            MediaSessionAction.NEXT_TRACK);
                }

                @Override
                public void onFastForward() {
                    MediaNotificationController.this.onMediaSessionAction(
                            MediaSessionAction.SEEK_FORWARD);
                }

                @Override
                public void onRewind() {
                    MediaNotificationController.this.onMediaSessionAction(
                            MediaSessionAction.SEEK_BACKWARD);
                }

                @Override
                public void onSeekTo(long pos) {
                    MediaNotificationController.this.onMediaSessionSeekTo(pos);
                }
            };

    /**
     * Finishes starting the service on O+.
     *
     * If startForegroundService() was called, the app MUST call startForeground on the created
     * service no matter what or it will crash.
     *
     * @param service the {@link Service} on which {@link Context#startForegroundService()} has been
     *         called.
     * @param notification a minimal version of the notification associated with the service.
     * @return true if {@link Service#startForeground()} was called.
     */
    public static boolean finishStartingForegroundServiceOnO(
            Service service, NotificationWrapper notification) {
        if (Build.VERSION.SDK_INT < Build.VERSION_CODES.O) return false;
        try {
            ForegroundServiceUtils.getInstance()
                    .startForeground(
                            service,
                            notification.getMetadata().id,
                            notification.getNotification(),
                            ServiceInfo.FOREGROUND_SERVICE_TYPE_MEDIA_PLAYBACK);
        } catch (RuntimeException e) {
            Log.e(TAG, "Unable to start media foreground service", e);
        }
        return true;
    }

    private PendingIntentProvider createPendingIntent(String action) {
        Intent intent = mDelegate.createServiceIntent().setAction(action);
        return PendingIntentProvider.getService(
                getContext(),
                0,
                intent,
                PendingIntent.FLAG_CANCEL_CURRENT
                        | IntentUtils.getPendingIntentMutabilityFlag(false));
    }

    /**
     * The class containing all the information for adding a button in the notification for an
     * action.
     */
    private static final class MediaButtonInfo {
        /** The resource ID of this media button icon. */
        public int iconResId;

        /** The resource ID of this media button description. */
        public int descriptionResId;

        /** The intent string to be fired when this media button is clicked. */
        public String intentString;

        /** The ID to identify the notification button. */
        public int buttonId;

        public MediaButtonInfo(
                int buttonResId, int descriptionResId, String intentString, int buttonId) {
            this.iconResId = buttonResId;
            this.descriptionResId = descriptionResId;
            this.intentString = intentString;
            this.buttonId = buttonId;
        }
    }

    /** An interface for separating embedder-specific logic. */
    public interface Delegate {
        /** Returns an intent that will start a Service which listens to notification actions. */
        Intent createServiceIntent();

        /** Returns the name of the embedding app. */
        String getAppName();

        /** Returns the notification group name used to prevent automatic grouping. */
        String getNotificationGroupName();

        /** Returns a builder suitable as a starting point for creating the notification. */
        NotificationWrapperBuilder createNotificationWrapperBuilder();

        /** Called when the Android MediaSession has been updated. */
        void onMediaSessionUpdated(MediaSessionCompat session);

        /** Called when a notification has been shown and should be logged in UMA. */
        void logNotificationShown(NotificationWrapper notification);
    }

    public MediaNotificationController(Delegate delegate) {
        mDelegate = delegate;

        mActionToButtonInfo = new SparseArray<>();

        mActionToButtonInfo.put(
                MediaSessionAction.PLAY,
                new MediaButtonInfo(
                        R.drawable.ic_play_arrow_white_24dp,
                        R.string.accessibility_play,
                        ACTION_PLAY,
                        MEDIA_ACTION_PLAY));
        mActionToButtonInfo.put(
                MediaSessionAction.PAUSE,
                new MediaButtonInfo(
                        R.drawable.ic_pause_white_24dp,
                        R.string.accessibility_pause,
                        ACTION_PAUSE,
                        MEDIA_ACTION_PAUSE));
        mActionToButtonInfo.put(
                MediaSessionAction.STOP,
                new MediaButtonInfo(
                        R.drawable.ic_stop_white_24dp,
                        R.string.accessibility_stop,
                        ACTION_STOP,
                        MEDIA_ACTION_STOP));
        mActionToButtonInfo.put(
                MediaSessionAction.PREVIOUS_TRACK,
                new MediaButtonInfo(
                        R.drawable.ic_skip_previous_white_24dp,
                        R.string.accessibility_previous_track,
                        ACTION_PREVIOUS_TRACK,
                        MEDIA_ACTION_PREVIOUS_TRACK));
        mActionToButtonInfo.put(
                MediaSessionAction.NEXT_TRACK,
                new MediaButtonInfo(
                        R.drawable.ic_skip_next_white_24dp,
                        R.string.accessibility_next_track,
                        ACTION_NEXT_TRACK,
                        MEDIA_ACTION_NEXT_TRACK));
        mActionToButtonInfo.put(
                MediaSessionAction.SEEK_FORWARD,
                new MediaButtonInfo(
                        R.drawable.ic_fast_forward_white_24dp,
                        R.string.accessibility_seek_forward,
                        ACTION_SEEK_FORWARD,
                        MEDIA_ACTION_SEEK_FORWARD));
        mActionToButtonInfo.put(
                MediaSessionAction.SEEK_BACKWARD,
                new MediaButtonInfo(
                        R.drawable.ic_fast_rewind_white_24dp,
                        R.string.accessibility_seek_backward,
                        ACTION_SEEK_BACKWARD,
                        MEDIA_ACTION_SEEK_BACKWARD));

        mThrottler = new Throttler(this);

        // Create `mPendingIntentInitializer`, which will be used to help initialize
        // `mPendingIntentActionSwipe`.
        mPendingIntentInitializer = new PendingIntentInitializer(this);
    }

    /**
     * Registers the started {@link Service} with the manager and creates the notification.
     *
     * @param service the service that was started
     */
    public void onServiceStarted(Service service) {
        if (mService == service) return;

        mService = service;
        updateNotification(/* serviceStarting= */ true, /* shouldLogNotification= */ true);
    }

    /** Handles the service destruction. */
    public void onServiceDestroyed() {
        mService = null;
    }

    public boolean processIntent(Service service, Intent intent) {
        if (intent == null || mMediaNotificationInfo == null) return false;

        if (intent.getAction() == null) {
            // The intent comes from  {@link AppHooks#startForegroundService}.
            onServiceStarted(service);
        } else {
            // The intent comes from the notification. In this case, {@link onServiceStarted()}
            // does need to be called.
            processAction(intent.getAction());
        }
        return true;
    }

    public void processAction(String action) {
        if (ACTION_STOP.equals(action)
                || ACTION_SWIPE.equals(action)
                || ACTION_CANCEL.equals(action)) {
            onStop(MediaNotificationListener.ACTION_SOURCE_MEDIA_NOTIFICATION);
            stopListenerService();
        } else if (ACTION_PLAY.equals(action)) {
            onPlay(MediaNotificationListener.ACTION_SOURCE_MEDIA_NOTIFICATION);
        } else if (ACTION_PAUSE.equals(action)) {
            onPause(MediaNotificationListener.ACTION_SOURCE_MEDIA_NOTIFICATION);
        } else if (AudioManager.ACTION_AUDIO_BECOMING_NOISY.equals(action)) {
            onPause(MediaNotificationListener.ACTION_SOURCE_HEADSET_UNPLUG);
        } else if (ACTION_PREVIOUS_TRACK.equals(action)) {
            onMediaSessionAction(MediaSessionAction.PREVIOUS_TRACK);
        } else if (ACTION_NEXT_TRACK.equals(action)) {
            onMediaSessionAction(MediaSessionAction.NEXT_TRACK);
        } else if (ACTION_SEEK_FORWARD.equals(action)) {
            onMediaSessionAction(MediaSessionAction.SEEK_FORWARD);
        } else if (ACTION_SEEK_BACKWARD.equals(action)) {
            onMediaSessionAction(MediaSessionAction.SEEK_BACKWARD);
        }
    }

    @VisibleForTesting
    public void onPlay(int actionSource) {
        // MediaSessionCompat calls this sometimes when `mMediaNotificationInfo`
        // is no longer available. It's unclear if it is a Support Library issue
        // or something that isn't properly cleaned up but given that the
        // crashes are rare and the fix is simple, null check was enough.
        if (mMediaNotificationInfo == null || !mMediaNotificationInfo.isPaused) return;
        mMediaNotificationInfo.listener.onPlay(actionSource);
    }

    @VisibleForTesting
    public void onPause(int actionSource) {
        // MediaSessionCompat calls this sometimes when `mMediaNotificationInfo`
        // is no longer available. It's unclear if it is a Support Library issue
        // or something that isn't properly cleaned up but given that the
        // crashes are rare and the fix is simple, null check was enough.
        if (mMediaNotificationInfo == null || mMediaNotificationInfo.isPaused) return;
        mMediaNotificationInfo.listener.onPause(actionSource);
    }

    @VisibleForTesting
    public void onStop(int actionSource) {
        // MediaSessionCompat calls this sometimes when `mMediaNotificationInfo`
        // is no longer available. It's unclear if it is a Support Library issue
        // or something that isn't properly cleaned up but given that the
        // crashes are rare and the fix is simple, null check was enough.
        if (mMediaNotificationInfo == null) return;
        mMediaNotificationInfo.listener.onStop(actionSource);
    }

    @VisibleForTesting
    public void onMediaSessionAction(int action) {
        // MediaSessionCompat calls this sometimes when `mMediaNotificationInfo`
        // is no longer available. It's unclear if it is a Support Library issue
        // or something that isn't properly cleaned up but given that the
        // crashes are rare and the fix is simple, null check was enough.
        if (mMediaNotificationInfo == null) return;
        mMediaNotificationInfo.listener.onMediaSessionAction(action);
    }

    @VisibleForTesting
    void onMediaSessionSeekTo(long pos) {
        // MediaSessionCompat calls this sometimes when `mMediaNotificationInfo`
        // is no longer available. It's unclear if it is a Support Library issue
        // or something that isn't properly cleaned up but given that the
        // crashes are rare and the fix is simple, null check was enough.
        if (mMediaNotificationInfo == null) return;
        mMediaNotificationInfo.listener.onMediaSessionSeekTo(pos);
    }

    @VisibleForTesting
    public void showNotification(MediaNotificationInfo mediaNotificationInfo) {
        if (shouldIgnoreMediaNotificationInfo(mMediaNotificationInfo, mediaNotificationInfo)) {
            return;
        }

        mMediaNotificationInfo = mediaNotificationInfo;

        // If there's no pending service start request, don't try to start service. If there is a
        // pending service start request but the service haven't started yet, only update the
        // |mMediaNotificationInfo|. The service will update the notification later once it's
        // started.
        if (mService == null && mediaNotificationInfo.isPaused) return;

        if (mService == null) {
            updateMediaSession();
            updateNotificationBuilder();
            // This is not allowed from the background, and there is no workaround on S+.  Just
            // catch the exception, and `mService` will remain null for us to try again later.
            try {
                ForegroundServiceUtils.getInstance()
                        .startForegroundService(mDelegate.createServiceIntent());
            } catch (RuntimeException e) {
            }
        } else {
            updateNotification(false, false);
        }
    }

    private static boolean shouldIgnoreMediaNotificationInfo(
            MediaNotificationInfo oldInfo, MediaNotificationInfo newInfo) {
        // If this is a web MediaSession notification, but we haven't yet gotten actions, then we
        // shouldn't display the notification.
        if (newInfo.mediaSessionActions != null && newInfo.mediaSessionActions.isEmpty()) {
            return true;
        }

        return newInfo.equals(oldInfo)
                || (newInfo.isPaused
                        && oldInfo != null
                        && newInfo.instanceId != oldInfo.instanceId);
    }

    public void clearNotification() {
        mThrottler.clearPendingNotifications();
        if (mMediaNotificationInfo == null) return;

        NotificationManagerCompat.from(getContext()).cancel(mMediaNotificationInfo.id);

        if (mMediaSession != null) {
            mMediaSession.setCallback(null);
            mMediaSession.setActive(false);
            mMediaSession.release();
            mMediaSession = null;
        }
        stopListenerService();
        mMediaNotificationInfo = null;
        mNotificationBuilder = null;
    }

    public void queueNotification(MediaNotificationInfo mediaNotificationInfo) {
        mThrottler.queueNotification(mediaNotificationInfo);
    }

    public void hideNotification(int instanceId) {
        if (mMediaNotificationInfo == null || instanceId != mMediaNotificationInfo.instanceId) {
            return;
        }
        clearNotification();
    }

    /** Schedules the `mPendingIntentActionSwipe` construction if needed. */
    @VisibleForTesting
    public void schedulePendingIntentConstructionIfNeeded() {
        if (mPendingIntentInitializer == null) {
            return;
        }
        mPendingIntentInitializer.schedulePendingIntentConstructionIfNeeded();
    }

    @VisibleForTesting
    public void stopListenerService() {
        if (mService == null) return;

        ForegroundServiceUtils.getInstance()
                .stopForeground(mService, Service.STOP_FOREGROUND_REMOVE);
        mService.stopSelf();
    }

    @NonNull
    @VisibleForTesting
    public MediaMetadataCompat createMetadata() {
        // Can't return null as {@link MediaSessionCompat#setMetadata()} will crash in some versions
        // of the Android compat library.
        MediaMetadataCompat.Builder metadataBuilder = new MediaMetadataCompat.Builder();
        if (mMediaNotificationInfo.isPrivate) return metadataBuilder.build();

        metadataBuilder.putString(
                MediaMetadataCompat.METADATA_KEY_TITLE, getSafeNotificationTitle());
        metadataBuilder.putString(
                MediaMetadataCompat.METADATA_KEY_ARTIST, mMediaNotificationInfo.origin);

        if (!TextUtils.isEmpty(mMediaNotificationInfo.metadata.getArtist())) {
            metadataBuilder.putString(
                    MediaMetadataCompat.METADATA_KEY_ARTIST,
                    mMediaNotificationInfo.metadata.getArtist());
        }
        if (!TextUtils.isEmpty(mMediaNotificationInfo.metadata.getAlbum())) {
            metadataBuilder.putString(
                    MediaMetadataCompat.METADATA_KEY_ALBUM,
                    mMediaNotificationInfo.metadata.getAlbum());
        }
        if (mMediaNotificationInfo.mediaSessionImage != null) {
            metadataBuilder.putBitmap(
                    MediaMetadataCompat.METADATA_KEY_ALBUM_ART,
                    mMediaNotificationInfo.mediaSessionImage);
        }
        if (mMediaNotificationInfo.mediaPosition != null) {
            metadataBuilder.putLong(
                    MediaMetadataCompat.METADATA_KEY_DURATION,
                    mMediaNotificationInfo.mediaPosition.getDuration());
        }

        return metadataBuilder.build();
    }

    @VisibleForTesting
    public void updateNotification(boolean serviceStarting, boolean shouldLogNotification) {
        if (mService == null) return;

        if (mMediaNotificationInfo == null) {
            if (serviceStarting) {
                finishStartingForegroundServiceOnO(
                        mService,
                        mDelegate.createNotificationWrapperBuilder().buildNotificationWrapper());
                ForegroundServiceUtils.getInstance()
                        .stopForeground(mService, Service.STOP_FOREGROUND_REMOVE);
            }
            return;
        }
        updateMediaSession();
        updateNotificationBuilder();

        NotificationWrapper notification = mNotificationBuilder.buildNotificationWrapper();

        // On O, finish starting the foreground service nevertheless, or Android will
        // crash Chrome.
        boolean finishedForegroundingService =
                serviceStarting && finishStartingForegroundServiceOnO(mService, notification);

        // We keep the service as a foreground service while the media is playing. When it is not,
        // the service isn't stopped but is no longer in foreground, thus at a lower priority.
        // While the service is in foreground, the associated notification can't be swipped away.
        // Moving it back to background allows the user to remove the notification.
        if (mMediaNotificationInfo.supportsSwipeAway() && mMediaNotificationInfo.isPaused) {
            ForegroundServiceUtils.getInstance()
                    .stopForeground(mService, Service.STOP_FOREGROUND_DETACH);
            BaseNotificationManagerProxy manager =
                    BaseNotificationManagerProxyFactory.create(getContext());
            manager.notify(notification);
        } else if (!finishedForegroundingService) {
            // We did not foreground the service and update the notification above, so we should do
            // so here.  On S and later, we cannot foreground the service if we're not currently
            // in the foreground, and on Q and later the background activity start restrictions
            // prevent us from launching a trampoline to fix it.  Try it, and see if it works.  If
            // not, then update the notification and leave the service in the background.
            try {
                ForegroundServiceUtils.getInstance()
                        .startForeground(
                                mService,
                                mMediaNotificationInfo.id,
                                notification.getNotification(),
                                ServiceInfo.FOREGROUND_SERVICE_TYPE_MEDIA_PLAYBACK);
            } catch (RuntimeException e) {
                BaseNotificationManagerProxy manager =
                        BaseNotificationManagerProxyFactory.create(getContext());
                manager.notify(notification);
            }
        }
        if (shouldLogNotification) {
            mDelegate.logNotificationShown(notification);
        }
    }

    @VisibleForTesting
    public void updateNotificationBuilder() {
        assert (mMediaNotificationInfo != null);

        mNotificationBuilder = mDelegate.createNotificationWrapperBuilder();
        setMediaStyleLayoutForNotificationBuilder(mNotificationBuilder);

        mNotificationBuilder.setShowWhen(false);
        mNotificationBuilder.setSmallIcon(mMediaNotificationInfo.notificationSmallIcon);
        mNotificationBuilder.setAutoCancel(false);
        mNotificationBuilder.setLocalOnly(true);
        mNotificationBuilder.setGroup(mDelegate.getNotificationGroupName());
        mNotificationBuilder.setGroupSummary(true);

        if (mMediaNotificationInfo.supportsSwipeAway()) {
            mNotificationBuilder.setOngoing(!mMediaNotificationInfo.isPaused);
            assert (mPendingIntentActionSwipe != null);
            mNotificationBuilder.setDeleteIntent(mPendingIntentActionSwipe);
        }

        // The intent will currently only be null when using a custom tab.
        // TODO(avayvod) work out what we should do in this case. See https://crbug.com/585395.
        if (mMediaNotificationInfo.contentIntent != null) {
            mNotificationBuilder.setContentIntent(
                    PendingIntentProvider.getActivity(
                            getContext(),
                            mMediaNotificationInfo.instanceId,
                            mMediaNotificationInfo.contentIntent,
                            PendingIntent.FLAG_UPDATE_CURRENT
                                    | IntentUtils.getPendingIntentMutabilityFlag(false)));
            // Set FLAG_UPDATE_CURRENT so that the intent extras is updated, otherwise the
            // intent extras will stay the same for the same tab.
        }

        mNotificationBuilder.setVisibility(
                mMediaNotificationInfo.isPrivate
                        ? NotificationCompat.VISIBILITY_PRIVATE
                        : NotificationCompat.VISIBILITY_PUBLIC);
    }

    @VisibleForTesting
    public void updateMediaSession() {
        if (!mMediaNotificationInfo.supportsPlayPause()) return;

        if (mMediaSession == null) mMediaSession = createMediaSession();

        activateAndroidMediaSession(mMediaNotificationInfo.instanceId);

        mDelegate.onMediaSessionUpdated(mMediaSession);

        mMediaSession.setMetadata(createMetadata());

        mMediaSession.setPlaybackState(createPlaybackState());
    }

    @VisibleForTesting
    public PlaybackStateCompat createPlaybackState() {
        PlaybackStateCompat.Builder playbackStateBuilder =
                new PlaybackStateCompat.Builder().setActions(computeMediaSessionActions());

        int state =
                mMediaNotificationInfo.isPaused
                        ? PlaybackStateCompat.STATE_PAUSED
                        : PlaybackStateCompat.STATE_PLAYING;

        if (mMediaNotificationInfo.mediaPosition != null) {
            playbackStateBuilder.setState(
                    state,
                    mMediaNotificationInfo.mediaPosition.getPosition(),
                    mMediaNotificationInfo.mediaPosition.getPlaybackRate(),
                    mMediaNotificationInfo.mediaPosition.getLastUpdatedTime());
        } else {
            playbackStateBuilder.setState(
                    state, PlaybackStateCompat.PLAYBACK_POSITION_UNKNOWN, 1.0f);
        }

        return playbackStateBuilder.build();
    }

    private long computeMediaSessionActions() {
        assert mMediaNotificationInfo != null;

        long actions = PlaybackStateCompat.ACTION_PLAY | PlaybackStateCompat.ACTION_PAUSE;

        Set<Integer> availableActions = mMediaNotificationInfo.mediaSessionActions;
        if (availableActions != null) {
            if (availableActions.contains(MediaSessionAction.PREVIOUS_TRACK)) {
                actions |= PlaybackStateCompat.ACTION_SKIP_TO_PREVIOUS;
            }
            if (availableActions.contains(MediaSessionAction.NEXT_TRACK)) {
                actions |= PlaybackStateCompat.ACTION_SKIP_TO_NEXT;
            }
            if (availableActions.contains(MediaSessionAction.SEEK_FORWARD)) {
                actions |= PlaybackStateCompat.ACTION_FAST_FORWARD;
            }
            if (availableActions.contains(MediaSessionAction.SEEK_BACKWARD)) {
                actions |= PlaybackStateCompat.ACTION_REWIND;
            }
            if (availableActions.contains(MediaSessionAction.SEEK_TO)) {
                actions |= PlaybackStateCompat.ACTION_SEEK_TO;
            }
        }
        return actions;
    }

    private MediaSessionCompat createMediaSession() {
        MediaSessionCompat mediaSession =
                new MediaSessionCompat(getContext(), mDelegate.getAppName());
        mediaSession.setCallback(mMediaSessionCallback);
        mediaSession.setActive(true);
        return mediaSession;
    }

    /**
     * Activates the media session.
     * @param instanceId the instance of the notification to activate. If it doesn't match the
     *         active notification, this method will no-op.
     */
    public void activateAndroidMediaSession(int instanceId) {
        if (mMediaNotificationInfo == null) return;
        if (mMediaNotificationInfo.instanceId != instanceId) return;
        if (!mMediaNotificationInfo.supportsPlayPause() || mMediaNotificationInfo.isPaused) return;
        if (mMediaSession == null) return;
        mMediaSession.setActive(true);
    }

    private void setMediaStyleLayoutForNotificationBuilder(NotificationWrapperBuilder builder) {
        setMediaStyleNotificationText(builder);
        if (!mMediaNotificationInfo.supportsPlayPause()) {
            // Non-playback (Cast) notification will not use MediaStyle, so not
            // setting the large icon is fine.
            builder.setLargeIcon(null);
            // Notifications in incognito shouldn't show an icon to avoid leaking information.
        } else if (mMediaNotificationInfo.notificationLargeIcon != null
                && !mMediaNotificationInfo.isPrivate) {
            builder.setLargeIcon(mMediaNotificationInfo.notificationLargeIcon);
        }
        addNotificationButtons(builder);
    }

    private void addNotificationButtons(NotificationWrapperBuilder builder) {
        Set<Integer> actions = new HashSet<>();

        // TODO(zqzhang): handle other actions when play/pause is not supported? See
        // https://crbug.com/667500
        if (mMediaNotificationInfo.supportsPlayPause()) {
            if (mMediaNotificationInfo.mediaSessionActions != null) {
                actions.addAll(mMediaNotificationInfo.mediaSessionActions);
            }
            if (mMediaNotificationInfo.isPaused) {
                actions.remove(MediaSessionAction.PAUSE);
                actions.add(MediaSessionAction.PLAY);
            } else {
                actions.remove(MediaSessionAction.PLAY);
                actions.add(MediaSessionAction.PAUSE);
            }
        }

        if (mMediaNotificationInfo.supportsStop()) {
            actions.add(MediaSessionAction.STOP);
        } else {
            actions.remove(MediaSessionAction.STOP);
        }

        List<Integer> bigViewActions = computeBigViewActions(actions);

        for (int action : bigViewActions) {
            MediaButtonInfo buttonInfo = mActionToButtonInfo.get(action);
            builder.addAction(
                    buttonInfo.iconResId,
                    getContext().getResources().getString(buttonInfo.descriptionResId),
                    createPendingIntent(buttonInfo.intentString),
                    buttonInfo.buttonId);
        }

        // Only apply MediaStyle when NotificationInfo supports play/pause.
        if (mMediaNotificationInfo.supportsPlayPause()) {
            builder.setMediaStyle(mMediaSession, computeCompactViewActionIndices(bigViewActions));
        }
    }

    private void setMediaStyleNotificationText(NotificationWrapperBuilder builder) {
        if (mMediaNotificationInfo.isPrivate) {
            // Notifications in incognito shouldn't show what is playing to avoid leaking
            // information.
            builder.setContentTitle(
                    getContext().getResources().getString(R.string.media_notification_incognito));
            builder.setSubText(
                    getContext().getResources().getString(R.string.notification_incognito_tab));
            return;
        }

        builder.setContentTitle(getSafeNotificationTitle());
        String artistAndAlbumText = getArtistAndAlbumText(mMediaNotificationInfo.metadata);
        builder.setContentText(artistAndAlbumText);
        builder.setSubText(mMediaNotificationInfo.origin);
    }

    private static String getArtistAndAlbumText(MediaMetadata metadata) {
        String artist = (metadata.getArtist() == null) ? "" : metadata.getArtist();
        String album = (metadata.getAlbum() == null) ? "" : metadata.getAlbum();
        if (artist.isEmpty() || album.isEmpty()) {
            return artist + album;
        }
        return artist + " - " + album;
    }

    /**
     * Compute the actions to be shown in BigView media notification.
     *
     * The method assumes PLAY and PAUSE cannot coexist.
     */
    private static List<Integer> computeBigViewActions(Set<Integer> actions) {
        // PLAY and PAUSE cannot coexist.
        assert !actions.contains(MediaSessionAction.PLAY)
                || !actions.contains(MediaSessionAction.PAUSE);

        int[] actionByOrder = {
            MediaSessionAction.PREVIOUS_TRACK,
            MediaSessionAction.SEEK_BACKWARD,
            MediaSessionAction.PLAY,
            MediaSessionAction.PAUSE,
            MediaSessionAction.SEEK_FORWARD,
            MediaSessionAction.NEXT_TRACK,
            MediaSessionAction.STOP,
        };

        // Sort the actions based on the expected ordering in the UI.
        List<Integer> sortedActions = new ArrayList<>();
        for (int action : actionByOrder) {
            if (actions.contains(action)) sortedActions.add(action);
            // Actions are not prioritized ,so when total actions are more then
            // {@link BIG_VIEW_ACTIONS_COUNT} ACTION_STOP will be dropped per the decided order.
            if (sortedActions.size() == BIG_VIEW_ACTIONS_COUNT) break;
        }

        return sortedActions;
    }

    /**
     * Compute the actions to be shown in CompactView media notification.
     *
     * The method assumes PLAY and PAUSE cannot coexist. This method also assumes that it is only
     * called when at least play or pause is supported.
     *
     * Actions in pairs are preferred if there are more actions than |COMPACT_VIEW_ACTIONS_COUNT|.
     */
    @VisibleForTesting
    static int[] computeCompactViewActionIndices(List<Integer> actions) {
        // PLAY and PAUSE cannot coexist.
        assert !actions.contains(MediaSessionAction.PLAY)
                || !actions.contains(MediaSessionAction.PAUSE);

        if (actions.size() <= COMPACT_VIEW_ACTIONS_COUNT) {
            // If the number of actions is less than |COMPACT_VIEW_ACTIONS_COUNT|, just return an
            // array of 0, 1, ..., |actions.size()|-1.
            int[] actionsArray = new int[actions.size()];
            for (int i = 0; i < actions.size(); ++i) actionsArray[i] = i;
            return actionsArray;
        }

        // The rest of this method is broken if |COMPACT_VIEW_ACTIONS_COUNT| changes from 3.
        assert COMPACT_VIEW_ACTIONS_COUNT == 3;

        // If we have both PREVIOUS_TRACK and NEXT_TRACK, then show those with PLAY or PAUSE in the
        // middle.
        if (actions.contains(MediaSessionAction.PREVIOUS_TRACK)
                && actions.contains(MediaSessionAction.NEXT_TRACK)) {
            int[] actionsArray = new int[COMPACT_VIEW_ACTIONS_COUNT];
            actionsArray[0] = actions.indexOf(MediaSessionAction.PREVIOUS_TRACK);
            if (actions.contains(MediaSessionAction.PLAY)) {
                actionsArray[1] = actions.indexOf(MediaSessionAction.PLAY);
            } else {
                actionsArray[1] = actions.indexOf(MediaSessionAction.PAUSE);
            }
            actionsArray[2] = actions.indexOf(MediaSessionAction.NEXT_TRACK);
            return actionsArray;
        }

        // If we have both SEEK_FORWARD and SEEK_BACKWARD, then show those with PLAY or PAUSE in the
        // middle.
        if (actions.contains(MediaSessionAction.SEEK_BACKWARD)
                && actions.contains(MediaSessionAction.SEEK_FORWARD)) {
            int[] actionsArray = new int[COMPACT_VIEW_ACTIONS_COUNT];
            actionsArray[0] = actions.indexOf(MediaSessionAction.SEEK_BACKWARD);
            if (actions.contains(MediaSessionAction.PLAY)) {
                actionsArray[1] = actions.indexOf(MediaSessionAction.PLAY);
            } else {
                actionsArray[1] = actions.indexOf(MediaSessionAction.PAUSE);
            }
            actionsArray[2] = actions.indexOf(MediaSessionAction.SEEK_FORWARD);
            return actionsArray;
        }

        // Only show STOP with PLAY and not with PAUSE.
        List<Integer> compactActions = new ArrayList<>();
        if (actions.contains(MediaSessionAction.PAUSE)) {
            compactActions.add(actions.indexOf(MediaSessionAction.PAUSE));
        } else {
            compactActions.add(actions.indexOf(MediaSessionAction.PLAY));
            if (actions.contains(MediaSessionAction.STOP)) {
                compactActions.add(actions.indexOf(MediaSessionAction.STOP));
            }
        }

        return CollectionUtil.integerCollectionToIntArray(compactActions);
    }

    private static Context getContext() {
        return ContextUtils.getApplicationContext();
    }

    // Return a non-blank string for use as the notification title, to avoid issues on some
    // versions of Android.
    private String getSafeNotificationTitle() {
        String title = mMediaNotificationInfo.metadata.getTitle();
        if (title != null && title.trim().length() > 0) {
            return title;
        }
        return getContext().getPackageName();
    }
}
