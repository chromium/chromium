// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.omnibox.fusebox;

import org.chromium.build.annotations.NullMarked;
import org.chromium.chrome.browser.omnibox.suggestions.SelectionController;

/** SelectionController for Fusebox attachments. */
@NullMarked
class AttachmentsSelectionController extends SelectionController {

    private final FuseboxAttachmentModelList mModelList;

    public AttachmentsSelectionController(FuseboxAttachmentModelList modelList) {
        super(Mode.SATURATING_WITH_SENTINEL);
        mModelList = modelList;
    }

    @Override
    protected int getItemCount() {
        return mModelList.size();
    }

    @Override
    protected void setItemState(int position, boolean isSelected) {
        if (position >= getItemCount()) return;
        mModelList
                .get(position)
                .model
                .set(FuseboxAttachmentProperties.REMOVE_BUTTON_SELECTED, isSelected);
    }

    public void handleActivation() {
        Integer position = getPosition();
        if (position == null) return;
        mModelList.get(position).model.get(FuseboxAttachmentProperties.ON_REMOVE).run();
        setPosition(Math.min(position, getItemCount() - 1));
    }
}
