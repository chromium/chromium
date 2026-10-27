// Copyright 2015 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.components.media_router;

import androidx.mediarouter.media.MediaRouter;
import androidx.mediarouter.media.MediaRouter.RouteInfo;

import org.chromium.build.annotations.NullMarked;
import org.chromium.build.annotations.Nullable;

/** A common descriptor of a device that can present some URI. */
@NullMarked
public class MediaSink {
    private static final String CAST_SINK_URN_PREFIX = "urn:x-org.chromium:media:sink:cast-";
    private final String mId;
    private final String mName;

    /**
     * Constructor.
     *
     * @param id A unique identifier of the sink.
     * @param name A user friendly name of the sink.
     */
    public MediaSink(String id, String name) {
        mId = id;
        mName = name;
    }

    /** @return The unique identifier of the sink. */
    public String getId() {
        return mId;
    }

    /** @return The user friendly name of the sink. */
    public String getName() {
        return mName;
    }

    /** @return The valid URN representing the sink. */
    public String getUrn() {
        return CAST_SINK_URN_PREFIX + getId();
    }

    @Override
    public boolean equals(Object o) {
        if (o == this) return true;
        if (o instanceof MediaSink other) {
            return mId.equals(other.getId()) && mName.equals(other.getName());
        }
        return false;
    }

    @Override
    public int hashCode() {
        final int prime = 31;
        int result = 1;
        result = prime * result + ((mId == null) ? 0 : mId.hashCode());
        result = prime * result + ((mName == null) ? 0 : mName.hashCode());
        return result;
    }

    @Override
    public String toString() {
        return String.format("MediaSink: %s, %s", getId(), getName());
    }

    /**
     * @param route The route information provided by Android.
     * @return A new MediaSink instance corresponding to the specified {@link RouteInfo}.
     */
    public static MediaSink fromRoute(MediaRouter.RouteInfo route) {
        return new MediaSink(route.getId(), route.getName());
    }

    /**
     * @param sinkId The id of the sink to find among known media routes.
     * @param router The instance of {@link MediaRouter} to enumerate the routes with.
     * @return A {@link MediaSink} corresponding to the {@link RouteInfo} with the specified id if
     * found, null otherwise.
     */
    public static @Nullable MediaSink fromSinkId(String sinkId, MediaRouter router) {
        for (MediaRouter.RouteInfo route : router.getRoutes()) {
            MediaSink sink = MediaSink.fromRoute(route);
            if (sink.getId().equals(sinkId)) return sink;
        }
        return null;
    }
}
