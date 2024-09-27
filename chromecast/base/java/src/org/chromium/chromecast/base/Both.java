// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chromecast.base;

import java.util.Objects;
import java.util.function.BiConsumer;
import java.util.function.BiFunction;
import java.util.function.BiPredicate;
import java.util.function.Consumer;
import java.util.function.Function;
import java.util.function.Predicate;

/**
 * Represents a structure containing an instance of both A and B.
 *
 * In algebraic type theory, this is the Product type.
 *
 * This is useful for representing many objects in one without having to create a new class.
 * The type itself is composable, so the type parameters can themselves be Boths, and so on
 * recursively. This way, one can make a binary tree of types and wrap it in a single type that
 * represents the composition of all the leaf types.
 *
 * @param <A> The first type.
 * @param <B> The second type.
 */
public class Both<A, B> {
    public final A first;
    public final B second;

    private Both(A a, B b) {
        this.first = a;
        this.second = b;
    }

    // Can be used as a method reference.
    public A getFirst() {
        return this.first;
    }

    // Can be used as a method reference.
    public B getSecond() {
        return this.second;
    }

    @Override
    public String toString() {
        return this.first + ", " + this.second;
    }

    @Override
    public boolean equals(Object other) {
        if (other instanceof Both) {
            Both<?, ?> that = (Both<?, ?>) other;
            return this.first.equals(that.first) && this.second.equals(that.second);
        }
        return false;
    }

    @Override
    public int hashCode() {
        return Objects.hash(this.first, this.second);
    }

    /**
     * Constructs a Both object containing both `a` and `b`.
     */
    public static <A, B> Both<A, B> both(A a, B b) {
        assert a != null;
        assert b != null;
        return new Both<>(a, b);
    }

    /**
     * Turns a function of two arguments into a function of a single Both argument.
     */
    public static <A, B, R> Function<Both<A, B>, R> adapt(
            BiFunction<? super A, ? super B, ? extends R> function) {
        return (Both<A, B> data) -> function.apply(data.first, data.second);
    }

    /**
     * Turns a consumer of two arguments into a consumer of a single Both argument.
     */
    public static <A, B> Consumer<Both<A, B>> adapt(BiConsumer<? super A, ? super B> consumer) {
        return (Both<A, B> data) -> consumer.accept(data.first, data.second);
    }

    /**
     * Turns a predicate of two arguments into a predicate of a single Both argument.
     */
    public static <A, B> Predicate<Both<A, B>> adapt(BiPredicate<? super A, ? super B> predicate) {
        return (Both<A, B> data) -> predicate.test(data.first, data.second);
    }
}
