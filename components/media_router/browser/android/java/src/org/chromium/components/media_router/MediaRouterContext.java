// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.components.media_router;

import android.content.ComponentName;
import android.content.Context;
import android.content.ContextWrapper;
import android.content.Intent;
import android.content.ServiceConnection;
import android.os.Bundle;
import android.os.Handler;
import android.os.IBinder;
import android.os.Looper;
import android.os.Message;
import android.os.Messenger;
import android.os.RemoteException;

import androidx.annotation.VisibleForTesting;
import androidx.mediarouter.media.MediaRouteDescriptor;

import org.chromium.base.Log;
import org.chromium.build.annotations.NullMarked;
import org.chromium.build.annotations.Nullable;

import java.util.ArrayList;
import java.util.HashMap;
import java.util.List;
import java.util.Map;

/**
 * The {@link Context} given to the AndroidX {@link androidx.mediarouter.media.MediaRouter}.
 *
 * <p>MediaRouter binds to every MediaRouteProviderService on the device, including ones in
 * third-party apps. Those apps send route descriptors as Bundles, which Chrome unparcels on the
 * main thread. If a descriptor holds a Parcelable class that only exists in the other app (for
 * example a custom IntentFilter subclass), reading it throws BadParcelableException. MediaRouter
 * doesn't catch it, so the browser crashes. See crbug.com/580092988.
 *
 * <p>This context puts a proxy between MediaRouter and each provider service. The proxy reads the
 * route fields that MediaRouter reads before MediaRouter does, and drops the routes that fail to
 * unparcel.
 */
@NullMarked
class MediaRouterContext extends ContextWrapper {
    private static final String TAG = "MediaRouter";

    // These values come from androidx.mediarouter.media.MediaRouteProviderProtocol, which is not
    // public.
    @VisibleForTesting
    static final String PROVIDER_SERVICE_INTERFACE = "android.media.MediaRouteProviderService";

    @VisibleForTesting static final int SERVICE_MSG_REGISTERED = 2;
    @VisibleForTesting static final int SERVICE_MSG_GENERIC_FAILURE = 0;
    private static final int SERVICE_MSG_DESCRIPTOR_CHANGED = 5;
    @VisibleForTesting static final int SERVICE_MSG_DYNAMIC_ROUTE_CREATED = 6;
    @VisibleForTesting static final int SERVICE_MSG_DYNAMIC_ROUTE_DESCRIPTORS_CHANGED = 7;
    private static final String PROVIDER_DESCRIPTOR_KEY_ROUTES = "routes";
    private static final String DATA_KEY_GROUP_ROUTE_DESCRIPTOR = "groupRoute";
    private static final String DATA_KEY_DYNAMIC_ROUTE_DESCRIPTORS = "dynamicRoutes";
    private static final String DYNAMIC_ROUTE_KEY_MEDIA_ROUTE_DESCRIPTOR = "mrDescriptor";

    private final Map<ServiceConnection, ProviderConnection> mConnections = new HashMap<>();

    MediaRouterContext(Context base) {
        super(base);
    }

    /**
     * MediaRouter keeps the result of this method and uses it for bindService(). Return this object
     * so that the proxy stays in place.
     */
    @Override
    public Context getApplicationContext() {
        return this;
    }

    @Override
    public boolean bindService(Intent service, ServiceConnection conn, int flags) {
        if (!PROVIDER_SERVICE_INTERFACE.equals(service.getAction())) {
            return super.bindService(service, conn, flags);
        }
        ProviderConnection proxy = mConnections.get(conn);
        if (proxy == null) {
            proxy = new ProviderConnection(conn);
            mConnections.put(conn, proxy);
        }
        boolean bound = false;
        try {
            bound = super.bindService(service, proxy, flags);
        } finally {
            // MediaRouter only calls unbindService() after a successful bind.
            if (!bound) mConnections.remove(conn);
        }
        return bound;
    }

    @Override
    public void unbindService(ServiceConnection conn) {
        ProviderConnection proxy = mConnections.remove(conn);
        super.unbindService(proxy != null ? proxy : conn);
    }

    /**
     * Hands MediaRouter a proxy messenger instead of the provider's own messenger.
     *
     * <p>MediaRouter calls linkToDeath() on the binder it gets, which does nothing on the local
     * proxy binder. That is fine, because Android also calls onServiceDisconnected() when the
     * provider process dies, and MediaRouter disconnects the provider in both cases.
     */
    private static class ProviderConnection implements ServiceConnection {
        private final ServiceConnection mTarget;

        ProviderConnection(ServiceConnection target) {
            mTarget = target;
        }

        @Override
        public void onServiceConnected(ComponentName name, @Nullable IBinder service) {
            // MediaRouter treats a null binder as an invalid service, so pass it through.
            IBinder binder =
                    service == null
                            ? null
                            : new ServiceMessengerProxy(new Messenger(service)).getBinder();
            mTarget.onServiceConnected(name, binder);
        }

        @Override
        public void onServiceDisconnected(ComponentName name) {
            mTarget.onServiceDisconnected(name);
        }

        @Override
        public void onBindingDied(ComponentName name) {
            mTarget.onBindingDied(name);
        }

        @Override
        public void onNullBinding(ComponentName name) {
            mTarget.onNullBinding(name);
        }
    }

    /**
     * Forwards messages from MediaRouter to the provider service, and swaps each message's replyTo
     * so that replies come back through {@link #sanitizeServiceMessage}.
     */
    private static class ServiceMessengerProxy {
        private final Messenger mService;
        private final Messenger mProxy;
        private @Nullable Messenger mClient;
        private @Nullable Messenger mClientProxy;

        ServiceMessengerProxy(Messenger service) {
            mService = service;
            mProxy = createMainMessenger(this::forwardToService);
        }

        IBinder getBinder() {
            return mProxy.getBinder();
        }

        private boolean forwardToService(Message msg) {
            Message copy = Message.obtain(msg);
            if (msg.replyTo != null) {
                copy.replyTo = getClientProxy(msg.replyTo);
            }
            try {
                mService.send(copy);
            } catch (RemoteException e) {
                // The service died. MediaRouter will get onServiceDisconnected().
                Log.w(TAG, "Failed to send message to media route provider.", e);
            }
            return true;
        }

        private Messenger getClientProxy(Messenger client) {
            if (!client.equals(mClient) || mClientProxy == null) {
                mClient = client;
                mClientProxy = createMainMessenger(msg -> forwardToClient(client, msg));
            }
            return mClientProxy;
        }

        private static boolean forwardToClient(Messenger client, Message msg) {
            Message copy = Message.obtain(msg);
            if (!sanitizeServiceMessage(copy)) {
                copy.recycle();
                return true;
            }
            try {
                client.send(copy);
            } catch (RemoteException e) {
                // The client is in this process, so this should not happen.
                Log.w(TAG, "Failed to forward message from media route provider.", e);
            }
            return true;
        }

        private static Messenger createMainMessenger(Handler.Callback callback) {
            return new Messenger(new Handler(Looper.getMainLooper(), callback));
        }
    }

    /**
     * Makes a message from a provider service safe for MediaRouter to read, by removing the routes
     * that MediaRouter can't unparcel.
     *
     * @return whether the message should be forwarded to MediaRouter.
     */
    @VisibleForTesting
    static boolean sanitizeServiceMessage(Message msg) {
        if (!(msg.obj instanceof Bundle)) return true;
        Bundle bundle = (Bundle) msg.obj;
        switch (msg.what) {
            case SERVICE_MSG_REGISTERED:
            case SERVICE_MSG_DESCRIPTOR_CHANGED:
                msg.obj = sanitizeProviderDescriptor(bundle);
                return true;
            case SERVICE_MSG_DYNAMIC_ROUTE_CREATED:
            case SERVICE_MSG_DYNAMIC_ROUTE_DESCRIPTORS_CHANGED:
                Bundle sanitized = sanitizeDynamicRoutes(bundle);
                if (sanitized != null) {
                    msg.obj = sanitized;
                    return true;
                }
                Log.w(TAG, "Unreadable group route from media route provider.");
                if (msg.what == SERVICE_MSG_DYNAMIC_ROUTE_DESCRIPTORS_CHANGED) {
                    // This is an update, not a reply, so nothing waits for it.
                    return false;
                }
                // Fail the request instead, so that MediaRouter doesn't wait forever.
                msg.what = SERVICE_MSG_GENERIC_FAILURE;
                msg.obj = null;
                msg.setData(null);
                return true;
            default:
                return true;
        }
    }

    /** Returns a copy of {@code descriptor} without the routes that can't be unparceled. */
    @VisibleForTesting
    static Bundle sanitizeProviderDescriptor(Bundle descriptor) {
        try {
            Bundle result = new Bundle(descriptor);
            List<?> routes = (List<?>) getValue(result, PROVIDER_DESCRIPTOR_KEY_ROUTES);
            if (routes != null) {
                ArrayList<Bundle> readableRoutes = new ArrayList<>();
                for (Object route : routes) {
                    if (route instanceof Bundle && isRouteReadable((Bundle) route)) {
                        readableRoutes.add((Bundle) route);
                    }
                }
                result.putParcelableArrayList(PROVIDER_DESCRIPTOR_KEY_ROUTES, readableRoutes);
            }
            return result;
        } catch (RuntimeException e) {
            // The descriptor itself can't be read. Report a provider with no routes.
            Log.w(TAG, "Dropped unreadable media route provider descriptor.", e);
            return new Bundle();
        }
    }

    /**
     * Returns a copy of a dynamic group message without the member routes that can't be unparceled,
     * or null if the group route itself can't be unparceled.
     */
    private static @Nullable Bundle sanitizeDynamicRoutes(Bundle bundle) {
        try {
            Bundle result = new Bundle(bundle);
            Bundle groupRoute = result.getBundle(DATA_KEY_GROUP_ROUTE_DESCRIPTOR);
            if (groupRoute != null && !isRouteReadable(groupRoute)) return null;
            List<?> dynamicRoutes = (List<?>) getValue(result, DATA_KEY_DYNAMIC_ROUTE_DESCRIPTORS);
            if (dynamicRoutes != null) {
                ArrayList<Bundle> readableRoutes = new ArrayList<>();
                for (Object dynamicRoute : dynamicRoutes) {
                    if (dynamicRoute instanceof Bundle
                            && isDynamicRouteReadable((Bundle) dynamicRoute)) {
                        readableRoutes.add((Bundle) dynamicRoute);
                    }
                }
                result.putParcelableArrayList(DATA_KEY_DYNAMIC_ROUTE_DESCRIPTORS, readableRoutes);
            }
            return result;
        } catch (RuntimeException e) {
            return null;
        }
    }

    private static boolean isDynamicRouteReadable(Bundle dynamicRoute) {
        try {
            Bundle route = dynamicRoute.getBundle(DYNAMIC_ROUTE_KEY_MEDIA_ROUTE_DESCRIPTOR);
            return route == null || isRouteReadable(route);
        } catch (RuntimeException e) {
            return false;
        }
    }

    /**
     * Returns whether MediaRouter can read {@code route}. Only reads the fields that MediaRouter
     * reads. Other fields, such as "extras", stay packed. They can hold classes that the boot class
     * loader can't load, like CastDevice, which their readers unparcel with their own class loader.
     */
    private static boolean isRouteReadable(Bundle route) {
        try {
            MediaRouteDescriptor descriptor = MediaRouteDescriptor.fromBundle(route);
            if (descriptor == null) return true;
            // isValid() reads the control filters. Invalid routes are left for MediaRouter to
            // reject, as it did before.
            descriptor.isValid();
            descriptor.getSettingsActivity();
            return true;
        } catch (RuntimeException e) {
            // Usually BadParcelableException, but Parcel can throw other RuntimeExceptions for
            // bad data.
            Log.w(TAG, "Dropped unreadable route from media route provider.");
            return false;
        }
    }

    @SuppressWarnings("deprecation") // Bundle.get() is the only untyped getter.
    private static @Nullable Object getValue(Bundle bundle, String key) {
        return bundle.get(key);
    }
}
