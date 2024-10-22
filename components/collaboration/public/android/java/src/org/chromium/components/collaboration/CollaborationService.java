// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.components.collaboration;

import androidx.annotation.VisibleForTesting;

/**
 * CollaborationService is the core class for managing collaboration group flows. It represents a
 * native CollaborationService object in Java.
 */
public interface CollaborationService {
    /**
     * Whether the service is an empty implementation. This is here because the Chromium build
     * disables RTTI, and we need to be able to verify that we are using an empty service from the
     * Chrome embedder.
     *
     * @return Whether the service implementation is empty.
     */
    @VisibleForTesting
    boolean isEmptyService();
}
