// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.components.media_router;

import static org.junit.Assert.assertEquals;
import static org.junit.Assert.assertFalse;
import static org.junit.Assert.assertNull;
import static org.junit.Assert.assertTrue;

import android.app.Application;
import android.content.ComponentName;
import android.content.Intent;
import android.content.IntentFilter;
import android.content.pm.ResolveInfo;
import android.content.pm.ServiceInfo;
import android.os.Bundle;
import android.os.Handler;
import android.os.Looper;
import android.os.Message;
import android.os.Messenger;
import android.os.Parcel;
import android.os.Parcelable;
import android.os.RemoteException;

import androidx.mediarouter.media.MediaRouteSelector;
import androidx.mediarouter.media.MediaRouter;

import org.junit.After;
import org.junit.Before;
import org.junit.Test;
import org.junit.runner.RunWith;
import org.robolectric.RuntimeEnvironment;
import org.robolectric.Shadows;
import org.robolectric.shadows.ShadowLooper;

import org.chromium.base.test.BaseRobolectricTestRunner;

import java.nio.charset.Charset;
import java.nio.charset.StandardCharsets;
import java.util.ArrayList;
import java.util.List;

/** Tests for {@link MediaRouterContext}. */
@RunWith(BaseRobolectricTestRunner.class)
public class MediaRouterContextTest {
    private static final String CONTROL_CATEGORY = "org.example.CATEGORY";
    private static final String BAD_ROUTE_ID = "bad_route";
    private static final String GOOD_ROUTE_ID = "good_route";
    private static final String EXTRA_APP_PARCELABLE = "org.example.EXTRA_APP_PARCELABLE";
    private static final int CLIENT_MSG_REGISTER = 1;
    private static final int SERVICE_VERSION = 2;

    @Before
    public void setUp() {
        MediaRouter.resetGlobalRouter();
    }

    @After
    public void tearDown() {
        MediaRouter.resetGlobalRouter();
    }

    /**
     * A third-party provider service sends a route that Chrome can't unparcel. This crashed Chrome
     * (crbug.com/580092988). The bad route must be dropped and the good route kept.
     */
    @Test
    public void testProviderWithUnreadableRouteDoesNotCrash() {
        Application app = RuntimeEnvironment.getApplication();
        ComponentName componentName =
                new ComponentName("org.example.provider", "org.example.provider.Service");

        ResolveInfo resolveInfo = new ResolveInfo();
        resolveInfo.serviceInfo = new ServiceInfo();
        resolveInfo.serviceInfo.packageName = componentName.getPackageName();
        resolveInfo.serviceInfo.name = componentName.getClassName();
        Shadows.shadowOf(app.getPackageManager())
                .addResolveInfoForIntent(
                        new Intent(MediaRouterContext.PROVIDER_SERVICE_INTERFACE), resolveInfo);

        Bundle descriptor = createProviderDescriptorWithUnreadableRoute();
        Messenger fakeProvider =
                new Messenger(
                        new Handler(
                                Looper.getMainLooper(),
                                msg -> {
                                    if (msg.what == CLIENT_MSG_REGISTER && msg.replyTo != null) {
                                        Message reply = Message.obtain();
                                        reply.what = MediaRouterContext.SERVICE_MSG_REGISTERED;
                                        reply.arg1 = msg.arg1;
                                        reply.arg2 = SERVICE_VERSION;
                                        reply.obj = descriptor;
                                        try {
                                            msg.replyTo.send(reply);
                                        } catch (RemoteException e) {
                                            throw new RuntimeException(e);
                                        }
                                    }
                                    return true;
                                }));
        Shadows.shadowOf(app)
                .setComponentNameAndServiceForBindServiceForIntent(
                        new Intent(MediaRouterContext.PROVIDER_SERVICE_INTERFACE)
                                .setComponent(componentName),
                        componentName,
                        fakeProvider.getBinder());

        MediaRouter router = BrowserMediaRouter.getAndroidMediaRouter();
        MediaRouteSelector selector =
                new MediaRouteSelector.Builder().addControlCategory(CONTROL_CATEGORY).build();
        MediaRouter.Callback callback = new MediaRouter.Callback() {};
        router.addCallback(selector, callback, MediaRouter.CALLBACK_FLAG_REQUEST_DISCOVERY);
        try {
            // Without the fix, this throws BadParcelableException from inside MediaRouter.
            ShadowLooper.idleMainLooper();

            List<String> routeNames = new ArrayList<>();
            for (MediaRouter.RouteInfo route : router.getRoutes()) {
                routeNames.add(route.getName());
            }
            assertTrue(routeNames.toString(), routeNames.contains(GOOD_ROUTE_ID));
            assertFalse(routeNames.toString(), routeNames.contains(BAD_ROUTE_ID));
        } finally {
            router.removeCallback(callback);
        }
    }

    @Test
    @SuppressWarnings("deprecation") // The typed getter needs API 33.
    public void testSanitizeProviderDescriptorDropsOnlyUnreadableRoutes() {
        Bundle sanitized =
                MediaRouterContext.sanitizeProviderDescriptor(
                        createProviderDescriptorWithUnreadableRoute());
        List<String> routeIds = new ArrayList<>();
        for (Bundle route : sanitized.<Bundle>getParcelableArrayList("routes")) {
            routeIds.add(route.getString("id"));
        }
        assertEquals(List.of(GOOD_ROUTE_ID), routeIds);
    }

    /**
     * Route extras can hold classes that the boot class loader can't load, like CastDevice from
     * Google Play services. They must stay packed so that their readers can set their own class
     * loader, and the route must be kept.
     */
    @Test
    @SuppressWarnings("deprecation") // The typed getters need API 33.
    public void testRouteExtrasWithAppParcelableStayReadable() {
        Bundle extras = new Bundle();
        extras.putParcelable(EXTRA_APP_PARCELABLE, new AppParcelable());
        Bundle route = createRouteBundle(GOOD_ROUTE_ID, /* withControlFilter= */ false);
        route.putBundle("extras", extras);
        ArrayList<Bundle> routes = new ArrayList<>();
        routes.add(route);
        Bundle descriptor = new Bundle();
        descriptor.putParcelableArrayList("routes", routes);

        Bundle received = roundTrip(descriptor);
        // Mimic an IPC Bundle, which uses a class loader that only knows OS classes.
        received.setClassLoader(ClassLoader.getPlatformClassLoader());

        Bundle sanitized = MediaRouterContext.sanitizeProviderDescriptor(received);
        List<Bundle> sanitizedRoutes = sanitized.getParcelableArrayList("routes");
        assertEquals(1, sanitizedRoutes.size());
        Bundle sanitizedExtras = sanitizedRoutes.get(0).getBundle("extras");
        sanitizedExtras.setClassLoader(AppParcelable.class.getClassLoader());
        assertTrue(sanitizedExtras.getParcelable(EXTRA_APP_PARCELABLE) instanceof AppParcelable);
    }

    @Test
    @SuppressWarnings("deprecation") // The typed getter needs API 33.
    public void testDynamicRouteCreatedWithUnreadableGroupRouteBecomesGenericFailure() {
        ArrayList<Bundle> routes = new ArrayList<>();
        routes.add(createRouteBundle(BAD_ROUTE_ID, /* withControlFilter= */ true));
        Bundle routesBundle = new Bundle();
        routesBundle.putParcelableArrayList("routes", routes);
        Bundle badRoute =
                replaceIntentFilterClassName(routesBundle)
                        .<Bundle>getParcelableArrayList("routes")
                        .get(0);
        Bundle data = new Bundle();
        data.putBundle("groupRoute", badRoute);

        Message msg = Message.obtain();
        msg.what = MediaRouterContext.SERVICE_MSG_DYNAMIC_ROUTE_CREATED;
        msg.arg1 = 42;
        msg.obj = data;

        assertTrue(MediaRouterContext.sanitizeServiceMessage(msg));
        assertEquals(MediaRouterContext.SERVICE_MSG_GENERIC_FAILURE, msg.what);
        assertEquals(42, msg.arg1);
        assertNull(msg.obj);
    }

    @Test
    @SuppressWarnings("deprecation") // The typed getter needs API 33.
    public void testDynamicRouteDescriptorsChangedDropsOnlyUnreadableMembers() {
        ArrayList<Bundle> dynamicRoutes = new ArrayList<>();
        dynamicRoutes.add(createDynamicRouteBundle(BAD_ROUTE_ID, /* withControlFilter= */ true));
        dynamicRoutes.add(createDynamicRouteBundle(GOOD_ROUTE_ID, /* withControlFilter= */ false));
        Bundle data = new Bundle();
        data.putBundle("groupRoute", createRouteBundle("group", /* withControlFilter= */ false));
        data.putParcelableArrayList("dynamicRoutes", dynamicRoutes);

        Message msg = Message.obtain();
        msg.what = MediaRouterContext.SERVICE_MSG_DYNAMIC_ROUTE_DESCRIPTORS_CHANGED;
        msg.obj = replaceIntentFilterClassName(data);

        assertTrue(MediaRouterContext.sanitizeServiceMessage(msg));
        assertEquals(MediaRouterContext.SERVICE_MSG_DYNAMIC_ROUTE_DESCRIPTORS_CHANGED, msg.what);
        Bundle sanitized = (Bundle) msg.obj;
        assertEquals("group", sanitized.getBundle("groupRoute").getString("id"));
        List<String> memberIds = new ArrayList<>();
        for (Bundle member : sanitized.<Bundle>getParcelableArrayList("dynamicRoutes")) {
            memberIds.add(member.getBundle("mrDescriptor").getString("id"));
        }
        assertEquals(List.of(GOOD_ROUTE_ID), memberIds);
    }

    @Test
    public void testDynamicRouteDescriptorsChangedWithUnreadableGroupRouteIsDropped() {
        Bundle data = new Bundle();
        data.putBundle(
                "groupRoute", createRouteBundle(BAD_ROUTE_ID, /* withControlFilter= */ true));

        Message msg = Message.obtain();
        msg.what = MediaRouterContext.SERVICE_MSG_DYNAMIC_ROUTE_DESCRIPTORS_CHANGED;
        msg.obj = replaceIntentFilterClassName(data);

        // This is an update, not a reply to a request, so it is dropped instead of failed.
        assertFalse(MediaRouterContext.sanitizeServiceMessage(msg));
    }

    /** A Parcelable class that only exists in the app, not in the OS. */
    public static class AppParcelable implements Parcelable {
        public static final Creator<AppParcelable> CREATOR =
                new Creator<>() {
                    @Override
                    public AppParcelable createFromParcel(Parcel in) {
                        return new AppParcelable();
                    }

                    @Override
                    public AppParcelable[] newArray(int size) {
                        return new AppParcelable[size];
                    }
                };

        @Override
        public int describeContents() {
            return 0;
        }

        @Override
        public void writeToParcel(Parcel dest, int flags) {}
    }

    private static Bundle createRouteBundle(String id, boolean withControlFilter) {
        Bundle route = new Bundle();
        route.putString("id", id);
        route.putString("name", id);
        route.putBoolean("enabled", true);
        if (withControlFilter) {
            IntentFilter filter = new IntentFilter();
            filter.addCategory(CONTROL_CATEGORY);
            ArrayList<IntentFilter> filters = new ArrayList<>();
            filters.add(filter);
            route.putParcelableArrayList("controlFilters", filters);
        }
        return route;
    }

    /**
     * Returns a provider descriptor with two routes. The first route has a control filter whose
     * class does not exist in this process, like the custom IntentFilter subclass in the crash. The
     * second route is valid. It has no control filter, because Robolectric may write the
     * IntentFilter class name only once per Parcel.
     */
    private static Bundle createProviderDescriptorWithUnreadableRoute() {
        ArrayList<Bundle> routes = new ArrayList<>();
        routes.add(createRouteBundle(BAD_ROUTE_ID, /* withControlFilter= */ true));
        routes.add(createRouteBundle(GOOD_ROUTE_ID, /* withControlFilter= */ false));
        Bundle descriptor = new Bundle();
        descriptor.putParcelableArrayList("routes", routes);
        return replaceIntentFilterClassName(descriptor);
    }

    /** Returns a dynamic group member, which wraps a route descriptor. */
    private static Bundle createDynamicRouteBundle(String id, boolean withControlFilter) {
        Bundle dynamicRoute = new Bundle();
        dynamicRoute.putBundle("mrDescriptor", createRouteBundle(id, withControlFilter));
        dynamicRoute.putInt("selectionState", 1);
        return dynamicRoute;
    }

    /** Returns a Bundle that has been written to a Parcel and read back, like an IPC Bundle. */
    private static Bundle roundTrip(Bundle bundle) {
        Parcel parcel = Parcel.obtain();
        try {
            bundle.writeToParcel(parcel, 0);
            parcel.setDataPosition(0);
            return Bundle.CREATOR.createFromParcel(parcel);
        } finally {
            parcel.recycle();
        }
    }

    /**
     * Writes {@code bundle} to a Parcel, renames the IntentFilter class to one that does not exist,
     * and reads it back. Reading the IntentFilter values then fails with ClassNotFoundException,
     * like a Parcelable class that only exists in another app.
     */
    private static Bundle replaceIntentFilterClassName(Bundle bundle) {
        Parcel parcel = Parcel.obtain();
        try {
            bundle.writeToParcel(parcel, 0);
            byte[] bytes = parcel.marshall();
            // The new name must have the same length. Robolectric may encode strings as UTF-8 or
            // UTF-16, so try both.
            boolean replaced = false;
            for (Charset charset :
                    new Charset[] {StandardCharsets.UTF_16LE, StandardCharsets.UTF_8}) {
                byte[] target = "android.content.IntentFilter".getBytes(charset);
                byte[] replacement = "org.example.FakeIntentFilter".getBytes(charset);
                assertEquals(target.length, replacement.length);
                int index = indexOf(bytes, target);
                if (index >= 0) {
                    System.arraycopy(replacement, 0, bytes, index, replacement.length);
                    replaced = true;
                    break;
                }
            }
            assertTrue("IntentFilter class name not found in Parcel", replaced);

            parcel.unmarshall(bytes, 0, bytes.length);
            parcel.setDataPosition(0);
            return Bundle.CREATOR.createFromParcel(parcel);
        } finally {
            parcel.recycle();
        }
    }

    private static int indexOf(byte[] outer, byte[] inner) {
        for (int i = 0; i <= outer.length - inner.length; i++) {
            boolean found = true;
            for (int j = 0; j < inner.length; j++) {
                if (outer[i + j] != inner[j]) {
                    found = false;
                    break;
                }
            }
            if (found) return i;
        }
        return -1;
    }
}
