// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.components.browser_ui.notifications;

import android.app.Notification;
import android.app.PendingIntent;
import android.content.Context;
import android.graphics.Bitmap;
import android.graphics.drawable.Icon;
import android.media.session.MediaSession;
import android.net.Uri;
import android.os.Build;
import android.os.Bundle;
import android.support.v4.media.session.MediaSessionCompat;
import android.widget.RemoteViews;

import androidx.core.app.NotificationCompat;

import org.chromium.base.Log;
import org.chromium.base.metrics.RecordHistogram;
import org.chromium.components.browser_ui.notifications.channels.ChannelsInitializer;

/** Wraps a {@link Notification.Builder} object. */
public class NotificationWrapperStandardBuilder implements NotificationWrapperBuilder {
    private static final String TAG = "NotifStandardBuilder";
    private final Notification.Builder mBuilder;
    private final Context mContext;
    private final NotificationMetadata mMetadata;

    public NotificationWrapperStandardBuilder(
            Context context,
            String channelId,
            ChannelsInitializer channelsInitializer,
            NotificationMetadata metadata) {
        mContext = context;
        mBuilder = new Notification.Builder(mContext);
        if (Build.VERSION.SDK_INT >= Build.VERSION_CODES.O) {
            channelsInitializer.safeInitialize(channelId);
            mBuilder.setChannelId(channelId);
        }
        mMetadata = metadata;
    }

    @Override
    public NotificationWrapperBuilder setAutoCancel(boolean autoCancel) {
        mBuilder.setAutoCancel(autoCancel);
        return this;
    }

    @Override
    public NotificationWrapperBuilder setContentIntent(PendingIntent contentIntent) {
        mBuilder.setContentIntent(contentIntent);
        return this;
    }

    @Override
    public NotificationWrapperBuilder setContentIntent(PendingIntentProvider contentIntent) {
        mBuilder.setContentIntent(contentIntent.getPendingIntent());
        return this;
    }

    @Override
    public NotificationWrapperBuilder setContentTitle(CharSequence title) {
        mBuilder.setContentTitle(title);
        return this;
    }

    @Override
    public NotificationWrapperBuilder setContentText(CharSequence text) {
        mBuilder.setContentText(text);
        return this;
    }

    @Override
    public NotificationWrapperBuilder setSmallIcon(int icon) {
        mBuilder.setSmallIcon(icon);
        return this;
    }

    @Override
    public NotificationWrapperBuilder setSmallIcon(Icon icon) {
        mBuilder.setSmallIcon(icon);
        return this;
    }

    @Override
    public NotificationWrapperBuilder setColor(int argb) {
        mBuilder.setColor(argb);
        return this;
    }

    @Override
    public NotificationWrapperBuilder setTicker(CharSequence text) {
        mBuilder.setTicker(text);
        return this;
    }

    @Override
    public NotificationWrapperBuilder setLocalOnly(boolean localOnly) {
        mBuilder.setLocalOnly(localOnly);
        return this;
    }

    @Override
    public NotificationWrapperBuilder setGroup(String group) {
        mBuilder.setGroup(group);
        return this;
    }

    @Override
    public NotificationWrapperBuilder setGroupSummary(boolean isGroupSummary) {
        mBuilder.setGroupSummary(isGroupSummary);
        return this;
    }

    @Override
    public NotificationWrapperBuilder addExtras(Bundle extras) {
        mBuilder.addExtras(extras);
        return this;
    }

    @Override
    public NotificationWrapperBuilder setOngoing(boolean ongoing) {
        mBuilder.setOngoing(ongoing);
        return this;
    }

    @Override
    public NotificationWrapperBuilder setVisibility(int visibility) {
        mBuilder.setVisibility(visibility);
        return this;
    }

    @Override
    public NotificationWrapperBuilder setShowWhen(boolean showWhen) {
        mBuilder.setShowWhen(showWhen);
        return this;
    }

    @Override
    @SuppressWarnings("deprecation")
    public NotificationWrapperBuilder addAction(
            int icon, CharSequence title, PendingIntent intent) {
        if (icon != 0) {
            mBuilder.addAction(
                    new Notification.Action.Builder(
                                    Icon.createWithResource(mContext, icon), title, intent)
                            .build());
        } else {
            mBuilder.addAction(icon, title, intent);
        }
        return this;
    }

    @Override
    public NotificationWrapperBuilder addAction(
            int icon,
            CharSequence title,
            PendingIntentProvider pendingIntentProvider,
            int actionType) {
        addAction(icon, title, pendingIntentProvider.getPendingIntent());
        return this;
    }

    @Override
    public NotificationWrapperBuilder addAction(Notification.Action action) {
        mBuilder.addAction(action);
        return this;
    }

    @Override
    public NotificationWrapperBuilder addAction(
            Notification.Action action, int flags, int actionType, int requestCode) {
        action.actionIntent =
                new PendingIntentProvider(action.actionIntent, flags, requestCode)
                        .getPendingIntent();
        addAction(action);
        return this;
    }

    @Override
    public NotificationWrapperBuilder addAction(NotificationCompat.Action action) {
        Log.w(TAG, "Ignoring compat action in standard builder.");
        return this;
    }

    @Override
    public NotificationWrapperBuilder addAction(
            NotificationCompat.Action action, int flags, int actionType, int requestCode) {
        Log.w(TAG, "Ignoring compat action in standard builder.");
        return this;
    }

    @Override
    public NotificationWrapperBuilder setDeleteIntent(PendingIntent intent) {
        mBuilder.setDeleteIntent(intent);
        return this;
    }

    @Override
    public NotificationWrapperBuilder setDeleteIntent(PendingIntentProvider intent) {
        mBuilder.setDeleteIntent(intent.getPendingIntent());
        return this;
    }

    @Override
    public NotificationWrapperBuilder setDeleteIntent(
            PendingIntentProvider intent, int ignoredActionType) {
        return setDeleteIntent(intent);
    }

    @Override
    @SuppressWarnings("deprecation")
    public NotificationWrapperBuilder setPriorityBeforeO(int pri) {
        if (Build.VERSION.SDK_INT < Build.VERSION_CODES.O) {
            mBuilder.setPriority(pri);
        }
        return this;
    }

    @Override
    public NotificationWrapperBuilder setProgress(int max, int percentage, boolean indeterminate) {
        mBuilder.setProgress(max, percentage, indeterminate);
        return this;
    }

    @Override
    public NotificationWrapperBuilder setSubText(CharSequence text) {
        mBuilder.setSubText(text);
        return this;
    }

    @Override
    public NotificationWrapperBuilder setWhen(long time) {
        mBuilder.setWhen(time);
        return this;
    }

    @Override
    public NotificationWrapperBuilder setLargeIcon(Bitmap icon) {
        mBuilder.setLargeIcon(icon);
        return this;
    }

    @Override
    public NotificationWrapperBuilder setVibrate(long[] vibratePattern) {
        mBuilder.setVibrate(vibratePattern);
        return this;
    }

    @Override
    public NotificationWrapperBuilder setSound(Uri sound) {
        mBuilder.setSound(sound);
        return this;
    }

    @Override
    public NotificationWrapperBuilder setSilent(boolean silent) {
        // Not supported on non-compat builders.
        return this;
    }

    @Override
    public NotificationWrapperBuilder setDefaults(int defaults) {
        mBuilder.setDefaults(defaults);
        return this;
    }

    @Override
    public NotificationWrapperBuilder setOnlyAlertOnce(boolean onlyAlertOnce) {
        mBuilder.setOnlyAlertOnce(onlyAlertOnce);
        return this;
    }

    @Override
    public NotificationWrapperBuilder setPublicVersion(Notification publicNotification) {
        mBuilder.setPublicVersion(publicNotification);
        return this;
    }

    @Override
    @SuppressWarnings("deprecation")
    public NotificationWrapperBuilder setContent(RemoteViews views) {
        mBuilder.setCustomContentView(views);
        return this;
    }

    @Override
    public NotificationWrapperBuilder setBigPictureStyle(
            Bitmap bigPicture, CharSequence summaryText) {
        Notification.BigPictureStyle style =
                new Notification.BigPictureStyle().bigPicture(bigPicture);

        // Android N doesn't show content text when expanded, so duplicate body text as a
        // summary for the big picture.
        style.setSummaryText(summaryText);

        mBuilder.setStyle(style);
        return this;
    }

    @Override
    public NotificationWrapperBuilder setBigTextStyle(CharSequence bigText) {
        mBuilder.setStyle(new Notification.BigTextStyle().bigText(bigText));
        return this;
    }

    @Override
    public NotificationWrapperBuilder setMediaStyle(MediaSessionCompat session, int[] actions) {
        Notification.MediaStyle style = new Notification.MediaStyle();
        style.setMediaSession(((MediaSession) session.getMediaSession()).getSessionToken());
        style.setShowActionsInCompactView(actions);
        mBuilder.setStyle(style);
        return this;
    }

    @Override
    public NotificationWrapperBuilder setCategory(String category) {
        mBuilder.setCategory(category);
        return this;
    }

    @Override
    public NotificationWrapperBuilder setTimeoutAfter(long ms) {
        if (Build.VERSION.SDK_INT >= Build.VERSION_CODES.O) {
            mBuilder.setTimeoutAfter(ms);
        }
        return this;
    }

    @Override
    @SuppressWarnings("deprecation")
    public NotificationWrapper buildWithBigContentView(RemoteViews view) {
        assert mMetadata != null;
        return new NotificationWrapper(mBuilder.setCustomBigContentView(view).build(), mMetadata);
    }

    @Override
    public NotificationWrapper buildWithBigTextStyle(String bigText) {
        Notification.BigTextStyle bigTextStyle = new Notification.BigTextStyle();
        bigTextStyle.setBuilder(mBuilder);
        bigTextStyle.bigText(bigText);

        assert mMetadata != null;
        return new NotificationWrapper(bigTextStyle.build(), mMetadata);
    }

    @Override
    public Notification build() {
        boolean success = false;
        try {
            Notification notification = mBuilder.build();
            success = true;
            return notification;
        } finally {
            RecordHistogram.recordBooleanHistogram("Notifications.Android.Build", success);
        }
    }

    @Override
    public NotificationWrapper buildNotificationWrapper() {
        assert mMetadata != null;
        return new NotificationWrapper(build(), mMetadata);
    }

    protected Notification.Builder getBuilder() {
        return mBuilder;
    }

    protected NotificationMetadata getMetadata() {
        assert mMetadata != null;
        return mMetadata;
    }
}
