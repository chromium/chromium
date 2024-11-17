// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chromecast.base;

import java.util.function.BiFunction;
import java.util.function.Consumer;
import java.util.function.Function;
import java.util.function.Predicate;

import java.util.function.Supplier;

/**
 * Interface for Observable state.
 *
 * Observables can have some data associated with them, which is provided to observers when the
 * Observable activates.
 *
 * @param <T> The type of the activation data.
 */
@FunctionalInterface
public interface Observable<T> {
    /**
     * Tracks this Observable with the given observer.
     *
     * When this Observable is activated, the observer will be opened with the activation data to
     * produce a scope. When this Observable is deactivated, that scope will be closed.
     *
     * When the returned Scope (referred to as a "subscription") is closed, the observer's scopes
     * will be closed and the observer will no longer be notified of updates.
     */
    Scope subscribe(Observer<? super T> observer);

    /**
     * Creates an Observable that opens observers's scopes only if both `this` and `other` are
     * activated, and closes those scopes if either of `this` or `other` are deactivated.
     *
     * This is useful for creating an event handler that should only activate when two events
     * have occurred, but those events may occur in any order.
     */
    default <U> Observable<Both<T, U>> and(Observable<U> other) {
        return flatMap(t -> other.flatMap(u -> just(Both.both(t, u))));
    }

    /**
     * Creates an Observable out of the activations of `this` and `other`. An observer that is
     * subscribed to the result will be subscribed to both `this` and `other` at the same time.
     *
     * This combines Observables in the same way that concatenation combines lists. This operation
     * forms a monoid over Observables with empty() as the identity element. I.e. for all
     * Observables `x`:
     *
     *   x.or(empty()) == x
     *   empty().or(x) == x
     */
    default Observable<T> or(Observable<T> other) {
        return observer -> subscribe(observer).and(other.subscribe(observer));
    }

    /**
     * Returns an Observable that is only activated with the values added after subscription.
     *
     * Some Observables synchronously notify observers of their current state the moment they are
     * subscribed. This operator filters these out and only notifies the observer of changes that
     * occur after the moment of subscription.
     */
    default Observable<T> after() {
        return observer -> {
            Box<Boolean> after = new Box<Boolean>(false);
            Scope sub = subscribe(t -> after.value ? observer.open(t) : Scope.NO_OP);
            after.value = true;
            return sub;
        };
    }

    /**
     * Returns an Observable that is activated when `this` and `other` are activated in order.
     *
     * This is similar to `and()`, but does not activate if `other` is activated before `this`.
     */
    default <U> Observable<Both<T, U>> andThen(Observable<U> other) {
        return and(other.after());
    }

    /**
     * Returns an Observable that applies the given Function to this Observable's activation values.
     */
    default <R> Observable<R> map(Function<? super T, ? extends R> f) {
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
    default <R> Observable<R> flatMap(
            Function<? super T, ? extends Observable<? extends R>> f) {
        return observer -> subscribe(t -> f.apply(t).subscribe(r -> observer.open(r)));
    }

    /**
     * Returns an Observable that is only activated when `this` is activated with a value such that
     * the given `predicate` returns true.
     */
    default Observable<T> filter(Predicate<? super T> predicate) {
        return flatMap(t -> predicate.test(t) ? just(t) : empty());
    }

    /**
     * Returns an Observable with its type mapped to Unit.
     */
    default Observable<Unit> opaque() {
        return map(x -> Unit.unit());
    }

    /**
     * Returns an Observable that accumulates this Observable's data into another Observable by
     * invoking |acc| whenever data are added to this, and closing the Scope it returns whenever
     * the data that added it is removed from this.
     *
     * The |factory| must create some object that extends Observable. Typically, this is a mutable
     * object like Cell (used in the implementation of fold()). This mutable Observable is passed
     * alongside the data from this Observable to the |acc| function, which can decide to mutate the
     * mutable Observable with whatever transformation of the data it chooses. The |acc| function
     * then returns a Scope which can defer another mutation operation until when the data are
     * removed from this Observable.
     *
     * This has a number of use cases. The fold() operator uses this with Cell to aggregate all the
     * simultaneous activations of the source into an Observable of a single value, which is useful
     * for computing sums or counts of the data (which in turn is used by the not() operator).
     *
     * Another use case is taking the most recent activation from the source Observable and
     * deduplicating equal activations.
     *
     *   Observable<Foo> source = ...;
     *   // |deduped| will have only one activation at a time, will not drop those activations if
     *   // the |source| revokes them, and will not deactivate and reactivate if |source| produces
     *   // data that .equals() the most recent data that preceded it.
     *   Observable<Foo> deduped = source.accumulate(() -> new Controller<Foo>(), (a, foo) -> {
     *       a.set(foo);
     *       return Scope.NO_OP;
     *   });
     */
    default <U, A extends Observable<U>> Observable<U> accumulate(
            Supplier<A> factory, BiFunction<A, T, ? extends Scope> acc) {
        return observer -> {
            A current = factory.get();
            return subscribe(t -> acc.apply(current, t)).and(current.subscribe(observer));
        };
    }

    /**
     * Returns an Observable that emits the most recent activation from |this|. Even if data is
     * revoked from |this|, the distinctUntilChanged() observable does not revoke it, and will only
     * do so once it receives a new *distinct* activation from |this|. If a new activation is
     * equivalent to the previous one, it is ignored.
     *
     * This provides a way to conveniently "break seams" in streams of data. If, for example, an
     * Observable emits `true`, `true`, `false`, `true`, in that order, the distinctUntilChanged()
     * Observable will emit `true`, `false`, `true` (dropping the redundant `true`).
     */
    default Observable<T> distinctUntilChanged() {
        return accumulate(Controller::new, (c, t) -> {
            c.set(t);
            return Scope.NO_OP;
        });
    }

    /**
     * Returns an Observable that combines the state of all of this Observable's data into
     * a single activation of type A, where the state is combined by successively applying |acc|
     * when this Observable adds data, and |dim| when this Observable removes data.
     *
     * By default, if |this| is empty, then the result Observable will be activated with the value
     * of |start|. If |this| is activated with data, then that T-typed data and the current A-typed
     * data from the result Observable will be fed to |acc| to calculate the new A-typed data for
     * the result Observable. If |this| has data deactivated, then that data, along with the current
     * A-typed data from the result Observable, will be fed to |dim| to calculate the new A-typed
     * data for the result Observable.
     *
     * There is always exactly one activation in the result Observable.
     *
     * This method provides a generic way to combine the state of multiple activations, "remember"
     * previous activations, or keep track of ordering in a way that can't be done with pure monadic
     * operations.
     */
    default <A> Observable<A> fold(A start, BiFunction<A, T, A> acc, BiFunction<A, T, A> dim) {
        return accumulate(() -> new Cell<A>(start), (current, t) -> {
            current.mutate(a -> acc.apply(a, t));
            return () -> current.mutate(a -> dim.apply(a, t));
        });
    }

    /**
     * Returns an Observable that contains the number of activations in |this|, which updates
     * dynamically as |this| updates.
     */
    default Observable<Integer> count() {
        return fold(0, (n, x) -> n + 1, (n, x) -> n - 1);
    }

    /**
     * Returns an Observable that is activated only when the given Observable is not activated.
     */
    static Observable<?> not(Observable<?> observable) {
        return observable.count().filter(n -> n == 0);
    }

    /**
     * A degenerate Observable that has no data.
     */
    static <T> Observable<T> empty() {
        return observer -> Scope.NO_OP;
    }

    /**
     * Creates an Observable that notifies subscribers with a single immutable value.
     *
     * This is the "return" operation in the Observable monad.
     */
    static <T> Observable<T> just(T value) {
        if (value == null) return empty();
        return observer -> observer.open(value);
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
    default Observable<T> debug(Consumer<String> logger) {
        return observer -> {
            logger.accept("subscribe");
            Scope subscription = subscribe(data -> {
                logger.accept("open " + data);
                Scope scope = observer.open(data);
                Scope debugClose = () -> logger.accept("close " + data);
                return scope.and(debugClose);
            });
            Scope debugUnsubscribe = () -> logger.accept("unsubscribe");
            return subscription.and(debugUnsubscribe);
        };
    }

    /**
     * An abstraction over posting a delayed task. Implementations should run the posted Runnable
     * after a certain amount of time has elapsed, preferably on the same thread that posted the
     * Runnable.
     */
    interface Scheduler {
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
    static Observable<?> alarm(Scheduler scheduler, long ms) {
        return observer -> {
            Controller<Unit> activation = new Controller<>();
            scheduler.postDelayed(() -> activation.set(Unit.unit()), ms);
            return activation.subscribe(observer);
        };
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
    default Observable<T> delay(Scheduler scheduler, long ms) {
        return flatMap(t -> alarm(scheduler, ms).map(x -> t));
    }
}
