// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chromecast.base;

import static org.robolectric.Shadows.shadowOf;

import static org.chromium.chromecast.base.Observable.just;

import android.app.Application;
import android.content.ComponentName;
import android.content.Context;
import android.content.Intent;
import android.os.IBinder;

import androidx.test.core.app.ApplicationProvider;

import org.junit.Test;
import org.junit.runner.RunWith;
import org.robolectric.Robolectric;
import org.robolectric.android.controller.ServiceController;
import org.robolectric.shadows.ShadowApplication;

import org.chromium.base.test.BaseRobolectricTestRunner;

@RunWith(BaseRobolectricTestRunner.class)
public final class LocalServiceTest {
    private static final class Foo {}

    private static final class FooService extends LocalService<Foo> {
        Observable<Foo> mImpl;

        public static Observable<Foo> connect(Context context) {
            return LocalService.connect(Foo.class, context, new Intent(context, FooService.class));
        }

        @Override
        protected Observable<Foo> bind(Intent intent) {
            return mImpl;
        }
    }

    @Test
    public void implReturnedImmediately() {
        Foo foo = new Foo();
        Controller<Foo> fooController = new Controller<>();
        fooController.set(foo);

        Application context = ApplicationProvider.getApplicationContext();
        ShadowApplication app = shadowOf(context);
        app.setBindServiceCallsOnServiceConnectedDirectly(false);
        ComponentName componentName = new ComponentName(context, FooService.class);
        ServiceController<FooService> fooService = Robolectric.buildService(FooService.class);
        fooService.get().mImpl = fooController;
        var fooConnection = FooService.connect(fooService.get());
        ReactiveRecorder recorder = ReactiveRecorder.record(fooConnection);
        IBinder binder = fooService.get().onBind(app.getNextStartedService());
        app.getBoundServiceConnections().get(0).onServiceConnected(componentName, binder);

        recorder.verify().opened(foo).end();
    }

    @Test
    public void implReturnedAsynchronouslyAfterBind() {
        Foo foo = new Foo();
        Controller<Foo> fooController = new Controller<>();

        Application context = ApplicationProvider.getApplicationContext();
        ShadowApplication app = shadowOf(context);
        app.setBindServiceCallsOnServiceConnectedDirectly(false);
        ComponentName componentName = new ComponentName(context, FooService.class);
        ServiceController<FooService> fooService = Robolectric.buildService(FooService.class);
        fooService.get().mImpl = fooController;
        var fooConnection = FooService.connect(fooService.get());
        ReactiveRecorder recorder = ReactiveRecorder.record(fooConnection);
        IBinder binder = fooService.get().onBind(app.getNextStartedService());
        app.getBoundServiceConnections().get(0).onServiceConnected(componentName, binder);

        recorder.verify().end();
        fooController.set(foo);
        recorder.verify().opened(foo).end();
    }

    @Test
    public void revokeImplIfServiceDisconnected() {
        Foo foo = new Foo();
        Controller<Foo> fooController = new Controller<>();
        fooController.set(foo);

        Application context = ApplicationProvider.getApplicationContext();
        ShadowApplication app = shadowOf(context);
        app.setBindServiceCallsOnServiceConnectedDirectly(false);
        ComponentName componentName = new ComponentName(context, FooService.class);
        ServiceController<FooService> fooService = Robolectric.buildService(FooService.class);
        fooService.get().mImpl = fooController;
        var fooConnection = FooService.connect(fooService.get());
        ReactiveRecorder recorder = ReactiveRecorder.record(fooConnection);
        IBinder binder = fooService.get().onBind(app.getNextStartedService());
        app.getBoundServiceConnections().get(0).onServiceConnected(componentName, binder);

        recorder.verify().opened(foo).end();

        app.getBoundServiceConnections().get(0).onServiceDisconnected(componentName);
        recorder.verify().closed(foo).end();
    }

    @Test
    public void revokeImplIfServiceRevoked() {
        Foo foo = new Foo();
        Controller<Foo> fooController = new Controller<>();
        fooController.set(foo);

        Application context = ApplicationProvider.getApplicationContext();
        ShadowApplication app = shadowOf(context);
        app.setBindServiceCallsOnServiceConnectedDirectly(false);
        ComponentName componentName = new ComponentName(context, FooService.class);
        ServiceController<FooService> fooService = Robolectric.buildService(FooService.class);
        fooService.get().mImpl = fooController;
        var fooConnection = FooService.connect(fooService.get());
        ReactiveRecorder recorder = ReactiveRecorder.record(fooConnection);
        IBinder binder = fooService.get().onBind(app.getNextStartedService());
        app.getBoundServiceConnections().get(0).onServiceConnected(componentName, binder);

        recorder.verify().opened(foo).end();

        fooController.reset();
        recorder.verify().closed(foo).end();
    }

    @Test
    public void multipleData() {
        Foo a = new Foo();
        Foo b = new Foo();
        Foo c = new Foo();
        Observable<Foo> foos = just(a).or(just(b)).or(just(c));

        Application context = ApplicationProvider.getApplicationContext();
        ShadowApplication app = shadowOf(context);
        app.setBindServiceCallsOnServiceConnectedDirectly(false);
        ComponentName componentName = new ComponentName(context, FooService.class);
        ServiceController<FooService> fooService = Robolectric.buildService(FooService.class);
        fooService.get().mImpl = foos;
        var fooConnection = FooService.connect(fooService.get());
        ReactiveRecorder recorder = ReactiveRecorder.record(fooConnection);
        IBinder binder = fooService.get().onBind(app.getNextStartedService());
        app.getBoundServiceConnections().get(0).onServiceConnected(componentName, binder);

        recorder.verify().opened(a).opened(b).opened(c).end();

        app.getBoundServiceConnections().get(0).onServiceDisconnected(componentName);

        recorder.verify().closed(c).closed(b).closed(a).end();
    }

    @Test
    public void multipleObservers() {
        Foo foo = new Foo();
        Controller<Foo> fooController = new Controller<>();
        fooController.set(foo);

        Application context = ApplicationProvider.getApplicationContext();
        ShadowApplication app = shadowOf(context);
        app.setBindServiceCallsOnServiceConnectedDirectly(false);
        ComponentName componentName = new ComponentName(context, FooService.class);
        ServiceController<FooService> fooService = Robolectric.buildService(FooService.class);
        fooService.get().mImpl = fooController;
        var fooConnection = FooService.connect(fooService.get());
        ReactiveRecorder recorder1 = ReactiveRecorder.record(fooConnection);
        ReactiveRecorder recorder2 = ReactiveRecorder.record(fooConnection);
        IBinder binder = fooService.get().onBind(app.getNextStartedService());
        app.getBoundServiceConnections().get(0).onServiceConnected(componentName, binder);

        recorder1.verify().opened(foo).end();
        recorder2.verify().opened(foo).end();

        // Unsubscribe one recorder but not the other.
        recorder1.unsubscribe();
        recorder1.verify().closed(foo).end();
        recorder2.verify().end();

        // Disconnect the service. This should only notify the remaining observer.
        app.getBoundServiceConnections().get(0).onServiceDisconnected(componentName);
        recorder1.verify().end();
        recorder2.verify().closed(foo).end();
    }
}
