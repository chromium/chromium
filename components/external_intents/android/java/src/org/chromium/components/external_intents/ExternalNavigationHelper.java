// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.components.external_intents;

import android.content.Context;
import android.content.Intent;

import org.chromium.build.annotations.NullMarked;

/** Helper for external navigation actions such as opening in app. */
@NullMarked
public interface ExternalNavigationHelper {
    /**
     * Launches an external app.
     *
     * @param intent The {@link Intent} used to launch the app.
     * @param context The {@link Context} needed to launch the app.
     */
    void launchExternalApp(Intent intent, Context context);

    /**
     * Launches an external app based on user confirmation to leave incognito.
     *
     * @param intent The {@link Intent} used to launch the app.
     * @param navigationId The navigation id of the current navigation.
     * @param context The {@link Context} needed to launch the app.
     * @param onUserConfirmation A {@link Runnable} to run if the user decides to launch the app.
     */
    void launchExternalAppWithIncognitoConfirmation(
            Intent intent, long navigationId, Context context, Runnable onUserConfirmation);
}
