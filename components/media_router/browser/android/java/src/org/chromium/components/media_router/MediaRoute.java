// Copyright 2015 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.components.media_router;

/** Contains all the info about the media route created by any {@link MediaRouteProvider}. */
public class MediaRoute {
    private static final String MEDIA_ROUTE_ID_PREFIX = "route:";
    private static final String MEDIA_ROUTE_ID_SEPARATOR = "/";

    /** The unique id of the route, assigned by the {@link BrowserMediaRouter}. */
    public final String id;

    /** The {@link MediaRouteProvider} unique id of the sink the route was created for. */
    public final String sinkId;

    /** The URL or URN that the route is casting. */
    private String mSourceId;

    /** The presentation id that was assigned to the route. */
    public final String presentationId;

    public MediaRoute(String sinkId, String sourceId, String presentationId) {
        this.id = createMediaRouteId(presentationId, sinkId, sourceId);
        this.sinkId = sinkId;
        this.mSourceId = sourceId;
        this.presentationId = presentationId;
    }

    private static String createMediaRouteId(
            String presentationId, String sinkId, String sourceUrn) {
        StringBuilder builder = new StringBuilder();
        builder.append(MEDIA_ROUTE_ID_PREFIX);
        builder.append(presentationId);
        builder.append(MEDIA_ROUTE_ID_SEPARATOR);
        builder.append(sinkId);
        builder.append(MEDIA_ROUTE_ID_SEPARATOR);
        builder.append(sourceUrn);
        return builder.toString();
    }

    public void setSourceId(String sourceId) {
        this.mSourceId = sourceId;
    }

    public String getSourceId() {
        return mSourceId;
    }
}
