// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chromecast.base;

import android.app.Service;
import android.content.ComponentName;
import android.content.Context;
import android.content.Intent;
import android.content.ServiceConnection;
import android.os.Binder;
import android.os.IBinder;

/**
 * Base class for services that are only bound to by other clients in the same application, running
 * in the same process.
 *
 * <p>Implementing this gives a way to establish a many-to-one relationship between components that
 * "share" some resource. The lifetime of the resource itself can be managed by the implementation's
 * bind() method, cleaning up the resource when the last observer unsubscribes.
 *
 * <p>Services that extend this class must NOT be exported (android:exported="false" in the
 * manifest).
 *
 * @param <T> Data type to observe.
 */
public abstract class LocalService<T> extends Service {
    private static class BinderObservable<T> extends Binder {
        private final Observable<T> mSrc;

        BinderObservable(Observable<T> src) {
            mSrc = src;
        }

        static <T> Observable<T> from(IBinder binder) {
            return ((BinderObservable<T>) binder).mSrc;
        }
    }

    /**
     * Returns an Observable that is notified when the service is bound, and is closed when the
     * service is unbound.
     *
     * <p>This method is called when the service is bound, and the Observable returned is subscribed
     * to by the local clients that are bound to the service.
     *
     * <p>The Observable returned by this method will be unsubscribed when the service is unbound.
     *
     * <p>If multiple clients bind with identical Intents, this method will only be called once. If
     * multiple clients bind with different Intents, this method will be called once for each unique
     * Intent, with a different Observable returned each time.
     */
    protected abstract Observable<T> bind(Intent intent);

    private static Observable<IBinder> connect(Context context, Intent intent) {
        return observer -> {
            OwnedScope scope = new OwnedScope();
            ServiceConnection connection =
                    new ServiceConnection() {
                        @Override
                        public void onServiceConnected(ComponentName name, IBinder service) {
                            scope.set(observer.open(service));
                        }

                        @Override
                        public void onServiceDisconnected(ComponentName name) {
                            scope.close();
                        }
                    };
            context.bindService(intent, connection, Context.BIND_AUTO_CREATE);
            return scope.and(() -> context.unbindService(connection));
        };
    }

    /**
     * Each implementation of LocalService should create a public static connect() method that takes
     * the input arguments for the service and returns an Observable<T> that serves as a proxy to
     * the Observable<T> returned by the service's bind() method.
     *
     * <p>This static method helps LocalService implementations implement such methods by doing the
     * heavy lifting of adapting the result of bind() to a proxy Observable<T> and making it
     * available to the client.
     *
     * <p>Example:
     *
     * <pre>{@code
     * public class FooService extends LocalService<Foo> {
     *   @Override
     *   protected Observable<Foo> bind(Intent intent) {
     *     return ...;
     *   }
     *
     *   public static SharedObservable<Foo> connect(Context context) {
     *     return LocalService.connect(Foo.class, context, new Intent(context, FooService.class));
     *   }
     * }
     * }</pre>
     */
    protected static <T> SharedObservable<T> connect(
            Class<T> clazz, Context context, Intent intent) {
        return connect(context, intent).flatMap(BinderObservable::<T>from).share();
    }

    @Override
    public final IBinder onBind(Intent intent) {
        return new BinderObservable(bind(intent));
    }

    @Override
    public final boolean onUnbind(Intent intent) {
        return false;
    }
}
