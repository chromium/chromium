// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.ui.android.whats_new;

import android.content.Context;

import org.chromium.build.annotations.NullMarked;
import org.chromium.chrome.browser.ui.android.whats_new.WhatsNewProperties.ViewState;
import org.chromium.chrome.browser.ui.android.whats_new.features.WhatsNewFeature;
import org.chromium.chrome.browser.ui.android.whats_new.features.WhatsNewFeatureProvider;
import org.chromium.ui.modelutil.MVCListAdapter.ModelList;
import org.chromium.ui.modelutil.ModelListAdapter;
import org.chromium.ui.modelutil.PropertyModel;

@NullMarked
public class WhatsNewMediator {
    private final Context mContext;
    private final PropertyModel mModel;

    private final ModelList mItemList;

    WhatsNewMediator(Context context, PropertyModel model, ModelList itemList) {
        mContext = context;
        mModel = model;

        mItemList = itemList;
        populateFeatureList();
    }

    void showBottomSheet() {
        setState(ViewState.OVERVIEW);
    }

    private void populateFeatureList() {
        for (WhatsNewFeature feature : WhatsNewFeatureProvider.getFeatureEntries()) {
            PropertyModel model =
                    WhatsNewListItemProperties.buildModelForFeature(
                            mContext, feature, this::onFeatureItemClicked);
            mItemList.add(
                    new ModelListAdapter.ListItem(
                            WhatsNewListItemProperties.DEFAULT_ITEM_TYPE, model));
        }
    }

    private void onFeatureItemClicked(WhatsNewFeature feature) {
        setState(ViewState.DETAIL);
    }

    void onBackButtonPressed() {
        if (mModel.get(WhatsNewProperties.VIEW_STATE) == ViewState.DETAIL) {
            setState(ViewState.OVERVIEW);
        } else {
            setState(ViewState.HIDDEN);
        }
    }

    private void setState(@ViewState int state) {
        mModel.set(WhatsNewProperties.VIEW_STATE, state);
    }
}
