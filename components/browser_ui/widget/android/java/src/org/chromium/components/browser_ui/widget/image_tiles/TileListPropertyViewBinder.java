// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.components.browser_ui.widget.image_tiles;

import androidx.recyclerview.widget.RecyclerView;

import org.chromium.ui.modelutil.PropertyKey;
import org.chromium.ui.modelutil.PropertyModel;
import org.chromium.ui.modelutil.PropertyModelChangeProcessor.ViewBinder;

/**
 * Binds the {@link TileListModel} with the {@link TileListView}.
 */
class TileListPropertyViewBinder implements ViewBinder<PropertyModel, RecyclerView, PropertyKey> {
    @Override
    public void bind(PropertyModel model, RecyclerView view, PropertyKey propertyKey) {}
}