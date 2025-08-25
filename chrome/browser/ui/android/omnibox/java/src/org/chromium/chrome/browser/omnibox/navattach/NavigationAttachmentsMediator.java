// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.omnibox.navattach;

import android.app.Activity;
import android.content.ClipData;
import android.content.Intent;
import android.graphics.Bitmap;
import android.net.Uri;
import android.provider.MediaStore;
import android.util.Log;

import org.chromium.build.annotations.NullMarked;
import org.chromium.chrome.browser.omnibox.R;
import org.chromium.ui.base.WindowAndroid;
import org.chromium.ui.modelutil.MVCListAdapter.ModelList;
import org.chromium.ui.modelutil.PropertyModel;

import java.util.ArrayList;
import java.util.List;

/** Mediator for the Navigation Attachments component. */
@NullMarked
class NavigationAttachmentsMediator {
    private static final String TAG = "NavAttachMediator";

    private final WindowAndroid mWindowAndroid;
    private final PropertyModel mModel;
    private final NavigationAttachmentsPopup mPopup;

    NavigationAttachmentsMediator(
            WindowAndroid windowAndroid,
            PropertyModel model,
            NavigationAttachmentsViewHolder viewHolder,
            ModelList modelList) {
        mWindowAndroid = windowAndroid;
        mModel = model;
        mPopup = viewHolder.popup;

        mModel.set(
                NavigationAttachmentsProperties.BUTTON_ADD_CLICKED, this::onToggleAttachmentsPopup);
        mModel.set(NavigationAttachmentsProperties.POPUP_CAMERA_CLICKED, this::launchCamera);
        mModel.set(NavigationAttachmentsProperties.POPUP_GALLERY_CLICKED, this::launchImagePicker);
    }

    void destroy() {}

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
                    Log.i(
                            TAG,
                            String.format(
                                    "Photo Bitmap: %dx%d", bitmap.getWidth(), bitmap.getHeight()));
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
                    for (var uri : uris) {
                        Log.i(TAG, "Photo URI: " + uri);
                    }
                },
                R.string.low_memory_error);
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
