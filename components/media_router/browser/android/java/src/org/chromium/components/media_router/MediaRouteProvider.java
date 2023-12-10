// Copyright 2015 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.components.media_router;

import androidx.annotation.Nullable;

/**
 * An interface components providing media sinks and routes need to implement to hooks up into
 * {@link BrowserMediaRouter}.
 *
 * Note: Empty-string origins passed through this interface should be considered
 * "unique origins" from url::Origin for the purposes of comparison.
 */
public interface MediaRouteProvider {
    /** Factory for {@link MediaRouteProvider}s. */
    interface Factory {
        void addProviders(MediaRouteManager manager);
    }

    /**
     * @param sourceId The id of the source to check.
     * @return if the specified source is supported by this route provider.
     */
    boolean supportsSource(String sourceId);

    /**
     * Initiates the discovery of media sinks corresponding to the given source id. Does nothing if
     * the source id is not supported by the MRP.
     * @param sourceId The id of the source to discover the media sinks for.
     */
    void startObservingMediaSinks(String sourceId);

    /**
     * Stops the discovery of media sinks corresponding to the given source id. Does nothing if
     * the source id is not supported by the MRP.
     * @param sourceId The id of the source to discover the media sinks for.
     */
    void stopObservingMediaSinks(String sourceId);

    /**
     * Tries to create a media route from the given media source to the media sink.
     * @param sourceId The source to create the route for.
     * @param sinkId The sink to create the route for.
     * @param presentationId The presentation id generated for this route.
     * @param origin The origin of the frame initiating the request.
     * @param tabId The id of the tab containing the frame initiating the request.
     * @param isOffTheRecord Whether the route is being requested from an OffTheRecord profile.
     * @param nativeRequestId The id of the request tracked by the native side.
     */
    void createRoute(
            String sourceId,
            String sinkId,
            String presentationId,
            String origin,
            int tabId,
            boolean isOffTheRecord,
            int nativeRequestId);

    /**
     * Tries to join an existing media route for the given media source and presentation id.
     * @param sourceId The source of the route to join.
     * @param presentationId The presentation id for the route to join.
     * @param origin The origin of the frame initiating the request.
     * @param tabId The id of the tab containing the frame initiating the request.
     * @param nativeRequestId The id of the request tracked by the native side.
     */
    void joinRoute(
            String sourceId, String presentationId, String origin, int tabId, int nativeRequestId);

    /**
     * Closes the media route with the given id. The route must be created by this provider.
     * @param routeId The id of the route to close.
     */
    void closeRoute(String routeId);

    /**
     * Notifies the route that the page is not attached to it any longer. The route must be created
     * by this provider.
     * @param routeId The id of the route.
     */
    void detachRoute(String routeId);

    /**
     * Sends a message to the route with the given id. The route must be created by this provider.
     * @param routeId The id of the route to send the message to.
     * @param message The message to send.
     */
    void sendStringMessage(String routeId, String message);

    /**
     * Returns a FlingingController for the given route ID.
     * Returns null if no FlingingController can be retrieved from the given route ID.
     * @param routeId The id of the route.
     */
    @Nullable
    FlingingController getFlingingController(String routeId);
}
