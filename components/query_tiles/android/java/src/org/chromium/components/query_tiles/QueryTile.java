// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.components.query_tiles;

import android.text.TextUtils;

import androidx.annotation.Nullable;

import org.chromium.components.browser_ui.widget.image_tiles.ImageTile;

import java.util.ArrayList;
import java.util.Arrays;
import java.util.Collections;
import java.util.List;

/**
 * Class encapsulating data needed to render a query tile for the query sites section on the NTP.
 */
public class QueryTile extends ImageTile {
    /** The string to be used for building a query when this tile is clicked. */
    public final String queryText;

    /** The next level tiles to be shown when this tile is clicked. */
    public final List<QueryTile> children;

    /** The urls of the images to be shown for the tile. */
    public final List<String> urls;

    /** The urls of the images to be shown for the tile. */
    public final List<String> searchParams;

    /** Constructor. */
    public QueryTile(String id, String displayTitle, String accessibilityText, String queryText,
            String[] urls, String[] searchParams, List<QueryTile> children) {
        super(id, displayTitle, accessibilityText);
        this.queryText = queryText;
        this.urls = Arrays.asList(urls);
        this.searchParams =
                (searchParams == null) ? new ArrayList<>() : Arrays.asList(searchParams);
        this.children =
                Collections.unmodifiableList(children == null ? new ArrayList<>() : children);
    }

    @Override
    public boolean equals(@Nullable Object obj) {
        if (!super.equals(obj)) return false;
        if (!(obj instanceof QueryTile)) return false;

        QueryTile other = (QueryTile) obj;

        if (!TextUtils.equals(queryText, other.queryText)) return false;

        if (children != null && !children.equals(other.children)) return false;
        if (children == null && other.children != null) return false;

        if (urls != null && !urls.equals(other.urls)) return false;
        if (urls == null && other.urls != null) return false;

        return true;
    }
}
