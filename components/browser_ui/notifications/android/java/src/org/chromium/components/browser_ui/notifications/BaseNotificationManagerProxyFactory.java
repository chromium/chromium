// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.components.browser_ui.notifications;

import androidx.annotation.Nullable;

import org.chromium.base.ResettersForTesting;
import org.chromium.base.ThreadUtils;
import org.chromium.components.browser_ui.util.BrowserUiUtilsCachedFlags;

/** Factory class for creating BaseNotificationManagerProxyFactory. */
public class BaseNotificationManagerProxyFactory {
    @Nullable private static BaseNotificationManagerProxy sProxyForTest;

    private BaseNotificationManagerProxyFactory() {}

    public static BaseNotificationManagerProxy create() {
        if (sProxyForTest != null) {
            return sProxyForTest;
        } else if (BrowserUiUtilsCachedFlags.getInstance().getAsyncNotificationManagerFlag()) {
            return new AsyncNotificationManagerProxyImpl();
        } else {
            return NotificationManagerProxyImpl.getInstance();
        }
    }

    /** Overrides the proxy instance for tests. */
    public static void setInstanceForTesting(BaseNotificationManagerProxy proxy) {
        ThreadUtils.runOnUiThreadBlocking(
                (Runnable)
                        () -> {
                            sProxyForTest = proxy;
                            if (proxy instanceof NotificationManagerProxy) {
                                NotificationManagerProxyImpl.setInstanceForTesting(
                                        (NotificationManagerProxy) proxy);
                            }
                        });
        ResettersForTesting.register(() -> sProxyForTest = null);
    }
}
