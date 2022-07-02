// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chromecast.base;

import org.chromium.base.Consumer;
import org.chromium.base.Function;

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
        return flatMap(t -> other.flatMap(u -> just(Both.both(t, u))));
    }

    /**
     * Returns an Observable that is only activated with the values added after subscription.
     *
     * Some Observables synchronously notify observers of their current state the moment they are
     * subscribed. This operator filters these out and only notifies the observer of changes that
     * occur after the moment of subscription.
     */
    public final Observable<T> after() {
        return make(observer -> {
            Box<Boolean> after = new Box<Boolean>(false);
            Subscription sub = subscribe(t -> after.value ? observer.open(t) : Scopes.NO_OP);
            after.value = true;
            return sub;
        });
    }

    /**
     * Returns an Observable that is activated when `this` and `other` are activated in order.
     *
     * This is similar to `and()`, but does not activate if `other` is activated before `this`.
     */
    public final <U> Observable<Both<T, U>> andThen(Observable<U> other) {
        return and(other.after());
    }

    /**
     * Returns an Observable that applies the given Function to this Observable's activation values.
     */
    public final <R> Observable<R> map(Function<? super T, ? extends R> f) {
        return flatMap(t -> just(f.apply(t)));
    }

    /**
     * Returns an Observable that applies the given Function to this Observable's activation data,
     * and notifies observers with the activation data of the Observable returned by the Function.
     *
     * If you have a function that returns an Observable, you can use this to avoid using map() to
     * create an Observable of Observables.
     *
     * This is an extremely powerful operation! Observables are a monad where flatMap() is the
     * "bind" operation that combines an Observable with a function that returns an Observable to
     * create a new Observable.
     *
     * One use case could be using Observables as "promises" and using flatMap() with "async
     * functions" that return Observables to create asynchronous programs:
     *
     *   Observable<Foo> getFooAsync();
     *   Observable<Bar> getBarAsync(Foo foo);
     *   Scope useBar(Bar bar);
     *
     *   getFooAsync().flatMap(foo -> getBarAsync(foo)).subscribe(bar -> useBar(bar));
     */
    public final <R> Observable<R> flatMap(
            Function<? super T, ? extends Observable<? extends R>> f) {
        return make(observer -> subscribe(t -> f.apply(t).subscribe(r -> observer.open(r))));
    }

    /**
     * Returns an Observable that is only activated when `this` is activated with a value such that
     * the given `predicate` returns true.
     */
    public final Observable<T> filter(Predicate<? super T> predicate) {
        return flatMap(t -> predicate.test(t) ? just(t) : empty());
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

    /**
     * Allows creating an Observable with a functional interface.
     */
    public static <T> Observable<T> make(
            Function<? super Observer<? super T>, ? extends Scope> impl) {
        return new Observable<T>() {
            @Override
            public Subscription subscribe(Observer<? super T> observer) {
                return impl.apply(observer)::close;
            }
        };
    }

    /**
     * A degenerate Observable that has no data.
     */
    public static <T> Observable<T> empty() {
        return make(observer -> Scopes.NO_OP);
    }

    /**
     * Creates an Observable that notifies subscribers with a single immutable value.
     *
     * This is the "return" operation in the Observable monad.
     */
    public static <T> Observable<T> just(T value) {
        if (value == null) return empty();
        return make(observer -> observer.open(value));
    }

    /**
     * Push debug info about subscriptions and state transitions to a logger.
     *
     * The logger is a consumer of strings. Typically, the consumer should be a lambda that prints
     * the input using org.chromium.base.Log with any extra info you want to include. See
     * chromium/base/reactive_java.md for an example.
     *
     * By passing a Consumer instead of having debug() call logger methods directly, you can 1)
     * control the logging level, or use alternative loggers, and 2) when using chromium's logger,
     * see the right file name and line number in the logs.
     */
    public Observable<T> debug(Consumer<String> logger) {
        return make(observer -> {
            logger.accept("subscribe");
            Scope subscription = subscribe(data -> {
                logger.accept(new StringBuilder("open ").append(data).toString());
                Scope scope = observer.open(data);
                Scope debugClose =
                        () -> logger.accept(new StringBuilder("close ").append(data).toString());
                return Scopes.combine(scope, debugClose);
            })::close;
            Scope debugUnsubscribe = () -> logger.accept("unsubscribe");
            return Scopes.combine(subscription, debugUnsubscribe);
        });
    }

    /**
     * An abstraction over posting a delayed task. Implementations should run the posted Runnable
     * after a certain amount of time has elapsed, preferably on the same thread that posted the
     * Runnable.
     */
    public interface Scheduler {
        void postDelayed(Runnable runnable, long delay);
    }

    /**
     * Return an Observable that activates a given amount of time (in milliseconds) after it is
     * subscribed to.
     *
     * Note that the alarm countdown starts when subscribed, not when the Observable is constructed.
     * Therefore, if there are multiple observers, each will have its own timer.
     *
     * If you use an alarm as the argument to an `.and()` operator, for example, a unique timer will
     * start for each activation of the left-hand side of the `.and()` call.
     *
     * The Scheduler is responsible for executing a Runnable after the given delay has elapsed,
     * preferably on the same thread that invoked its postDelayed() method, unless the observers of
     * this Observable are thread-safe.
     */
    public static Observable<?> alarm(Scheduler scheduler, long ms) {
        return make(observer -> {
            Controller<Unit> activation = new Controller<>();
            scheduler.postDelayed(() -> activation.set(Unit.unit()), ms);
            return activation.subscribe(observer);
        });
    }

    /**
     * An Observable that delays any activations by the given amount of time.
     *
     * If the activation is revoked before the given amount of time elapses, the activation is
     * effectively "canceled" from the perspective of observers.
     *
     * The Scheduler is responsible for executing a Runnable after the given delay has elapsed,
     * preferably on the same thread that invoked its postDelayed() method, unless the observers of
     * this Observable are thread-safe.
     */
    public final Observable<T> delay(Scheduler scheduler, long ms) {
        return flatMap(t -> alarm(scheduler, ms).map(x -> t));
    }
}
