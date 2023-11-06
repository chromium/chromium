// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.omnibox.suggestions.querytiles;

import org.chromium.ui.modelutil.PropertyKey;
import org.chromium.ui.modelutil.PropertyModel;

/** Binds QueryTileView properties. */
public interface QueryTileViewBinder {
    public static void bind(PropertyModel model, QueryTileView view, PropertyKey propertyKey) {
        if (QueryTileViewProperties.IMAGE == propertyKey) {
            view.setImage(model.get(QueryTileViewProperties.IMAGE));
        } else if (QueryTileViewProperties.TITLE == propertyKey) {
            view.setTitle(model.get(QueryTileViewProperties.TITLE));
        } else if (QueryTileViewProperties.ON_FOCUS_VIA_SELECTION == propertyKey) {
            view.setOnFocusViaSelectionListener(
                    model.get(QueryTileViewProperties.ON_FOCUS_VIA_SELECTION));
        } else if (QueryTileViewProperties.ON_CLICK == propertyKey) {
            view.setOnClickListener(model.get(QueryTileViewProperties.ON_CLICK));
        }
    }
}
