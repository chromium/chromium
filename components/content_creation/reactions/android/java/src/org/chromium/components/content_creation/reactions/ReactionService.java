// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.components.content_creation.reactions;

import org.chromium.base.Callback;

import java.util.List;

/**
 * Interface for interacting with the native Reaction service. The service is
 * responsible for building the reaction list in the right order.
 */
public interface ReactionService {
    /**
     * Gets the list of available reactions.
     */
    void getReactions(Callback<List<ReactionMetadata>> callback);
}
