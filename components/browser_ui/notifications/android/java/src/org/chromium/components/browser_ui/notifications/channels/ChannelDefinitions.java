// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.components.browser_ui.notifications.channels;

import android.app.NotificationChannel;
import android.app.NotificationChannelGroup;
import android.content.res.Resources;
import android.media.AudioAttributes;
import android.net.Uri;
import android.os.Build;

import androidx.annotation.RequiresApi;

import java.util.HashSet;
import java.util.List;
import java.util.Set;

/**
 * Contains the properties of all our pre-definable notification channels on Android O+. See also
 * {@link ChromeChannelDefinitions}. <br/>
 * <br/>
 * See the README.md alongside ChromeChannelDefinitions.java for more information before adding or
 * changing any channels.
 */
@RequiresApi(Build.VERSION_CODES.O)
public abstract class ChannelDefinitions {
    /** @return A set of all known channel group ids that can be used for {@link #getChannelGroup}. */
    public abstract Set<String> getAllChannelGroupIds();

    /** @return A set of all known channel ids that can be used for {@link #getChannelFromId}. */
    public abstract Set<String> getAllChannelIds();

    /** @return A set of channel ids of channels that should be initialized on startup. */
    public abstract Set<String> getStartupChannelIds();

    /** @return A set of channel group ids of channel groups that should be initialized on startup. */
    public Set<String> getStartupChannelGroupIds() {
        Set<String> groupIds = new HashSet<>();
        for (String id : getStartupChannelIds()) {
            groupIds.add(getChannelFromId(id).mGroupId);
        }
        return groupIds;
    }

    /**
     * @return An array of old ChannelIds that may have been returned by
     *         {@link #getStartupChannelIds} in the past, but are no longer in use.
     */
    public abstract List<String> getLegacyChannelIds();

    public PredefinedChannelGroup getChannelGroupForChannel(PredefinedChannel channel) {
        return getChannelGroup(channel.mGroupId);
    }

    public abstract PredefinedChannelGroup getChannelGroup(String groupId);

    public abstract PredefinedChannel getChannelFromId(String channelId);

    /**
     * @param channelId a channel ID which may or may not correlate to a PredefinedChannel.
     * @return true if the input is a valid channel ID other than one returned in
     *     {@link getAllChannelIds}.
     */
    public boolean isValidNonPredefinedChannelId(String channelId) {
        return false;
    }

    /**
     * Helper class for storing predefined channel properties while allowing the channel name to be
     * lazily evaluated only when it is converted to an actual NotificationChannel.
     */
    public static class PredefinedChannel {
        private static final boolean SHOW_NOTIFICATION_BADGES_DEFAULT = false;
        private static final boolean SUPPRESS_SOUND_DEFAULT = false;

        private final String mId;
        private final int mNameResId;
        private final int mImportance;
        private final String mGroupId;
        private final boolean mShowNotificationBadges;
        private final boolean mSuppressSound;

        public static PredefinedChannel create(
                String id, int nameResId, int importance, String groupId) {
            return new PredefinedChannel(
                    id,
                    nameResId,
                    importance,
                    groupId,
                    SHOW_NOTIFICATION_BADGES_DEFAULT,
                    SUPPRESS_SOUND_DEFAULT);
        }

        public static PredefinedChannel createBadged(
                String id, int nameResId, int importance, String groupId) {
            return new PredefinedChannel(
                    id, nameResId, importance, groupId, true, SUPPRESS_SOUND_DEFAULT);
        }

        public static PredefinedChannel createSilenced(
                String id, int nameResId, int importance, String groupId) {
            return new PredefinedChannel(
                    id, nameResId, importance, groupId, SHOW_NOTIFICATION_BADGES_DEFAULT, true);
        }

        private PredefinedChannel(
                String id,
                int nameResId,
                int importance,
                String groupId,
                boolean showNotificationBadges,
                boolean suppressSound) {
            this.mId = id;
            this.mNameResId = nameResId;
            this.mImportance = importance;
            this.mGroupId = groupId;
            this.mShowNotificationBadges = showNotificationBadges;
            this.mSuppressSound = suppressSound;
        }

        NotificationChannel toNotificationChannel(Resources resources) {
            String name = resources.getString(mNameResId);
            NotificationChannel channel = new NotificationChannel(mId, name, mImportance);
            channel.setGroup(mGroupId);
            channel.setShowBadge(mShowNotificationBadges);

            if (mSuppressSound) {
                AudioAttributes attributes =
                        new AudioAttributes.Builder()
                                .setContentType(AudioAttributes.CONTENT_TYPE_SONIFICATION)
                                .setUsage(AudioAttributes.USAGE_NOTIFICATION)
                                .build();

                // Passing a null sound causes no sound to be played.
                Uri sound = null;
                channel.setSound(sound, attributes);
            }

            return channel;
        }
    }

    /**
     * Helper class for storing predefined channel group properties while allowing the group name to
     * be lazily evaluated only when it is converted to an actual NotificationChannelGroup.
     */
    public static class PredefinedChannelGroup {
        public final String mId;
        public final int mNameResId;

        public PredefinedChannelGroup(String id, int nameResId) {
            this.mId = id;
            this.mNameResId = nameResId;
        }

        public NotificationChannelGroup toNotificationChannelGroup(Resources resources) {
            String name = resources.getString(mNameResId);
            return new NotificationChannelGroup(mId, name);
        }
    }
}
