// Copyright 2016 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.components.media_router;

import androidx.mediarouter.media.MediaRouter;
import androidx.mediarouter.media.MediaRouter.RouteInfo;

import org.robolectric.util.ReflectionHelpers;

/** Utility classes and methods for MediaRouterTests. */
public class TestUtils {
    /**
     * Creates a mock {@link RouteInfo} to supply where needed in the tests.
     * @param id The id of the route
     * @param name The user friendly name of the route
     * @return The initialized mock RouteInfo instance
     */
    static RouteInfo createMockRouteInfo(String id, String name) {
        Class<?>[] paramClasses =
                new Class[] {MediaRouter.ProviderInfo.class, String.class, String.class};
        Object[] paramValues = new Object[] {null, "", ""};
        RouteInfo routeInfo =
                ReflectionHelpers.callConstructor(
                        RouteInfo.class,
                        ReflectionHelpers.ClassParameter.fromComponentLists(
                                paramClasses, paramValues));
        ReflectionHelpers.setField(routeInfo, "mUniqueId", id);
        ReflectionHelpers.setField(routeInfo, "mName", name);
        return routeInfo;
    }
}
