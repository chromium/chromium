// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.components.media_router.caf;

import androidx.mediarouter.media.MediaRouter;

import org.chromium.components.media_router.MediaSink;
import org.chromium.components.media_router.MediaSource;

/** The information of create route requests. */
public class CreateRouteRequestInfo {
    public final MediaSource source;
    public final MediaSink sink;
    public final String presentationId;
    public final String origin;
    public final int tabId;
    public final boolean isOffTheRecord;
    public final int nativeRequestId;
    public final MediaRouter.RouteInfo routeInfo;

    public CreateRouteRequestInfo(MediaSource source, MediaSink sink, String presentationId,
            String origin, int tabId, boolean isOffTheRecord, int nativeRequestId,
            MediaRouter.RouteInfo routeInfo) {
        this.source = source;
        this.sink = sink;
        this.presentationId = presentationId;
        this.origin = origin;
        this.tabId = tabId;
        this.isOffTheRecord = isOffTheRecord;
        this.nativeRequestId = nativeRequestId;
        this.routeInfo = routeInfo;
    }
}
