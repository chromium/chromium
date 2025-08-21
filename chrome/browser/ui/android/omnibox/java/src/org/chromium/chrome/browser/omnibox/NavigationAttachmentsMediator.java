// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.omnibox;

import org.chromium.build.annotations.NullMarked;
import org.chromium.ui.modelutil.PropertyModel;

/** Mediator for the Navigation Attachments component. */
@NullMarked
class NavigationAttachmentsMediator {
    private final PropertyModel mModel;

    NavigationAttachmentsMediator(PropertyModel model) {
        mModel = model;
        // TODO: remove line below when more wiring is available.
        // Keeping this line because otherwise member variable would be unused.
        mModel.set(NavigationAttachmentsProperties.TOOLBAR_VISIBLE, false);
    }

    void destroy() {}
}
