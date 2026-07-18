// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chromecast.shell;

import androidx.activity.OnBackPressedCallback;
import androidx.activity.OnBackPressedDispatcher;
import androidx.lifecycle.LifecycleOwner;

import org.chromium.chromecast.base.Controller;
import org.chromium.chromecast.base.Observable;
import org.chromium.chromecast.base.Scope;
import org.chromium.chromecast.base.SharedObservable;
import org.chromium.chromecast.base.Unit;

/**
 * An adapter that exposes an OnBackPressedDispatcher as an Observable of back press events.
 *
 * <p>The OnBackPressedCallback registered with the OnBackPressedDispatcher by this adapter is only
 * enabled when there are active observers of the Observable returned from
 * observeBackPressedEvents(). If an observer is responsible for handling the back press event, but
 * in the course of doing so decides the system should handle it instead, it should use the
 * fallbackToDefaultBackPressHandler() method, which will disable the callback that activates the
 * Observable and re-dispatch the event to the OnBackPressedDispatcher, which will invoke the
 * next-highest-priority callback (or the system default).
 *
 * <p>If you create multiple BackPressCallbackAdapters on a single OnBackPressedDispatcher, the last
 * one added that has active subscriptions will be the one to actually receive back press events.
 */
public class BackPressCallbackAdapter {
    private final OnBackPressedDispatcher mDispatcher;
    private final OnBackPressedCallback mCallback;
    private final SharedObservable<Unit> mSharedBackPressEvents;

    public static BackPressCallbackAdapter create(
            LifecycleOwner owner, OnBackPressedDispatcher dispatcher) {
        Controller<Unit> backPressedEvents = new Controller<>();
        var callback =
                new OnBackPressedCallback(/* enabled= */ false) {
                    @Override
                    public void handleOnBackPressed() {
                        backPressedEvents.reset();
                        backPressedEvents.set(Unit.unit());
                    }
                };
        Observable<Unit> rawBackPressEvents =
                observer -> {
                    callback.setEnabled(true);
                    Scope cleanup = () -> callback.setEnabled(false);
                    return cleanup.and(backPressedEvents.subscribe(observer));
                };
        SharedObservable<Unit> sharedBackPressEvents = SharedObservable.from(rawBackPressEvents);
        dispatcher.addCallback(owner, callback);
        return new BackPressCallbackAdapter(dispatcher, callback, sharedBackPressEvents);
    }

    private BackPressCallbackAdapter(
            OnBackPressedDispatcher dispatcher,
            OnBackPressedCallback callback,
            SharedObservable<Unit> sharedBackPressEvents) {
        mDispatcher = dispatcher;
        mCallback = callback;
        mSharedBackPressEvents = sharedBackPressEvents;
    }

    public void fallbackToDefaultBackPressHandler() {
        if (mCallback.isEnabled()) {
            mCallback.setEnabled(false);
            mDispatcher.onBackPressed();
            mCallback.setEnabled(true);
        } else {
            mDispatcher.onBackPressed();
        }
    }

    public SharedObservable<Unit> observeBackPressedEvents() {
        return mSharedBackPressEvents;
    }
}
