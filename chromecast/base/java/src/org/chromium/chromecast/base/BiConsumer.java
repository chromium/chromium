// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chromecast.base;

/**
 * A function that takes two arguments and returns void.
 *
 * TODO(sanfin): replace with Java 8 library if we're ever able to use the Java 8 library.
 *
 * @param <A> The first argument type.
 * @param <B> The second argument type.
 */
public interface BiConsumer<A, B> { public void accept(A first, B second); }
