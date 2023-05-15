// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chromecast.base;

/**
 * A Scope that encapsulates up to one other Scope at a time.
 *
 * If the set() method is used to assign an owned Scope, any previous Scope assigned to this
 * OwnedScope is closed.
 *
 * This can be useful for turning APIs with start/stop, or connected/disconnected method pairs into
 * safely implementing Observables.
 *
 * For example, you can easily implement an API that encapsulates a ServiceConnection in an
 * Observable, which notifies subscribers when the connection is established and closes the scope
 * safely when the connection is ended.
 *
 *   // A static method on a Service implementation like this can provide an easy connection API.
 *   public static Observable<IFoo> connect(Context context) {
 *       return observer -> {
 *           // Use an OwnedScope to avoid a null check in onServiceDisconnected().
 *           OwnedScope subscription = new OwnedScope();
 *           ServiceConnection connection = new ServiceConnection() {
 *               @Override
 *               public void onServiceConnected(ComponentName componentName, IBinder binder) {
 *                   // If, for some reason, onServiceConnected() is called twice, the new
 *                   // connection overrides the old one and the observer is notified correctly.
 *                   subscription.set(observer.open((IFoo) binder));
 *               }
 *               @Override
 *               public void onServiceDisconnected(ComponentName componentName) {
 *                   // This is safe no matter how many times onServiceConnected() was invoked.
 *                   subscription.close();
 *               }
 *           };
 *           context.bindService(new Intent(context, FooService.class), connection);
 *           return subscription.and(() -> context.unbindService(connection));
 *       };
 *   }
 *
 * An OwnedScope can also be useful for cleanup in clients for such APIs:
 *
 *   class FooClient extends Scope {
 *       private final OwnedScope mConnection = new OwnedScope;
 *       Foo mFoo;
 *
 *       public void establishConnection() {
 *           mConnection.set(FooService.connect(context).subscribe((IFoo foo) {
 *               mFoo = foo;
 *               return () -> mFoo = null;
 *           }));
 *       }
 *
 *       @Override
 *       public void close() {
 *           mConnection.close();
 *       }
 *   }
 */
public class OwnedScope implements Scope {
    private Scope mScope;

    public OwnedScope() {}

    public OwnedScope(Scope scope) {
        mScope = scope;
    }

    public void set(Scope scope) {
        if (mScope != null && scope != mScope) mScope.close();
        mScope = scope;
    }

    @Override
    public void close() {
        if (mScope == null) return;
        mScope.close();
        mScope = null;
    }
}
