// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.components.browser_ui.notifications;

/**
 * Lists base::Features that can be accessed through {@link NotificationsFeatureMap}.
 *
 * <p>Should be kept in sync with |kFeaturesExposedToJava| in
 * //components/browser_ui/notifications/android/notifications_feature_map.cc
 */
public abstract class NotificationsFeatureList {
    public static final String ASYNC_NOTIFICATION_MANAGER = "AsyncNotificationManager";
}
