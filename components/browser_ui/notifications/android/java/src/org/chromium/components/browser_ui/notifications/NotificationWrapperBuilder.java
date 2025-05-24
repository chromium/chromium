// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.components.browser_ui.notifications;

import android.app.Notification;
import android.app.PendingIntent;
import android.graphics.Bitmap;
import android.graphics.drawable.Icon;
import android.net.Uri;
import android.os.Bundle;
import android.support.v4.media.session.MediaSessionCompat;
import android.widget.RemoteViews;

import androidx.core.app.NotificationCompat;

import org.chromium.build.annotations.NullMarked;
import org.chromium.build.annotations.Nullable;

/** Abstraction over Notification.Builder and NotificationCompat.Builder interfaces. */
@NullMarked
public interface NotificationWrapperBuilder {
    NotificationWrapperBuilder setAutoCancel(boolean autoCancel);

    @Deprecated
    NotificationWrapperBuilder setContentIntent(@Nullable PendingIntent contentIntent);

    NotificationWrapperBuilder setContentIntent(@Nullable PendingIntentProvider contentIntent);

    NotificationWrapperBuilder setContentTitle(@Nullable CharSequence title);

    NotificationWrapperBuilder setContentText(@Nullable CharSequence text);

    NotificationWrapperBuilder setSmallIcon(int icon);

    NotificationWrapperBuilder setSmallIcon(Icon icon);

    NotificationWrapperBuilder setColor(int argb);

    NotificationWrapperBuilder setTicker(@Nullable CharSequence text);

    NotificationWrapperBuilder setLocalOnly(boolean localOnly);

    NotificationWrapperBuilder setGroup(String group);

    NotificationWrapperBuilder setGroupSummary(boolean isGroupSummary);

    NotificationWrapperBuilder addExtras(Bundle extras);

    NotificationWrapperBuilder setOngoing(boolean ongoing);

    NotificationWrapperBuilder setVisibility(int visibility);

    NotificationWrapperBuilder setShowWhen(boolean showWhen);

    @Deprecated
    NotificationWrapperBuilder addAction(int icon, CharSequence title, PendingIntent intent);

    /** @param actionType is for UMA. In Chrome, this is {@link NotificationUmaTracker.ActionType}. */
    NotificationWrapperBuilder addAction(
            int icon, CharSequence title, PendingIntentProvider intent, int actionType);

    @Deprecated
    NotificationWrapperBuilder addAction(Notification.Action action);

    /** @param actionType is for UMA. In Chrome, this is {@link NotificationUmaTracker.ActionType}. */
    NotificationWrapperBuilder addAction(
            Notification.Action action, int flags, int actionType, int requestCode);

    @Deprecated
    NotificationWrapperBuilder addAction(NotificationCompat.Action action);

    /** @param actionType is for UMA. In Chrome, this is {@link NotificationUmaTracker.ActionType}. */
    NotificationWrapperBuilder addAction(
            NotificationCompat.Action action, int flags, int actionType, int requestCode);

    @Deprecated
    NotificationWrapperBuilder setDeleteIntent(@Nullable PendingIntent intent);

    NotificationWrapperBuilder setDeleteIntent(@Nullable PendingIntentProvider intent);

    NotificationWrapperBuilder setDeleteIntent(
            @Nullable PendingIntentProvider intent, int actionType);

    /**
     * Sets the priority of single notification on Android versions prior to Oreo.
     * (From Oreo onwards, priority is instead determined by channel importance.)
     */
    NotificationWrapperBuilder setPriorityBeforeO(int pri);

    NotificationWrapperBuilder setProgress(int max, int percentage, boolean indeterminate);

    NotificationWrapperBuilder setSubText(@Nullable CharSequence text);

    NotificationWrapperBuilder setWhen(long time);

    NotificationWrapperBuilder setLargeIcon(@Nullable Bitmap icon);

    NotificationWrapperBuilder setVibrate(long[] vibratePattern);

    NotificationWrapperBuilder setSound(Uri sound);

    NotificationWrapperBuilder setSilent(boolean silent);

    NotificationWrapperBuilder setDefaults(int defaults);

    NotificationWrapperBuilder setOnlyAlertOnce(boolean onlyAlertOnce);

    NotificationWrapperBuilder setPublicVersion(@Nullable Notification publicNotification);

    NotificationWrapperBuilder setContent(RemoteViews views);

    NotificationWrapperBuilder setBigPictureStyle(
            Bitmap bigPicture, @Nullable CharSequence summaryText);

    NotificationWrapperBuilder setBigTextStyle(@Nullable CharSequence bigText);

    NotificationWrapperBuilder setMediaStyle(MediaSessionCompat session, int[] actions);

    NotificationWrapperBuilder setCategory(String category);

    /** Sets the lifetime of a notification. Does nothing prior to Oreo. */
    NotificationWrapperBuilder setTimeoutAfter(long ms);

    NotificationWrapper buildWithBigContentView(RemoteViews bigView);

    NotificationWrapper buildWithBigTextStyle(String bigText);

    @Deprecated
    @Nullable
    Notification build();

    NotificationWrapper buildNotificationWrapper();
}
