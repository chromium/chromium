// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.components.content_creation.reactions;

import org.chromium.base.annotations.CalledByNative;
import org.chromium.base.annotations.JNINamespace;

/**
 * Bridge class in charge of creating Java {@link ReactionMetadata} instances based on their native
 * counterpart.
 */
@JNINamespace("content_creation")
public class ReactionMetadataConversionBridge {
    /**
     * Creates a {@link ReactionMetadata} instance based on the given parameters.
     * @return the {@link ReactionMetadata} instance.
     */
    @CalledByNative
    private static ReactionMetadata createReactionMetadata(
            int type, String localizedName, String thumbnailUrl, String assetUrl) {
        return new ReactionMetadata(type, localizedName, thumbnailUrl, assetUrl);
    }
}