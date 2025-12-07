// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.components.payments.service;

import org.chromium.build.annotations.NullMarked;

/** The interface for controlling the connection state of a service. */
@NullMarked
public interface Reconnectable {
    /** Connects to the remote service. */
    void connectToService();

    /** Terminates the connection to the remote service. */
    void terminateConnection();

    /** Cleans up the system resources that were used for the connection to the service. */
    void unbindService();
}
