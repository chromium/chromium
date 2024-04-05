// Copyright 2015 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.components.browser_ui.media;

import android.content.Intent;
import android.graphics.Bitmap;
import android.text.TextUtils;

import androidx.annotation.Nullable;

import org.chromium.services.media_session.MediaMetadata;
import org.chromium.services.media_session.MediaPosition;

import java.util.Set;

/** Exposes information about the current media notification to the external clients. */
public class MediaNotificationInfo {
    // Bits defining various user actions supported by the media notification.

    /** If set, play/pause controls are shown and handled via notification UI and MediaSession. */
    public static final int ACTION_PLAY_PAUSE = 1 << 0;

    /** If set, a stop button is shown and handled via the notification UI. */
    public static final int ACTION_STOP = 1 << 1;

    /**
     * If set, a user can swipe the notification away when it's paused.
     * If notification swipe is not supported, it will behave like {@link #ACTION_STOP}.
     */
    public static final int ACTION_SWIPEAWAY = 1 << 2;

    /** A value that represents an invalid ID. */
    public static final int INVALID_ID = -1;

    /** Use this class to construct an instance of {@link MediaNotificationInfo}. */
    public static final class Builder {
        private MediaMetadata mMetadata;
        private boolean mIsPaused;
        private String mOrigin = "";
        private int mInstanceId = INVALID_ID;
        private boolean mIsPrivate = true;
        private int mNotificationSmallIcon;
        private Bitmap mNotificationLargeIcon;
        private int mDefaultNotificationLargeIcon;
        private Bitmap mMediaSessionImage;
        private int mActions = ACTION_PLAY_PAUSE | ACTION_SWIPEAWAY | ACTION_STOP;
        private int mId = INVALID_ID;
        private Intent mContentIntent;
        private MediaNotificationListener mListener;
        private Set<Integer> mMediaSessionActions;
        private @Nullable MediaPosition mMediaPosition;

        /** Initializes the builder with the default values. */
        public Builder() {}

        public MediaNotificationInfo build() {
            assert mMetadata != null;
            assert mOrigin != null;
            assert mListener != null;

            return new MediaNotificationInfo(
                    mMetadata,
                    mIsPaused,
                    mOrigin,
                    mInstanceId,
                    mIsPrivate,
                    mNotificationSmallIcon,
                    mNotificationLargeIcon,
                    mDefaultNotificationLargeIcon,
                    mMediaSessionImage,
                    mActions,
                    mId,
                    mContentIntent,
                    mListener,
                    mMediaSessionActions,
                    mMediaPosition);
        }

        public Builder setMetadata(MediaMetadata metadata) {
            mMetadata = metadata;
            return this;
        }

        public Builder setPaused(boolean isPaused) {
            mIsPaused = isPaused;
            return this;
        }

        public Builder setOrigin(String origin) {
            mOrigin = origin;
            return this;
        }

        public Builder setInstanceId(int instanceId) {
            mInstanceId = instanceId;
            return this;
        }

        public Builder setPrivate(boolean isPrivate) {
            mIsPrivate = isPrivate;
            return this;
        }

        public Builder setNotificationSmallIcon(int icon) {
            mNotificationSmallIcon = icon;
            return this;
        }

        public Builder setNotificationLargeIcon(Bitmap icon) {
            mNotificationLargeIcon = icon;
            return this;
        }

        public Builder setDefaultNotificationLargeIcon(int icon) {
            mDefaultNotificationLargeIcon = icon;
            return this;
        }

        public Builder setMediaSessionImage(Bitmap image) {
            mMediaSessionImage = image;
            return this;
        }

        public Builder setActions(int actions) {
            mActions = actions;
            return this;
        }

        public Builder setId(int id) {
            mId = id;
            return this;
        }

        public Builder setContentIntent(Intent intent) {
            mContentIntent = intent;
            return this;
        }

        public Builder setListener(MediaNotificationListener listener) {
            mListener = listener;
            return this;
        }

        public Builder setMediaSessionActions(Set<Integer> actions) {
            mMediaSessionActions = actions;
            return this;
        }

        public Builder setMediaPosition(@Nullable MediaPosition position) {
            mMediaPosition = position;
            return this;
        }
    }

    /** The bitset defining user actions handled by the notification. */
    private final int mActions;

    /** The metadata associated with the media. */
    public final MediaMetadata metadata;

    /** The current state of the media, paused or not. */
    public final boolean isPaused;

    /** The origin of the tab containing the media. */
    public final String origin;

    /**
     * An identifier that helps distinguish different instances of the same type of media
     * notification. The {@link id} is shared by different MediaNotificationInfo instances for the
     * same media type, but this identifier provides an extra layer of differentiation. In Chrome,
     * for example, this corresponds to the source tab.
     */
    public final int instanceId;

    /** Whether the media notification should be considered as private. */
    public final boolean isPrivate;

    /** The id of the notification small icon from R.drawable. */
    public final int notificationSmallIcon;

    /** The Bitmap resource used as the notification large icon. */
    public final Bitmap notificationLargeIcon;

    /** The id of the default notification large icon from R.drawable. */
    public final int defaultNotificationLargeIcon;

    /**
     * The Bitmap resource used for Android MediaSession image, which will be used on lock screen
     * and wearable devices.
     */
    public final Bitmap mediaSessionImage;

    /** The id to use for the Android Notification. */
    public final int id;

    /** The intent to send when the notification is selected. */
    public final Intent contentIntent;

    /** The listener for the control events. */
    public final MediaNotificationListener listener;

    /**
     * The actions enabled in MediaSession. If null, the notification is not associated with a web
     * Media Session.
     */
    public final @Nullable Set<Integer> mediaSessionActions;

    /** The current position of the media session. */
    public final @Nullable MediaPosition mediaPosition;

    /** @return if play/pause actions are supported by this notification. */
    public boolean supportsPlayPause() {
        return (mActions & ACTION_PLAY_PAUSE) != 0;
    }

    /** @return if stop action is supported by this notification. */
    public boolean supportsStop() {
        return (mActions & ACTION_STOP) != 0;
    }

    /** @return if notification should be dismissable by swiping it away when paused. */
    public boolean supportsSwipeAway() {
        return (mActions & ACTION_SWIPEAWAY) != 0;
    }

    /**
     * Create a new MediaNotificationInfo.
     * @param metadata The metadata associated with the media.
     * @param isPaused The current state of the media, paused or not.
     * @param origin The origin of the tab containing the media.
     * @param instanceId The id of the tab containing the media.
     * @param isPrivate Whether the media notification should be considered as private.
     * @param notificationSmallIcon The small icon used in the notification.
     * @param notificationLargeIcon The large icon used in the notification.
     * @param defaultNotificationLargeIcon The fallback large icon when |notificationLargeIcon| is
     *        improper to use.
     * @param mediaSessionImage The artwork image to be used in Android MediaSession.
     * @param actions The actions supported in this notification.
     * @param id The id of this notification, which is used for distinguishing media playback, cast
     *        and media remote.
     * @param contentIntent The intent to send when the notification is selected.
     * @param listener The listener for the control events.
     * @param mediaSessionActions The actions supported by the page.
     * @param mediaPosition The current position of the media.
     */
    private MediaNotificationInfo(
            MediaMetadata metadata,
            boolean isPaused,
            String origin,
            int instanceId,
            boolean isPrivate,
            int notificationSmallIcon,
            Bitmap notificationLargeIcon,
            int defaultNotificationLargeIcon,
            Bitmap mediaSessionImage,
            int actions,
            int id,
            Intent contentIntent,
            MediaNotificationListener listener,
            Set<Integer> mediaSessionActions,
            @Nullable MediaPosition mediaPosition) {
        this.metadata = metadata;
        this.isPaused = isPaused;
        this.origin = origin;
        assert instanceId != INVALID_ID;
        this.instanceId = instanceId;
        this.isPrivate = isPrivate;
        this.notificationSmallIcon = notificationSmallIcon;
        this.notificationLargeIcon = notificationLargeIcon;
        this.defaultNotificationLargeIcon = defaultNotificationLargeIcon;
        this.mediaSessionImage = mediaSessionImage;
        this.mActions = actions;
        assert id != INVALID_ID;
        this.id = id;
        this.contentIntent = contentIntent;
        this.listener = listener;
        this.mediaSessionActions = mediaSessionActions;
        this.mediaPosition = mediaPosition;
    }

    @Override
    @SuppressWarnings("ReferenceEquality")
    public boolean equals(Object obj) {
        if (obj == this) return true;
        if (!(obj instanceof MediaNotificationInfo)) return false;

        MediaNotificationInfo other = (MediaNotificationInfo) obj;
        return isPaused == other.isPaused
                && isPrivate == other.isPrivate
                && instanceId == other.instanceId
                && notificationSmallIcon == other.notificationSmallIcon
                && (notificationLargeIcon == other.notificationLargeIcon
                        || (notificationLargeIcon != null
                                && notificationLargeIcon.sameAs(other.notificationLargeIcon)))
                && defaultNotificationLargeIcon == other.defaultNotificationLargeIcon
                && mediaSessionImage == other.mediaSessionImage
                && mActions == other.mActions
                && id == other.id
                && (metadata == other.metadata
                        || (metadata != null && metadata.equals(other.metadata)))
                && TextUtils.equals(origin, other.origin)
                && (contentIntent == other.contentIntent
                        || (contentIntent != null && contentIntent.equals(other.contentIntent)))
                && (listener == other.listener
                        || (listener != null && listener.equals(other.listener)))
                && (mediaSessionActions == other.mediaSessionActions
                        || (mediaSessionActions != null
                                && mediaSessionActions.equals(other.mediaSessionActions)))
                && mediaPosition == other.mediaPosition;
    }

    @Override
    public int hashCode() {
        int result = isPaused ? 1 : 0;
        result = 31 * result + (isPrivate ? 1 : 0);
        result = 31 * result + (metadata == null ? 0 : metadata.hashCode());
        result = 31 * result + (origin == null ? 0 : origin.hashCode());
        result = 31 * result + (contentIntent == null ? 0 : contentIntent.hashCode());
        result = 31 * result + instanceId;
        result = 31 * result + notificationSmallIcon;
        result =
                31 * result
                        + (notificationLargeIcon == null ? 0 : notificationLargeIcon.hashCode());
        result = 31 * result + defaultNotificationLargeIcon;
        result = 31 * result + (mediaSessionImage == null ? 0 : mediaSessionImage.hashCode());
        result = 31 * result + mActions;
        result = 31 * result + id;
        result = 31 * result + listener.hashCode();
        result = 31 * result + (mediaSessionActions == null ? 0 : mediaSessionActions.hashCode());
        result = 31 * result + (mediaPosition == null ? 0 : mediaPosition.hashCode());
        return result;
    }
}
