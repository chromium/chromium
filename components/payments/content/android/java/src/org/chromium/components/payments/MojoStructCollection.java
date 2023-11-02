// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.components.payments;

import org.chromium.mojo.bindings.Struct;

import java.nio.ByteBuffer;
import java.util.Collection;

/** Helper class for serializing a collection of Mojo structs. */
public class MojoStructCollection {
    /**
     * Serialize a collection of Mojo structs.
     * @param collection A collection of Mojo structs to serialize.
     * @return An array of Mojo structs serialized into byte buffer objects.
     */
    public static <T extends Struct> ByteBuffer[] serialize(Collection<T> collection) {
        ByteBuffer[] result = new ByteBuffer[collection.size()];
        int i = 0;
        for (T item : collection) {
            result[i++] = item.serialize();
        }
        return result;
    }

    // Prevent instantiation.
    private MojoStructCollection() {}
}
