// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.components.navigation_interception;

import org.jni_zero.CalledByNative;

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
     * @param hiddenCrossFrame whether the navigation has been initiated by another (hidden) frame.
     * @param isSandboxedFrame whether the navigation was initiated by a sandboxed frame.
     * @return true if the navigation should be ignored.
     */
    @CalledByNative
    public abstract boolean shouldIgnoreNavigation(
            NavigationHandle navigationHandle,
            GURL escapedUrl,
            boolean hiddenCrossFrame,
            boolean isSandboxedFrame);

    /**
     * This method is called for navigations to external protocols in subframes, which on Android
     * are handled similarly to how we handle main frame navigations that could result in
     * navigations to 3rd party applications. Note that for subframes only external protocols are
     * ever allowed to leave the browser.
     *
     * @param escapedUrl The url from the NavigationHandle, properly escaped for external
     *         navigation.
     * @param transition The {@link PageTransition} for the Navigation
     * @param hasUserGesture Whether the navigation is associated with a user gesture.
     * @param initiatorOrigin The Origin that initiated this navigation, if any.
     *
     * @return Tri-state: An empty URL indicating an async action is pending, a URL to redirect the
     *         subframe to, or null if no action is to be taken.
     */
    @CalledByNative
    protected GURL handleSubframeExternalProtocol(
            GURL escapedUrl,
            @PageTransition int transition,
            boolean hasUserGesture,
            Origin initiatorOrigin) {
        return null;
    }

    /**
     * This method is called when a main frame requests a resource with a user gesture (eg. xhr,
     * fetch, etc.). The page may wish to redirect to an app after the resource requests completes,
     * which may be after blink user activation has expired.
     */
    @CalledByNative
    protected void onResourceRequestWithGesture() {}
}
