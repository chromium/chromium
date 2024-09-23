// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.components.browser_ui.notifications.channels;

import android.app.NotificationChannel;
import android.app.NotificationChannelGroup;
import android.app.NotificationManager;
import android.content.res.Resources;
import android.os.Build;

import androidx.annotation.RequiresApi;

import org.chromium.components.browser_ui.notifications.BaseNotificationManagerProxy;

import java.util.ArrayDeque;
import java.util.Collection;
import java.util.Collections;
import java.util.HashMap;
import java.util.HashSet;
import java.util.List;
import java.util.Queue;
import java.util.Set;

/** Initializes our notification channels. */
@RequiresApi(Build.VERSION_CODES.O)
public class ChannelsInitializer {
    private final BaseNotificationManagerProxy mNotificationManager;
    private final ChannelDefinitions mChannelDefinitions;
    private Resources mResources;
    private final Queue<Runnable> mPendingTasks = new ArrayDeque<>();
    private boolean mIsTaskRunning;

    public ChannelsInitializer(
            BaseNotificationManagerProxy notificationManagerProxy,
            ChannelDefinitions channelDefinitions,
            Resources resources) {
        mChannelDefinitions = channelDefinitions;
        mNotificationManager = notificationManagerProxy;
        mResources = resources;
    }

    /**
     * Creates all the channels on the notification manager that we want to appear in our
     * channel settings from first launch onwards.
     */
    public void initializeStartupChannels() {
        Set<String> groupIds = mChannelDefinitions.getStartupChannelGroupIds();
        Set<String> channelIds = mChannelDefinitions.getStartupChannelIds();
        ensureInitialized(groupIds, channelIds);
    }

    /**
     * Updates all the channels to reflect the correct locale.
     *
     * @param resources The new resources to use.
     */
    public void updateLocale(Resources resources) {
        mResources = resources;
        mPendingTasks.add(() -> runUpdateExistingKnownChannelsTask());
        processPendingTasks();
    }

    private void runUpdateExistingKnownChannelsTask() {
        mNotificationManager.getNotificationChannelGroups(
                (channelGroups) -> onChannelGroupsRetrieved(channelGroups));
    }

    private void onChannelGroupsRetrieved(List<NotificationChannelGroup> channelGroups) {
        mNotificationManager.getNotificationChannels(
                (channels) -> onNotificationChannelsRetrieved(channelGroups, channels));
    }

    private void onNotificationChannelsRetrieved(
            List<NotificationChannelGroup> channelGroups, List<NotificationChannel> channels) {
        Set<String> groupIds = new HashSet<>();
        Set<String> channelIds = new HashSet<>();
        for (NotificationChannelGroup group : channelGroups) {
            groupIds.add(group.getId());
        }
        for (NotificationChannel channel : channels) {
            channelIds.add(channel.getId());
        }
        // only re-initialize known channel ids, as we only want to update known & existing channels
        groupIds.retainAll(mChannelDefinitions.getAllChannelGroupIds());
        channelIds.retainAll(mChannelDefinitions.getAllChannelIds());
        runEnsureInitializedWithEnabledStateTask(groupIds, channelIds, true);
    }

    /**
     * Cleans up any old channels that are no longer required from previous versions of the app.
     * It's safe to call this multiple times since deleting an already-deleted channel is a no-op.
     */
    public void deleteLegacyChannels() {
        mPendingTasks.add(() -> runDeleteLegacyChannelsTask());
        processPendingTasks();
    }

    private void runDeleteLegacyChannelsTask() {
        for (String channelId : mChannelDefinitions.getLegacyChannelIds()) {
            mNotificationManager.deleteNotificationChannel(channelId);
        }
        onCurrentTaskFinished();
    }

    /**
     * Ensures the given channel has been created on the notification manager so a notification
     * can be safely posted to it. This should only be used for channel ids with an entry in
     * {@link ChannelDefinitions.PredefinedChannels}, or that start with a known prefix.
     *
     * Calling this is a (potentially lengthy) no-op if the channel has already been created.
     *
     * @param channelId The ID of the channel to be initialized.
     */
    public void ensureInitialized(String channelId) {
        ensureInitializedWithEnabledState(channelId, true);
    }

    /**
     * Ensures the given channels have been created on the notification manager so a notification
     * can be safely posted to them. This should only be used for channel ids with an entry in
     * {@link ChannelDefinitions.PredefinedChannels}, or that start with a known prefix.
     *
     * Calling this is a (potentially lengthy) no-op if the channels have already been created.
     *
     * @param groupIds The IDs of the channel groups to be initialized.
     * @param channelIds The IDs of the channel to be initialized.
     */
    public void ensureInitialized(Collection<String> groupIds, Collection<String> channelIds) {
        ensureInitializedWithEnabledState(groupIds, channelIds, true);
    }

    /**
     * As ensureInitialized, but create the channel in disabled mode. The channel's importance will
     * be set to IMPORTANCE_NONE, instead of using the value from
     * {@link ChannelDefinitions.PredefinedChannels}.
     */
    public void ensureInitializedAndDisabled(String channelId) {
        ensureInitializedWithEnabledState(channelId, false);
    }

    private ChannelDefinitions.PredefinedChannel getPredefinedChannel(String channelId) {
        if (mChannelDefinitions.isValidNonPredefinedChannelId(channelId)) return null;
        ChannelDefinitions.PredefinedChannel predefinedChannel =
                mChannelDefinitions.getChannelFromId(channelId);
        if (predefinedChannel == null) {
            throw new IllegalStateException("Could not initialize channel: " + channelId);
        }
        return predefinedChannel;
    }

    private void ensureInitializedWithEnabledState(String channelId, boolean enabled) {
        Collection<String> groupIds = Collections.emptyList();
        Collection<String> channelIds = Collections.singletonList(channelId);
        ensureInitializedWithEnabledState(groupIds, channelIds, enabled);
    }

    private void ensureInitializedWithEnabledState(
            Collection<String> groupIds, Collection<String> channelIds, boolean enabled) {
        mPendingTasks.add(
                () -> runEnsureInitializedWithEnabledStateTask(groupIds, channelIds, enabled));
        processPendingTasks();
    }

    private void runEnsureInitializedWithEnabledStateTask(
            Collection<String> groupIds, Collection<String> channelIds, boolean enabled) {
        HashMap<String, NotificationChannelGroup> channelGroups = new HashMap<>();
        HashMap<String, NotificationChannel> channels = new HashMap<>();

        for (String groupId : groupIds) {
            ChannelDefinitions.PredefinedChannelGroup predefinedChannelGroup =
                    mChannelDefinitions.getChannelGroup(groupId);
            if (predefinedChannelGroup == null) continue;
            NotificationChannelGroup channelGroup =
                    predefinedChannelGroup.toNotificationChannelGroup(mResources);
            channelGroups.put(channelGroup.getId(), channelGroup);
        }

        for (String channelId : channelIds) {
            ChannelDefinitions.PredefinedChannel predefinedChannel =
                    getPredefinedChannel(channelId);
            if (predefinedChannel == null) continue;
            NotificationChannelGroup channelGroup =
                    mChannelDefinitions
                            .getChannelGroupForChannel(predefinedChannel)
                            .toNotificationChannelGroup(mResources);
            NotificationChannel channel = predefinedChannel.toNotificationChannel(mResources);
            if (!enabled) {
                channel.setImportance(NotificationManager.IMPORTANCE_NONE);
            }
            channelGroups.put(channelGroup.getId(), channelGroup);
            channels.put(channel.getId(), channel);
        }

        // Channel groups must be created before the channels.
        for (var channelGroup : channelGroups.values()) {
            mNotificationManager.createNotificationChannelGroup(channelGroup);
        }
        for (var channel : channels.values()) {
            mNotificationManager.createNotificationChannel(channel);
        }
        onCurrentTaskFinished();
    }

    /**
     * This calls ensureInitialized after checking this isn't null.
     * @param channelId Id of the channel to be initialized.
     */
    public void safeInitialize(String channelId) {
        // The channelId may be null if the notification will be posted by another app that
        // does not target O or sets its own channels, e.g. WebAPK notifications.
        if (channelId == null) {
            return;
        }
        ensureInitialized(channelId);
    }

    private void processPendingTasks() {
        if (mIsTaskRunning || mPendingTasks.isEmpty()) {
            return;
        }
        mIsTaskRunning = true;
        mPendingTasks.remove().run();
    }

    private void onCurrentTaskFinished() {
        mIsTaskRunning = false;
        processPendingTasks();
    }
}
