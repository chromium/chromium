// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chromecast.base;

/**
 * A function that takes two arguments and returns a value.
 *
 * TODO(sanfin): replace with Java 8 library if we're ever able to use the Java 8 library.
 *
 * @param <A> The first argument type.
 * @param <B> The second argument type.
 * @param <R> The return type.
 */
public interface BiFunction<A, B, R> { public R apply(A first, B second); }
