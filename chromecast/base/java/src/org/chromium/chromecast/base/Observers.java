// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chromecast.base;

import java.util.function.BiFunction;
import java.util.function.Consumer;

/**
 * Helper functions for creating Observers, used by Observable.subscribe() to handle state changes.
 */
public final class Observers {
    // Uninstantiable.
    private Observers() {}

    /**
     * Shorthand for making a Observer that only has side effects on activation.
     *
     * @param <T> The type of the activation data.
     */
    public static <T> Observer<T> onEnter(Consumer<? super T> consumer) {
        return (T value) -> {
            consumer.accept(value);
            return Scopes.NO_OP;
        };
    }

    /**
     * Shorthand for making a Observer that only has side effects on deactivation.
     *
     * @param <T> The type of the activation data.
     */
    public static <T> Observer<T> onExit(Consumer<? super T> consumer) {
        return (T value) -> () -> consumer.accept(value);
    }

    /**
     * Adapts a Observer-like function that takes two arguments into a true Observer that
     * takes a Both object.
     *
     * @param <A> The type of the first argument (and the first item in the Both).
     * @param <B> The type of the second argument (and the second item in the Both).
     *
     * For example, one can refactor the following:
     *
     *     observableA.and(observableB).subscribe((Both<A, B> data) -> {
     *         A a = data.first;
     *         B b = data.second;
     *         ...
     *     });
     *
     * ... into this:
     *
     *     observableA.and(observableB).subscribe(Observers.both((A a, B b) -> ...));
     */
    public static <A, B> Observer<Both<A, B>> both(
            BiFunction<? super A, ? super B, ? extends Scope> function) {
        return (Both<A, B> data) -> function.apply(data.first, data.second);
    }
}
