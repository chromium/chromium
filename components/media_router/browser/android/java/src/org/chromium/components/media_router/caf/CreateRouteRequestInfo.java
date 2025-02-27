// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.components.media_router.caf;

import androidx.mediarouter.media.MediaRouter;

import org.chromium.components.media_router.MediaRoute;
import org.chromium.components.media_router.MediaSink;
import org.chromium.components.media_router.MediaSource;

/** The information of create route requests. */
public class CreateRouteRequestInfo {
    // `routeId` will remain unchanged, whether `mSource` changed or not. This
    // happens when media source for an existing route changes.
    private MediaSource mSource;
    public final String routeId;
    public final MediaSink sink;
    public final String presentationId;
    public final String origin;
    public final int tabId;
    public final boolean isOffTheRecord;
    public final int nativeRequestId;
    public final MediaRouter.RouteInfo routeInfo;

    public CreateRouteRequestInfo(
            MediaSource source,
            MediaSink sink,
            String presentationId,
            String origin,
            int tabId,
            boolean isOffTheRecord,
            int nativeRequestId,
            MediaRouter.RouteInfo routeInfo) {
        this.routeId = new MediaRoute(sink.getId(), source.getSourceId(), presentationId).id;
        this.mSource = source;
        this.sink = sink;
        this.presentationId = presentationId;
        this.origin = origin;
        this.tabId = tabId;
        this.isOffTheRecord = isOffTheRecord;
        this.nativeRequestId = nativeRequestId;
        this.routeInfo = routeInfo;
    }

    public MediaSource getMediaSource() {
        return this.mSource;
    }

    public void setMediaSource(MediaSource source) {
        this.mSource = source;
    }
}
