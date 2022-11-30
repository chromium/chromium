// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.components.content_creation.reactions;

/**
 * Model class for a lightweight reaction.
 */
public class ReactionMetadata {
    public final @ReactionType int type;
    public final String localizedName;
    public final String thumbnailUrl;
    public final String assetUrl;
    public final int frameCount;

    public ReactionMetadata(@ReactionType int type, String localizedName, String thumbnailUrl,
            String assetUrl, int frameCount) {
        this.type = type;
        this.localizedName = localizedName;
        this.thumbnailUrl = thumbnailUrl;
        this.assetUrl = assetUrl;
        this.frameCount = frameCount;
    }
}