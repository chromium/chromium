// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.components.external_intents;

import android.app.Activity;

import org.chromium.components.navigation_interception.NavigationParams;
import org.chromium.content_public.browser.WebContents;

/**
 * An interface via which the embedder provides the context information that
 * InterceptNavigationDelegateImpl needs.
 */
public interface InterceptNavigationDelegateClient {
    /* Returns the WebContents in the context of which this InterceptNavigationDelegateImpl instance
     * is operating. */
    WebContents getWebContents();

    /* Creates an ExternalNavigationHandler instance that is configured for this client. */
    ExternalNavigationHandler createExternalNavigationHandler();

    /* Returns the time of the user's last interaction with the app. */
    long getLastUserInteractionTime();

    /* Gets a RedirectHandler instance that is associated with this client, creating it if
     * necessary. */
    RedirectHandler getOrCreateRedirectHandler();

    /* Creates an AuthenticatorNavigationInterceptor instance that is configured for this client.
     */
    AuthenticatorNavigationInterceptor createAuthenticatorNavigationInterceptor();

    /* Returns whether whether the tab associated with this client is incognito. */
    boolean isIncognito();

    /* Returns whether whether the tab associated with this client is currently hidden. */
    boolean isHidden();

    /* Returns the Activity associated with this client. */
    Activity getActivity();

    /* Returns true if the tab associated with this client was launched from an external app. */
    boolean wasTabLaunchedFromExternalApp();

    /* Returns true if the tab associated with this client was launched from a long press in the
     * background. */
    boolean wasTabLaunchedFromLongPressInBackground();

    /* Invoked when the tab associated with this client should be closed. */
    void closeTab();

    /* Invoked when a navigation has begun in the InterceptNavigationDelegateImpl instance
     * associated with this instance. */
    void onNavigationStarted(NavigationParams params);
}
