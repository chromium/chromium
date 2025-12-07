// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.components.navigation_interception;

import org.jni_zero.CalledByNative;
import org.jni_zero.JNINamespace;
import org.jni_zero.NativeMethods;

import org.chromium.base.RequiredCallback;
import org.chromium.build.annotations.NullMarked;
import org.chromium.build.annotations.Nullable;
import org.chromium.content_public.browser.NavigationHandle;
import org.chromium.content_public.browser.WebContents;
import org.chromium.ui.base.PageTransition;
import org.chromium.url.GURL;
import org.chromium.url.Origin;

@JNINamespace("navigation_interception")
@NullMarked
public abstract class InterceptNavigationDelegate {
    /**
     * This method is called for every top-level navigation within the associated WebContents. The
     * method allows the embedder to ignore navigations. This is used on Android to 'convert'
     * certain navigations to Intents to 3rd party applications.
     *
     * @param navigationHandle parameters describing the navigation.
     * @param escapedUrl The url from the NavigationHandle, properly escaped for external
     *     navigation.
     * @param hiddenCrossFrame whether the navigation has been initiated by another (hidden) frame.
     * @param isSandboxedFrame whether the navigation was initiated by a sandboxed frame.
     * @param shouldRunAsync whether any slow checks should run async.
     * @param resultCallback the callback that must be called synchrnously unless |shouldRunAsync|
     *     is true, in which case it must be called when the async check finishes, or before any
     *     calls to {@link #finishPendingShouldIgnoreCheck()} return.
     */
    public abstract void shouldIgnoreNavigation(
            NavigationHandle navigationHandle,
            GURL escapedUrl,
            boolean hiddenCrossFrame,
            boolean isSandboxedFrame,
            boolean shouldRunAsync,
            RequiredCallback<Boolean> resultCallback);

    /**
     * If an async shouldIgnoreNavigation request is in progress, it should be finished calling the
     * |resultCallback| before returning from this method.
     *
     * <p>If the request cannot be finished synchronously, this call can be ignored and the
     * navigation will be deferred until the
     */
    @CalledByNative
    protected void requestFinishPendingShouldIgnoreCheck() {}

    /**
     * This method is called for navigations to external protocols in subframes, which on Android
     * are handled similarly to how we handle main frame navigations that could result in
     * navigations to 3rd party applications. Note that for subframes only external protocols are
     * ever allowed to leave the browser.
     *
     * @param escapedUrl The url from the NavigationHandle, properly escaped for external
     *     navigation.
     * @param transition The {@link PageTransition} for the Navigation
     * @param hasUserGesture Whether the navigation is associated with a user gesture.
     * @param initiatorOrigin The Origin that initiated this navigation, if any.
     * @return Tri-state: An empty URL indicating an async action is pending, a URL to redirect the
     *     subframe to, or null if no action is to be taken.
     */
    @CalledByNative
    protected @Nullable GURL handleSubframeExternalProtocol(
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

    @CalledByNative
    private void callShouldIgnoreNavigation(
            NavigationHandle navigationHandle,
            GURL escapedUrl,
            boolean hiddenCrossFrame,
            boolean isSandboxedFrame,
            boolean shouldRunAsync) {
        RequiredCallback<Boolean> resultCallback =
                new RequiredCallback<>(
                        (Boolean shouldIgnore) -> {
                            InterceptNavigationDelegateJni.get()
                                    .onShouldIgnoreNavigationResult(
                                            navigationHandle.getWebContents(), shouldIgnore);
                        });
        shouldIgnoreNavigation(
                navigationHandle,
                escapedUrl,
                hiddenCrossFrame,
                isSandboxedFrame,
                shouldRunAsync,
                resultCallback);
    }

    @NativeMethods
    public interface Natives {
        void onShouldIgnoreNavigationResult(WebContents webContents, boolean shouldIgnore);
    }
}
