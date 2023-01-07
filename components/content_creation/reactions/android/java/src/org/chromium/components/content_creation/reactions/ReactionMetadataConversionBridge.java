// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.components.content_creation.reactions;

import androidx.annotation.Nullable;

import org.chromium.base.annotations.CalledByNative;
import org.chromium.base.annotations.JNINamespace;

import java.util.ArrayList;
import java.util.List;

/**
 * Bridge class in charge of creating Java {@link ReactionMetadata} instances based on their native
 * counterpart.
 */
@JNINamespace("content_creation")
public class ReactionMetadataConversionBridge {
    /**
     * Creates an empty Java List instance to be used in native.
     * @return a reference to an empty Java List.
     */
    @CalledByNative
    private static List<ReactionMetadata> createReactionList() {
        return new ArrayList<>();
    }

    /**
     * Creates a {@link ReactionMetadata} instance based on the given parameters,
     * and then attempts to add it to the given list.
     * @return the {@link ReactionMetadata} instance.
     */
    @CalledByNative
    private static ReactionMetadata createMetadataAndMaybeAddToList(
            @Nullable List<ReactionMetadata> list, int type, String localizedName,
            String thumbnailUrl, String assetUrl, int frameCount) {
        ReactionMetadata metadata =
                new ReactionMetadata(type, localizedName, thumbnailUrl, assetUrl, frameCount);

        if (list != null) {
            list.add(metadata);
        }

        return metadata;
    }
}