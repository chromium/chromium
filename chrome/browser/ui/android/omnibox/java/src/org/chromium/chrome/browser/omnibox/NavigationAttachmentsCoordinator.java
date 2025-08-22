// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.omnibox;

import android.content.Context;
import android.view.ViewGroup;

import androidx.annotation.VisibleForTesting;

import org.chromium.build.annotations.NullMarked;
import org.chromium.build.annotations.Nullable;
import org.chromium.components.omnibox.OmniboxFeatures;
import org.chromium.ui.base.WindowAndroid;
import org.chromium.ui.modelutil.PropertyModel;
import org.chromium.ui.modelutil.PropertyModelChangeProcessor;

/** Coordinator for the Navigation Attachments component. */
@NullMarked
public class NavigationAttachmentsCoordinator implements UrlFocusChangeListener {
    private final @Nullable NavigationAttachmentsMediator mMediator;
    private final @Nullable NavigationAttachmentsViewHolder mViewHolder;

    public NavigationAttachmentsCoordinator(
            Context context, WindowAndroid windowAndroid, ViewGroup parent) {
        if (!OmniboxFeatures.sOmniboxMultimodalInput.isEnabled()
                || parent.findViewById(R.id.location_bar_navigation_toolbar) == null) {
            mMediator = null;
            mViewHolder = null;
            return;
        }

        var popup =
                new NavigationAttachmentsPopup(
                        context, parent.findViewById(R.id.location_bar_attachments_add));
        mViewHolder = new NavigationAttachmentsViewHolder(parent, popup);

        PropertyModel model =
                new PropertyModel.Builder(NavigationAttachmentsProperties.ALL_KEYS)
                        .with(NavigationAttachmentsProperties.TOOLBAR_VISIBLE, false)
                        .build();
        PropertyModelChangeProcessor.create(
                model, mViewHolder, NavigationAttachmentsViewBinder::bind);
        mMediator = new NavigationAttachmentsMediator(windowAndroid, model, mViewHolder);
    }

    public void destroy() {
        if (mMediator != null) {
            mMediator.destroy();
        }
    }

    /** Called when the URL focus changes. */
    @Override
    public void onUrlFocusChange(boolean hasFocus) {
        if (mMediator != null) {
            mMediator.onUrlFocusChange(hasFocus);
        }
    }

    @VisibleForTesting
    @Nullable NavigationAttachmentsViewHolder getViewHolderForTesting() {
        return mViewHolder;
    }
}
