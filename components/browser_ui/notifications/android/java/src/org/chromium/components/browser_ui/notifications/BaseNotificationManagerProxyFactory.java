// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.components.browser_ui.notifications;

import android.content.Context;

import org.chromium.components.browser_ui.util.BrowserUiUtilsCachedFlags;

/** Factory class for creating BaseNotificationManagerProxyFactory. */
public class BaseNotificationManagerProxyFactory {
    private BaseNotificationManagerProxyFactory() {}

    public static BaseNotificationManagerProxy create(Context context) {
        if (BrowserUiUtilsCachedFlags.getInstance().getAsyncNotificationManagerFlag()) {
            return new AsyncNotificationManagerProxyImpl(context);
        } else {
            return new NotificationManagerProxyImpl(context);
        }
    }
}
