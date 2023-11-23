// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.components.browser_ui.site_settings;

import org.chromium.content_public.browser.BrowserContextHandle;

/**
 * Enables custom implementation for the notification site settings category, similar to
 * {@link LocationCategory}.
 */
public class NotificationCategory extends SiteSettingsCategory {
    NotificationCategory(BrowserContextHandle browserContextHandle) {
        // Android does not treat notifications as a 'permission', i.e. notification status cannot
        // be checked via Context#checkPermission(). Hence we pass an empty string here and override
        // #enabledForChrome() to use the notification-status checking API instead.
        super(browserContextHandle, Type.NOTIFICATIONS, /* androidPermission= */ "");
    }
}
