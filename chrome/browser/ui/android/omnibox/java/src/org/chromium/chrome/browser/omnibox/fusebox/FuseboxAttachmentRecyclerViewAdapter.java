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

/** ModelListAdapter for FuseboxAttachment component. */
@NullMarked
class FuseboxAttachmentRecyclerViewAdapter extends SimpleRecyclerViewAdapter {
    @IntDef({
        FuseboxAttachmentType.ATTACHMENT_FILE,
        FuseboxAttachmentType.ATTACHMENT_IMAGE,
        FuseboxAttachmentType.ATTACHMENT_TAB
    })
    @Retention(RetentionPolicy.SOURCE)
    public @interface FuseboxAttachmentType {
        int ATTACHMENT_FILE = 0;
        int ATTACHMENT_IMAGE = 1;
        int ATTACHMENT_TAB = 2;
    }

    FuseboxAttachmentRecyclerViewAdapter(ModelList data) {
        super(data);
        registerType(
                FuseboxAttachmentType.ATTACHMENT_FILE,
                (parent) -> {
                    return parent.getContext()
                            .getSystemService(android.view.LayoutInflater.class)
                            .inflate(R.layout.fusebox_attachment_layout, parent, false);
                },
                FuseboxAttachmentViewBinder::bind);
        registerType(
                FuseboxAttachmentType.ATTACHMENT_IMAGE,
                (parent) -> {
                    return parent.getContext()
                            .getSystemService(android.view.LayoutInflater.class)
                            .inflate(R.layout.fusebox_image_attachment_layout, parent, false);
                },
                FuseboxAttachmentViewBinder::bind);
        registerType(
                FuseboxAttachmentType.ATTACHMENT_TAB,
                (parent) -> {
                    return parent.getContext()
                            .getSystemService(android.view.LayoutInflater.class)
                            .inflate(R.layout.fusebox_attachment_layout, parent, false);
                },
                FuseboxAttachmentViewBinder::bind);
    }
}
