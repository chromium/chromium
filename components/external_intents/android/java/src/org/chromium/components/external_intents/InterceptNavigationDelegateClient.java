// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.components.external_intents;

import android.app.Activity;

import org.chromium.build.annotations.NullMarked;
import org.chromium.content_public.browser.LoadUrlParams;
import org.chromium.content_public.browser.WebContents;

/**
 * An interface via which the embedder provides the context information that
 * InterceptNavigationDelegateImpl needs.
 */
@NullMarked
public interface InterceptNavigationDelegateClient {
    /* Returns the WebContents in the context of which this InterceptNavigationDelegateImpl instance
     * is operating. */
    WebContents getWebContents();

    /* Creates an ExternalNavigationHandler instance that is configured for this client. */
    ExternalNavigationHandler createExternalNavigationHandler();

    /* Gets a RedirectHandler instance that is associated with this client, creating it if
     * necessary. */
    RedirectHandler getOrCreateRedirectHandler();

    /* Returns whether whether the tab associated with this client is incognito. */
    boolean isIncognito();

    /* Returns the Activity associated with this client. */
    Activity getActivity();

    /* Returns true if the tab associated with this client was launched from an external app. */
    boolean wasTabLaunchedFromExternalApp();

    /* Returns true if the tab associated with this client was launched from a long press in the
     * background. */
    boolean wasTabLaunchedFromLongPressInBackground();

    /* Invoked when the tab associated with this client should be closed. */
    void closeTab();

    /**
     * Loads a URL as specified by |loadUrlParams| if possible. May fail in exceptional conditions
     * (e.g., if there is no valid tab).
     *
     * @param loadUrlParams parameters of the URL to be loaded
     */
    void loadUrlIfPossible(LoadUrlParams loadUrlParams);
}
