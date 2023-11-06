// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.components.query_tiles.bridges;

import androidx.annotation.Nullable;

import org.jni_zero.CalledByNative;
import org.jni_zero.JNINamespace;

import org.chromium.components.query_tiles.QueryTile;
import org.chromium.url.GURL;

import java.util.ArrayList;
import java.util.List;

/**
 * Bridge to the native query tile service for the given {@link Profile}.
 */
@JNINamespace("query_tiles")
public class TileConversionBridge {
    @CalledByNative
    private static List<QueryTile> createList() {
        return new ArrayList<>();
    }

    @CalledByNative
    private static QueryTile createTileAndMaybeAddToList(@Nullable List<QueryTile> list,
            String tileId, String displayTitle, String accessibilityText, String queryText,
            GURL[] urls, String[] searchParams, List<QueryTile> children) {
        QueryTile tile = new QueryTile(
                tileId, displayTitle, accessibilityText, queryText, urls, searchParams, children);
        if (list != null) list.add(tile);
        return tile;
    }
}
