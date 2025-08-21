// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.omnibox;

import android.content.Context;
import android.view.ViewGroup;

import org.chromium.build.annotations.NullMarked;
import org.chromium.build.annotations.Nullable;
import org.chromium.components.omnibox.OmniboxFeatures;
import org.chromium.ui.modelutil.PropertyModel;
import org.chromium.ui.modelutil.PropertyModelChangeProcessor;

/** Coordinator for the Navigation Attachments component. */
@NullMarked
public class NavigationAttachmentsCoordinator {
    private final @Nullable NavigationAttachmentsMediator mMediator;

    public NavigationAttachmentsCoordinator(Context context, ViewGroup parent) {
        if (!OmniboxFeatures.sOmniboxMultimodalInput.isEnabled()
                || parent.findViewById(R.id.location_bar_navigation_toolbar) == null) {
            mMediator = null;
            return;
        }
        PropertyModel model =
                new PropertyModel.Builder(NavigationAttachmentsProperties.ALL_KEYS)
                        .with(NavigationAttachmentsProperties.TOOLBAR_VISIBLE, false)
                        .build();
        PropertyModelChangeProcessor.create(model, parent, NavigationAttachmentsViewBinder::bind);
        mMediator = new NavigationAttachmentsMediator(model);
    }

    public void destroy() {
        if (mMediator != null) {
            mMediator.destroy();
        }
    }
}
