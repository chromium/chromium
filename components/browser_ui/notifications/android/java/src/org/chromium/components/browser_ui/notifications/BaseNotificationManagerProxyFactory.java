// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.components.browser_ui.notifications;

import android.content.Context;

import androidx.annotation.NonNull;
import androidx.annotation.Nullable;

import org.chromium.base.ResettersForTesting;
import org.chromium.base.ThreadUtils;
import org.chromium.components.browser_ui.util.BrowserUiUtilsCachedFlags;

/** Factory class for creating BaseNotificationManagerProxyFactory. */
public class BaseNotificationManagerProxyFactory {
    @Nullable private static BaseNotificationManagerProxy sProxyForTest;

    private BaseNotificationManagerProxyFactory() {}

    public static BaseNotificationManagerProxy create(@NonNull Context applicationContext) {
        applicationContext = applicationContext.getApplicationContext();
        if (sProxyForTest != null) {
            return sProxyForTest;
        } else if (BrowserUiUtilsCachedFlags.getInstance().getAsyncNotificationManagerFlag()) {
            return new AsyncNotificationManagerProxyImpl(applicationContext);
        } else {
            return new NotificationManagerProxyImpl(applicationContext);
        }
    }

    /** Overrides the proxy instance for tests. */
    public static void setInstanceForTesting(BaseNotificationManagerProxy proxy) {
        ThreadUtils.runOnUiThreadBlocking((Runnable) () -> sProxyForTest = proxy);
        ResettersForTesting.register(() -> sProxyForTest = null);
    }
}
