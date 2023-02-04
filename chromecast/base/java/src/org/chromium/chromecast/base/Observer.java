// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chromecast.base;

import java.util.function.BiFunction;
import java.util.function.Consumer;

/**
 * Interface representing the actions to perform when entering and exiting a state.
 *
 * The open() implementation is called when entering the state, and the Scope that it returns is
 * invoked when leaving the state. The side-effects of open() are like a constructor, and the
 * side-effects of the Scope's close() are like a destructor.
 *
 * @param <T> The argument type that the constructor takes.
 */
public interface Observer<T> {
    Scope open(T data);

    /**
     * Shorthand for making an Observer that only has side effects when the scope of data is opened.
     *
     * An Observer normally returns a Scope that defers a "cleanup" action until when the scope of
     * the given data is closed (i.e. the data is removed from the Observable). If you do not need
     * any cleanup operation for when data is removed, you can use this helper function to make an
     * Observer that only responds when data is added.
     *
     * Example:
     *
     *   Cell<String> src = new Cell<>("a");
     *   Subscription sub = src.subscribe(Observer.onOpen(System.out::println));  // Prints "a"
     *   src.set("b");  // Prints "b"
     *   src.set("c");  // Prints "c"
     *
     * @param <T> The type of the data to be observed.
     * @param consumer The action to be run when the scope of data is opened.
     * @return An Observer that applies the side effect when data is added.
     */
    static <T> Observer<T> onOpen(Consumer<? super T> consumer) {
        return data -> {
            consumer.accept(data);
            return Scope.NO_OP;
        };
    }

    /**
     * Shorthand for making an Observer that only has side effects when the scope of data is closed.
     *
     * An Observer normally has effects that are executed immediately when the scope of data is
     * opened (i.e. data is added to the Observable). If you do not need to execute any operation
     * when data is added, but do need to register a "cleanup" action for the data, you can use this
     * helper function to make an Observer that only responds when data is removed.
     *
     * Example:
     *
     *   Cell<String> src = new Cell<>("a");
     *   Subscription sub = src.subscribe(Observer.onClose(System.out::println));  // Does nothing.
     *   src.set("b");  // Prints "a", because it was removed when replaced by "b"
     *   src.set("c");  // Prints "b", because it was removed when replaced by "c"
     *
     * @param <T> The type of the data to be observed.
     * @param consumer The action to be run when the scope of data is closed.
     * @return An Observer that applies side effect when data is removed.
     */
    static <T> Observer<T> onClose(Consumer<? super T> consumer) {
        return data -> () -> consumer.accept(data);
    }

    /**
     * Adapts an Observer-like function that takes two arguments into a true Observer that takes a
     * Both object.
     *
     * For example, one can refactor the following:
     *
     *   observableA.and(observableB).subscribe((Both<A, B> data) -> {
     *       A a = data.first;
     *       B b = data.second;
     *   });
     *
     * ... into this:
     *
     *   observableA.and(observableB).subscribe(Observers.both((A a, B b) -> ...));
     *
     * @param <A> The type of the first argument (and the first item in the Both).
     * @param <B> The type of the second argument (and the second item in the Both).
     * @param f The function to be converted into an Observer.
     * @return An Observer that can be subscribed to an Observable<Both<A, B>>.
     */
    static <A, B> Observer<Both<A, B>> both(BiFunction<? super A, ? super B, ? extends Scope> f) {
        return data -> f.apply(data.first, data.second);
    }
}
