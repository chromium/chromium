// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chromecast.base;

/**
 * A function that takes a single argument and returns a boolean.
 *
 * TODO(sanfin): replace with Java 8 library if we're ever able to use the Java 8 library.
 *
 * @param <T> The argument type.
 */
public interface Predicate<T> { public boolean test(T value); }
