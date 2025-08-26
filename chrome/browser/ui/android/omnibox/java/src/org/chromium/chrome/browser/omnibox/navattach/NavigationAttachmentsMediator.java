// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.omnibox.navattach;

import android.app.Activity;
import android.content.ClipData;
import android.content.Context;
import android.content.Intent;
import android.graphics.Bitmap;
import android.graphics.drawable.BitmapDrawable;
import android.graphics.drawable.Drawable;
import android.net.Uri;
import android.provider.MediaStore;

import androidx.appcompat.content.res.AppCompatResources;

import org.chromium.build.annotations.NullMarked;
import org.chromium.build.annotations.Nullable;
import org.chromium.chrome.browser.omnibox.R;
import org.chromium.ui.base.WindowAndroid;
import org.chromium.ui.modelutil.MVCListAdapter.ModelList;
import org.chromium.ui.modelutil.PropertyModel;
import org.chromium.ui.modelutil.SimpleRecyclerViewAdapter;

import java.util.ArrayList;
import java.util.List;

/** Mediator for the Navigation Attachments component. */
@NullMarked
class NavigationAttachmentsMediator {
    private final Context mContext;
    private final WindowAndroid mWindowAndroid;
    private final PropertyModel mModel;
    private final NavigationAttachmentsPopup mPopup;
    private final ModelList mModelList;
    private final Drawable mFallbackDrawable;

    NavigationAttachmentsMediator(
            Context context,
            WindowAndroid windowAndroid,
            PropertyModel model,
            NavigationAttachmentsViewHolder viewHolder,
            ModelList modelList) {
        mContext = context;
        mWindowAndroid = windowAndroid;
        mModel = model;
        mPopup = viewHolder.popup;
        mModelList = modelList;
        mFallbackDrawable =
                AppCompatResources.getDrawable(mContext, R.drawable.ic_attach_file_24dp);

        mModel.set(
                NavigationAttachmentsProperties.BUTTON_ADD_CLICKED, this::onToggleAttachmentsPopup);
        mModel.set(NavigationAttachmentsProperties.POPUP_CAMERA_CLICKED, this::launchCamera);
        mModel.set(NavigationAttachmentsProperties.POPUP_GALLERY_CLICKED, this::launchImagePicker);
        mModel.set(NavigationAttachmentsProperties.POPUP_FILE_CLICKED, this::launchFilePicker);
        mModel.set(
                NavigationAttachmentsProperties.ON_USE_AI_MODE_CHANGED, this::onUseAiModeChanged);
    }

    void destroy() {}

    void onUseAiModeChanged(boolean enabled) {
        if (!enabled) {
            mModelList.clear();
            mModel.set(NavigationAttachmentsProperties.ATTACHMENTS_VISIBLE, false);
        }
    }

    /** Called when the URL focus changes. */
    void onUrlFocusChange(boolean hasFocus) {
        mModel.set(NavigationAttachmentsProperties.TOOLBAR_VISIBLE, hasFocus);
        if (!hasFocus) {
            mPopup.dismiss();
        }
    }

    private void onToggleAttachmentsPopup() {
        if (mPopup.isShowing()) {
            mPopup.dismiss();
        } else {
            mPopup.show();
        }
    }

    private void launchCamera() {
        mPopup.dismiss();

        // Ask for a small-sized bitmap as a direct reply (passing no `EXTRA_OUTPUT` uri).
        // This should be sufficiently good, offering image of around 200-300px on the long edge.
        var i = new Intent(MediaStore.ACTION_IMAGE_CAPTURE);

        mWindowAndroid.showCancelableIntent(
                i,
                (resultCode, data) -> {
                    if (resultCode != Activity.RESULT_OK
                            || data == null
                            || data.getExtras() == null) {
                        return;
                    }

                    var bitmap = (Bitmap) data.getExtras().get("data");
                    if (bitmap == null) return;
                    addAttachment(new BitmapDrawable(mContext.getResources(), bitmap));
                },
                R.string.low_memory_error);
    }

    private void launchImagePicker() {
        mPopup.dismiss();

        var i =
                new Intent(Intent.ACTION_GET_CONTENT)
                        .addCategory(Intent.CATEGORY_OPENABLE)
                        .putExtra(Intent.EXTRA_ALLOW_MULTIPLE, true)
                        .setType("image/*");

        mWindowAndroid.showCancelableIntent(
                i,
                (resultCode, data) -> {
                    if (resultCode != Activity.RESULT_OK || data == null) return;

                    var uris = extractUrisFromResult(data);
                    for (var unused : uris) {
                        addAttachment(null);
                    }
                },
                R.string.low_memory_error);
    }

    private void launchFilePicker() {
        mPopup.dismiss();

        var i =
                new Intent(Intent.ACTION_OPEN_DOCUMENT)
                        .addCategory(Intent.CATEGORY_OPENABLE)
                        .setType("*/*")
                        .putExtra(Intent.EXTRA_ALLOW_MULTIPLE, true)
                        .addFlags(
                                Intent.FLAG_GRANT_READ_URI_PERMISSION
                                        | Intent.FLAG_GRANT_PERSISTABLE_URI_PERMISSION);

        mWindowAndroid.showCancelableIntent(
                i,
                (resultCode, data) -> {
                    if (resultCode != Activity.RESULT_OK || data == null) return;

                    var uris = extractUrisFromResult(data);
                    for (var unused : uris) {
                        addAttachment(null);
                    }
                },
                /* errorId= */ android.R.string.cancel);
    }

    /* package */ void addAttachment(@Nullable Drawable thumbnail) {
        mModel.set(NavigationAttachmentsProperties.ATTACHMENTS_VISIBLE, true);
        PropertyModel model =
                new PropertyModel.Builder(NavigationAttachmentItemProperties.ALL_KEYS)
                        .with(
                                NavigationAttachmentItemProperties.THUMBNAIL,
                                thumbnail != null ? thumbnail : mFallbackDrawable)
                        .with(NavigationAttachmentItemProperties.TITLE, "Attachment")
                        .with(NavigationAttachmentItemProperties.DESCRIPTION, "Description")
                        .build();
        mModelList.add(
                new SimpleRecyclerViewAdapter.ListItem(
                        NavigationAttachmentsRecyclerViewAdapter.NavigationAttachmentItemType
                                .ATTACHMENT_ITEM,
                        model));
    }

    // Parse GET_CONTENT response, extracting single- or multiple image selections.
    private static List<Uri> extractUrisFromResult(Intent data) {
        List<Uri> out = new ArrayList<>();
        Uri single = data.getData();
        if (single != null) out.add(single);

        ClipData clip = data.getClipData();
        if (clip == null) return out;

        for (int i = 0; i < clip.getItemCount(); i++) {
            Uri u = clip.getItemAt(i).getUri();
            if (u != null) out.add(u);
        }
        return out;
    }
}
