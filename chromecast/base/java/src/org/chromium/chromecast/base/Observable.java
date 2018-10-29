// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chromecast.base;

/**
 * Interface for Observable state.
 *
 * Observables can have some data associated with them, which is provided to observers when the
 * Observable activates.
 *
 * @param <T> The type of the activation data.
 */
public abstract class Observable<T> {
    /**
     * Tracks this Observable with the given observer.
     *
     * When this Observable is activated, the observer will be opened with the activation data to
     * produce a scope. When this Observable is deactivated, that scope will be closed.
     *
     * When the returned Subscription is closed, the observer's scopes will be closed and the
     * observer will no longer be notified of updates.
     */
    public abstract Subscription subscribe(Observer<? super T> observer);

    /**
     * Creates an Observable that opens observers's scopes only if both `this` and `other` are
     * activated, and closes those scopes if either of `this` or `other` are deactivated.
     *
     * This is useful for creating an event handler that should only activate when two events
     * have occurred, but those events may occur in any order.
     */
    public final <U> Observable<Both<T, U>> and(Observable<U> other) {
        Controller<Both<T, U>> controller = new Controller<>();
        subscribe(t -> other.subscribe(u -> {
            controller.set(Both.both(t, u));
            return controller::reset;
        }));
        return controller;
    }

    /**
     * Returns an Observable that is activated when `this` and `other` are activated in order.
     *
     * This is similar to `and()`, but does not activate if `other` is activated before `this`.
     */
    public final <U> Observable<Both<T, U>> andThen(Observable<U> other) {
        Controller<U> otherAfterThis = new Controller<>();
        other.subscribe((U value) -> {
            otherAfterThis.set(value);
            return otherAfterThis::reset;
        });
        subscribe(Observers.onEnter(x -> otherAfterThis.reset()));
        return and(otherAfterThis);
    }

    /**
     * Returns an Observable that applies the given Function to this Observable's activation values.
     */
    public final <R> Observable<R> map(Function<? super T, ? extends R> transform) {
        Controller<R> controller = new Controller<>();
        subscribe((T value) -> {
            controller.set(transform.apply(value));
            return controller::reset;
        });
        return controller;
    }

    /**
     * Returns an Observable that is only activated when `this` is activated with a value such that
     * the given `predicate` returns true.
     */
    public final Observable<T> filter(Predicate<? super T> predicate) {
        Controller<T> controller = new Controller<>();
        subscribe((T value) -> {
            if (predicate.test(value)) {
                controller.set(value);
            }
            return controller::reset;
        });
        return controller;
    }

    /**
     * Returns an Observable that is activated only when the given Observable is not activated.
     */
    public static Observable<?> not(Observable<?> observable) {
        Controller<Unit> opposite = new Controller<>();
        opposite.set(Unit.unit());
        observable.subscribe(x -> {
            opposite.reset();
            return () -> opposite.set(Unit.unit());
        });
        return opposite;
    }
}
