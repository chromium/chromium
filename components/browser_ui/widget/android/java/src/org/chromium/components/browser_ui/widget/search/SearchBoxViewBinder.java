// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.components.browser_ui.widget.search;

import org.chromium.build.annotations.NullMarked;
import org.chromium.ui.modelutil.PropertyKey;
import org.chromium.ui.modelutil.PropertyModel;
import org.chromium.ui.modelutil.PropertyModelChangeProcessor.ViewBinder;

/** Binds properties to the desktop search box view. */
@NullMarked
public class SearchBoxViewBinder {
    public static void bind(PropertyModel model, SearchBoxView view, PropertyKey key) {
        if (key == SearchBoxProperties.TEXT_CHANGED_CALLBACK) {
            view.setSearchTextCallback(model.get(SearchBoxProperties.TEXT_CHANGED_CALLBACK));
        } else if (key == SearchBoxProperties.SEARCH_TEXT) {
            String text = model.get(SearchBoxProperties.SEARCH_TEXT);
            view.setSearchText(text);
        } else if (key == SearchBoxProperties.HINT_TEXT) {
            view.setHintText(model.get(SearchBoxProperties.HINT_TEXT));
        } else if (key == SearchBoxProperties.FOCUS_CHANGED_CALLBACK) {
            view.setFocusChangeCallback(model.get(SearchBoxProperties.FOCUS_CHANGED_CALLBACK));
        } else if (key == SearchBoxProperties.HAS_FOCUS) {
            view.setSearchTextFocus(model.get(SearchBoxProperties.HAS_FOCUS));
        } else if (key == SearchBoxProperties.CLEAR_SEARCH_TEXT_RUNNABLE) {
            view.setClearButtonClickedRunnable(
                    model.get(SearchBoxProperties.CLEAR_SEARCH_TEXT_RUNNABLE));
        } else if (key == SearchBoxProperties.CLEAR_BUTTON_VISIBILITY) {
            view.setClearButtonVisibility(model.get(SearchBoxProperties.CLEAR_BUTTON_VISIBILITY));
        } else if (key == SearchBoxProperties.SEARCH_LOUPE_VISIBILITY) {
            view.setSearchLoupeVisibility(model.get(SearchBoxProperties.SEARCH_LOUPE_VISIBILITY));
        }
    }

    public static ViewBinder<PropertyModel, SearchBoxView, PropertyKey> createViewBinder() {
        return SearchBoxViewBinder::bind;
    }
}
