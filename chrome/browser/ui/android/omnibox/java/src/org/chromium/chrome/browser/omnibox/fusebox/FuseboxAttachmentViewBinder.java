// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.omnibox.fusebox;

import android.text.TextUtils;
import android.view.View;
import android.view.ViewGroup;
import android.widget.ImageView;
import android.widget.TextView;

import org.chromium.build.annotations.NullMarked;
import org.chromium.chrome.browser.omnibox.R;
import org.chromium.chrome.browser.omnibox.styles.OmniboxResourceProvider;
import org.chromium.ui.modelutil.PropertyKey;
import org.chromium.ui.modelutil.PropertyModel;

/** Binds the Fusebox Attachment Item properties to the view. */
@NullMarked
class FuseboxAttachmentViewBinder {
    /**
     * @see PropertyModelChangeProcessor.ViewBinder#bind(Object, Object, Object)
     */
    public static void bind(PropertyModel model, View view, PropertyKey propertyKey) {
        if (propertyKey == FuseboxAttachmentProperties.ATTACHMENT) {
            FuseboxAttachment attachment = model.get(FuseboxAttachmentProperties.ATTACHMENT);
            assert attachment != null : "FuseboxAttachment cannot be null";
            updateViewForUploadState(attachment, view);
        } else if (propertyKey == FuseboxAttachmentProperties.ON_REMOVE) {
            view.findViewById(R.id.attachment_remove_button)
                    .setOnClickListener(
                            v -> model.get(FuseboxAttachmentProperties.ON_REMOVE).run());
        }
    }

    private static void updateViewForUploadState(FuseboxAttachment attachment, View view) {
        View progressView = view.findViewById(R.id.attachment_spinner);
        ImageView imageView = view.findViewById(R.id.attachment_thumbnail);
        ViewGroup.LayoutParams layoutParams = view.getLayoutParams();
        if (attachment.isUploadComplete()) {
            progressView.setVisibility(View.GONE);
            imageView.setVisibility(View.VISIBLE);
            imageView.setImageDrawable(
                    attachment.thumbnail != null
                            ? attachment.thumbnail
                            : OmniboxResourceProvider.getDrawable(
                                    view.getContext(), R.drawable.ic_attach_file_24dp));
            applyTitleAndDescriptionIfPresent(attachment, view);
        } else {
            progressView.setVisibility(View.VISIBLE);
            imageView.setVisibility(View.GONE);
            TextView titleView = view.findViewById(R.id.attachment_title);
            if (titleView != null) {
                titleView.setVisibility(View.GONE);
            }
        }
        view.setLayoutParams(layoutParams);
    }

    private static void applyTitleAndDescriptionIfPresent(FuseboxAttachment attachment, View view) {
        TextView titleView = view.findViewById(R.id.attachment_title);
        if (titleView == null) return;

        if (TextUtils.isEmpty(attachment.title)) {
            titleView.setVisibility(View.GONE);
        } else {
            titleView.setVisibility(View.VISIBLE);
            titleView.setText(attachment.title);
        }
    }
}
