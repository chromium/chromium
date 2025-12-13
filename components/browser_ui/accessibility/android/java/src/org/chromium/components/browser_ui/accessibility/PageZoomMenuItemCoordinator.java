// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.components.browser_ui.accessibility;

import org.chromium.build.annotations.NullMarked;
import org.chromium.build.annotations.Nullable;
import org.chromium.ui.modelutil.PropertyModel;

/**
 * Coordinator for the page zoom menu item. This class is responsible for mediating between the page
 * zoom feature and the app menu.
 *
 * <p>This coordinator is non-standard, as it does not create its own view or model. Instead, the
 * model is created and set from the outside. This is because the app menu framework is responsible
 * for creating the views and models for all menu items. This coordinator simply hooks into the
 * existing infrastructure to provide page zoom functionality.
 */
@NullMarked
public class PageZoomMenuItemCoordinator {
    private final PageZoomManager mManager;
    private @Nullable PropertyModel mModel;

    @SuppressWarnings("unused")
    private @Nullable PageZoomMenuItemMediator mMediator;

    public PageZoomMenuItemCoordinator(PageZoomManager manager) {
        mManager = manager;
    }

    public void setModel(PropertyModel model) {
        mModel = model;
        mMediator = new PageZoomMenuItemMediator(mModel, mManager);
    }
}
