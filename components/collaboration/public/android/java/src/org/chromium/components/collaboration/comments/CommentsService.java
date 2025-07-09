// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.components.collaboration.comments;

import org.chromium.build.annotations.NullMarked;

// This interface serves as the primary interaction point for the UI to query
// for and manage comments. It provides a purely asynchronous API for all data
// access to ensure the UI thread is never blocked.
// The main implementation for this lives as a KeyedService in C++.
@NullMarked
public interface CommentsService {

    // Returns whether the service has fully initialized.
    boolean isInitialized();

    // Returns true if this is an empty implementation.
    boolean isEmptyService();
}
