// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.omnibox.fusebox;

import androidx.annotation.IntDef;

import org.chromium.build.annotations.NullMarked;
import org.chromium.chrome.browser.omnibox.R;
import org.chromium.ui.modelutil.MVCListAdapter.ModelList;
import org.chromium.ui.modelutil.SimpleRecyclerViewAdapter;

import java.lang.annotation.Retention;
import java.lang.annotation.RetentionPolicy;

/** ModelListAdapter for NavigationAttachments component. */
@NullMarked
class NavigationAttachmentsRecyclerViewAdapter extends SimpleRecyclerViewAdapter {
    @IntDef({
        NavigationAttachmentItemType.ATTACHMENT_ITEM,
        NavigationAttachmentItemType.ATTACHMENT_IMAGE,
        NavigationAttachmentItemType.ATTACHMENT_TAB
    })
    @Retention(RetentionPolicy.SOURCE)
    public @interface NavigationAttachmentItemType {
        int ATTACHMENT_ITEM = 0;
        int ATTACHMENT_IMAGE = 1;
        int ATTACHMENT_TAB = 2;
    }

    NavigationAttachmentsRecyclerViewAdapter(ModelList data) {
        super(data);
        registerType(
                NavigationAttachmentItemType.ATTACHMENT_ITEM,
                (parent) -> {
                    return parent.getContext()
                            .getSystemService(android.view.LayoutInflater.class)
                            .inflate(R.layout.fusebox_attachment_layout, parent, false);
                },
                NavigationAttachmentItemViewBinder::bind);
        registerType(
                NavigationAttachmentItemType.ATTACHMENT_IMAGE,
                (parent) -> {
                    return parent.getContext()
                            .getSystemService(android.view.LayoutInflater.class)
                            .inflate(R.layout.fusebox_attachment_layout, parent, false);
                },
                NavigationAttachmentItemViewBinder::bind);
        registerType(
                NavigationAttachmentItemType.ATTACHMENT_TAB,
                (parent) -> {
                    return parent.getContext()
                            .getSystemService(android.view.LayoutInflater.class)
                            .inflate(R.layout.fusebox_attachment_layout, parent, false);
                },
                NavigationAttachmentItemViewBinder::bind);
    }
}
