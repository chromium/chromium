// Copyright 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.components.navigation_interception;

import org.chromium.base.annotations.CalledByNative;
import org.chromium.content_public.browser.NavigationHandle;
import org.chromium.ui.base.PageTransition;
import org.chromium.url.GURL;
import org.chromium.url.Origin;

public abstract class InterceptNavigationDelegate {
    /**
     * This method is called for every top-level navigation within the associated WebContents.
     * The method allows the embedder to ignore navigations. This is used on Android to 'convert'
     * certain navigations to Intents to 3rd party applications.
     *
     * @param navigationHandle parameters describing the navigation.
     * @param escapedUrl The url from the NavigationHandle, properly escaped for external
     *         navigation.
     * @return true if the navigation should be ignored.
     */
    @CalledByNative
    public abstract boolean shouldIgnoreNavigation(
            NavigationHandle navigationHandle, GURL escapedUrl);

    /**
     * This method is called for navigations to external protocols, which on Android are handled in
     * the same we handle regular navigations that could result in navigations to 3rd party
     * applications.
     *
     * @param escapedUrl The url from the NavigationHandle, properly escaped for external
     *         navigation.
     * @param transition The {@link PageTransition} for the Navigation
     * @param hasUserGesture Whether the navigation is associated with a user gesture.
     * @param initiatorOrigin The Origin that initiated this navigation, if any.
     */
    @CalledByNative
    private void handleExternalProtocolDialog(GURL escapedUrl, @PageTransition int transition,
            boolean hasUserGesture, Origin initiatorOrigin) {
        // TODO(https://crbug.com/1290507): Refactor this to construct the ExternalNavigationParams
        // directly and don't create an intermediate NavigationHandle.
        // Treat external protocol dialogs as a navigation to the provided |url|.
        NavigationHandle navigationHandle = new NavigationHandle(0 /* nativeNavigationHandleProxy*/,
                escapedUrl, GURL.emptyGURL() /* referrerUrl */,
                GURL.emptyGURL() /* baseUrlForDataUrl */, false /* isInPrimaryMainFrame */,
                false /* isSameDocument*/, true /* isRendererInitiated */, initiatorOrigin,
                transition, false /* isPost */, hasUserGesture, false /* isRedirect */,
                true /* isExternalProtocol */,
                0 /* navigationId - doesn't correspond to a native NavigationHandle*/,
                false /* isPageActivation */);
        shouldIgnoreNavigation(navigationHandle, escapedUrl);
    }
}
