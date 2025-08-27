// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.components.collaboration;

import org.chromium.build.annotations.NullMarked;

/** An interface to manage collaboration flow UI screens. */
@NullMarked
public interface CollaborationControllerDelegate {
    /**
     * This method is called exactly once, and the service takes ownership of the native and java
     * object after this call.
     *
     * @return The native pointer of the current delegate.
     */
    long getNativePtr();

    /** Cleans up any outstanding resources. */
    void destroy();
}
