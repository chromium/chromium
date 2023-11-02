// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chromecast.base;

/**
 * Encapsulates a mutable variable. Useful for sharing mutable state between the local scope and
 * capturing lambdas.
 *
 *   Box<Integer> box = new Box<>(0);
 *   foo.subscribe(Observers.onOpen(x -> println("Times invoked: " + ++box.value)));
 *   println("Times invoked right after subscription: " + box.value);
 *
 * This should be used sparingly, because it implies sharing mutable state. It's best used locally
 * where all uses of the boxed value are easily identifiable on sight.
 *
 * @param <T> The wrapped value type.
 */
public class Box<T> {
    public T value;

    public Box(T value) {
        this.value = value;
    }
}