// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.components.navigation_interception;

import android.text.TextUtils;

import androidx.annotation.Nullable;

import org.chromium.base.annotations.CalledByNative;
import org.chromium.url.Origin;

/**
 * Navigation parameters container used to keep parameters for navigation interception.
 */
public class NavigationParams {
    /** Target URL of the navigation. */
    public final String url;

    /** The referrer URL for the navigation. */
    public final String referrer;

    /** True if the the navigation method is "POST". */
    public final boolean isPost;

    /** True if the navigation was initiated by the user. */
    public final boolean hasUserGesture;

    /** Page transition type (e.g. link / typed). */
    public final int pageTransitionType;

    /** Is the navigation a redirect (in which case URL is the "target" address). */
    public final boolean isRedirect;

    /** True if the target URL can't be handled by Chrome's internal protocol handlers. */
    public final boolean isExternalProtocol;

    /**
     * True if the navigation was originated from a navigation which had been
     * initiated by the user.
     */
    public final boolean hasUserGestureCarryover;

    /** True if the navigation was originated from the main frame. */
    public final boolean isMainFrame;

    /** True if navigation is renderer initiated. Eg clicking on a link. */
    public final boolean isRendererInitiated;

    /** Initiator origin of the request, could be null. */
    public final Origin initiatorOrigin;

    public NavigationParams(String url, String referrer, boolean isPost, boolean hasUserGesture,
            int pageTransitionType, boolean isRedirect, boolean isExternalProtocol,
            boolean isMainFrame, boolean isRendererInitiated, boolean hasUserGestureCarryover,
            @Nullable Origin initiatorOrigin) {
        this.url = url;
        this.referrer = TextUtils.isEmpty(referrer) ? null : referrer;
        this.isPost = isPost;
        this.hasUserGesture = hasUserGesture;
        this.pageTransitionType = pageTransitionType;
        this.isRedirect = isRedirect;
        this.isExternalProtocol = isExternalProtocol;
        this.isMainFrame = isMainFrame;
        this.isRendererInitiated = isRendererInitiated;
        this.hasUserGestureCarryover = hasUserGestureCarryover;
        this.initiatorOrigin = initiatorOrigin;
    }

    @CalledByNative
    public static NavigationParams create(String url, String referrer, boolean isPost,
            boolean hasUserGesture, int pageTransitionType, boolean isRedirect,
            boolean isExternalProtocol, boolean isMainFrame, boolean isRendererInitiated,
            boolean hasUserGestureCarryover, @Nullable Origin initiatorOrigin) {
        return new NavigationParams(url, referrer, isPost, hasUserGesture, pageTransitionType,
                isRedirect, isExternalProtocol, isMainFrame, isRendererInitiated,
                hasUserGestureCarryover, initiatorOrigin);
    }
}
