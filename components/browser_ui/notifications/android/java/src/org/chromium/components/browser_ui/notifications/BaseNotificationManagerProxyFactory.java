// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.components.browser_ui.notifications;

import android.content.Context;

/** Factory class for creating BaseNotificationManagerProxyFactory. */
public class BaseNotificationManagerProxyFactory {
    private BaseNotificationManagerProxyFactory() {}

    public static BaseNotificationManagerProxy create(Context context) {
        if (NotificationsFeatureMap.isEnabled(
                NotificationsFeatureList.ASYNC_NOTIFICATION_MANAGER)) {
            return new AsyncNotificationManagerProxyImpl(context);
        } else {
            return new NotificationManagerProxyImpl(context);
        }
    }
}
