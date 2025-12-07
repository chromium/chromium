// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.ui.android.whats_new;

import android.content.Context;
import android.view.LayoutInflater;
import android.view.View;
import android.widget.ListView;

import org.chromium.build.annotations.NullMarked;
import org.chromium.chrome.browser.flags.ChromeFeatureList;
import org.chromium.components.browser_ui.bottomsheet.BottomSheetController;
import org.chromium.ui.modelutil.LayoutViewBuilder;
import org.chromium.ui.modelutil.MVCListAdapter.ModelList;
import org.chromium.ui.modelutil.ModelListAdapter;
import org.chromium.ui.modelutil.PropertyModel;
import org.chromium.ui.modelutil.PropertyModelChangeProcessor;

/** Coordinator for the What's New page. */
@NullMarked
public class WhatsNewCoordinator {
    private final WhatsNewMediator mMediator;

    private final WhatsNewBottomSheetContent mBottomSheetContent;

    private final View mView;

    private final PropertyModel mPropertyModel;
    private final ModelList mModelList;

    /**
     * Constructs and initializes the WhatsNewCoordinator.
     *
     * @param context A {@link Context} to create views and retrieve resources.
     * @param bottomSheetController A {@link BottomSheetController} used to show/hide the sheet
     */
    public WhatsNewCoordinator(Context context, BottomSheetController bottomSheetController) {
        // Inflate the XML.
        mView = LayoutInflater.from(context).inflate(R.layout.whats_new_page, /* root= */ null);

        mModelList = new ModelList();
        ModelListAdapter adapter = new ModelListAdapter(mModelList);
        adapter.registerType(
                WhatsNewListItemProperties.DEFAULT_ITEM_TYPE,
                new LayoutViewBuilder<>(R.layout.whats_new_list_item),
                WhatsNewListItemViewBinder::bind);

        ListView listView = mView.findViewById(R.id.whats_new_items_list);
        listView.setAdapter(adapter);

        mPropertyModel = new PropertyModel.Builder(WhatsNewProperties.ALL_KEYS).build();
        mMediator = new WhatsNewMediator(context, mPropertyModel, mModelList);

        mBottomSheetContent =
                new WhatsNewBottomSheetContent(
                        mView, bottomSheetController, mMediator::onBackButtonPressed);

        PropertyModelChangeProcessor.create(
                mPropertyModel, mBottomSheetContent, WhatsNewViewBinder::bind);
    }

    /* Returns the view that shows in the What's New page.  */
    public View getView() {
        return mView;
    }

    /* Request to show the What's New bottom sheet.  */
    public void showBottomSheet() {
        if (isEnabled()) {
            mMediator.showBottomSheet();
        }
    }

    PropertyModel getModelForTesting() {
        return mPropertyModel;
    }

    ModelList getModelListForTesting() {
        return mModelList;
    }

    private boolean isEnabled() {
        return ChromeFeatureList.isEnabled(ChromeFeatureList.CLANK_WHATS_NEW);
    }
}
