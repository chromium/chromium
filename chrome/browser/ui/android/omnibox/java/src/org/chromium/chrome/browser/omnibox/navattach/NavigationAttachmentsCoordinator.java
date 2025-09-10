// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.omnibox.navattach;

import android.content.Context;
import android.view.ViewGroup;

import org.chromium.base.supplier.ObservableSupplier;
import org.chromium.build.annotations.NullMarked;
import org.chromium.build.annotations.Nullable;
import org.chromium.chrome.browser.omnibox.R;
import org.chromium.chrome.browser.omnibox.UrlFocusChangeListener;
import org.chromium.chrome.browser.profiles.Profile;
import org.chromium.components.omnibox.OmniboxFeatures;
import org.chromium.ui.base.WindowAndroid;
import org.chromium.ui.modelutil.MVCListAdapter.ModelList;
import org.chromium.ui.modelutil.PropertyModel;
import org.chromium.ui.modelutil.PropertyModelChangeProcessor;
import org.chromium.url.GURL;

/** Coordinator for the Navigation Attachments component. */
@NullMarked
public class NavigationAttachmentsCoordinator implements UrlFocusChangeListener {
    private final @Nullable NavigationAttachmentsMediator mMediator;
    private final @Nullable NavigationAttachmentsViewHolder mViewHolder;

    public NavigationAttachmentsCoordinator(
            Context context,
            WindowAndroid windowAndroid,
            ViewGroup parent,
            ObservableSupplier<Profile> profileObservableSupplier) {
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

        var modelList = new ModelList();
        var adapter = new NavigationAttachmentsRecyclerViewAdapter(modelList);
        mViewHolder.attachmentsView.setAdapter(adapter);

        PropertyModel model =
                new PropertyModel.Builder(NavigationAttachmentsProperties.ALL_KEYS)
                        .with(NavigationAttachmentsProperties.ADAPTER, adapter)
                        .with(NavigationAttachmentsProperties.TOOLBAR_VISIBLE, false)
                        .build();
        PropertyModelChangeProcessor.create(
                model, mViewHolder, NavigationAttachmentsViewBinder::bind);
        mMediator =
                new NavigationAttachmentsMediator(
                        context,
                        windowAndroid,
                        model,
                        mViewHolder,
                        modelList,
                        profileObservableSupplier);
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

    @Nullable NavigationAttachmentsViewHolder getViewHolderForTesting() {
        return mViewHolder;
    }

    /**
     * Whether the url returned by {@link #getAimUrl} should be used when the user submits a typed
     * query.
     */
    public boolean shouldUseAimUrl() {
        return mMediator != null && mMediator.isUsingAiMode();
    }

    /** Returns the URL associated with the current AIM session. */
    public GURL getAimUrl(String queryText) {
        if (mMediator == null) return GURL.emptyGURL();
        return mMediator.getAimUrl(queryText);
    }
}
