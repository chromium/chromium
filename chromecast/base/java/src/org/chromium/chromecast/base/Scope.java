// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chromecast.base;

/**
 * Interface representing the actions to perform when exiting a state.
 *
 * The close() method is invoked when leaving the state. The side-effects of this method are like a
 * destructor.
 */
public interface Scope extends AutoCloseable {
    // Implements AutoCloseable, with the added constraint that no checked exceptions are thrown.
    @Override
    public void close();
}
