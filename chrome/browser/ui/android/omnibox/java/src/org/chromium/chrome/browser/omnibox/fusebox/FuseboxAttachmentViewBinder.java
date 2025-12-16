// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.omnibox.fusebox;

import static org.chromium.build.NullUtil.assumeNonNull;

import android.content.Context;
import android.graphics.Bitmap;
import android.graphics.drawable.Drawable;
import android.text.TextUtils;
import android.view.View;
import android.view.ViewGroup;
import android.widget.ImageView;
import android.widget.TextView;

import org.chromium.build.annotations.NullMarked;
import org.chromium.chrome.browser.omnibox.R;
import org.chromium.chrome.browser.omnibox.fusebox.FuseboxAttachmentRecyclerViewAdapter.FuseboxAttachmentType;
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
            imageView.setImageDrawable(getThumbnailDrawable(attachment, view.getContext()));
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

    static Drawable getThumbnailDrawable(FuseboxAttachment attachment, Context context) {
        switch (attachment.type) {
            case FuseboxAttachmentType.ATTACHMENT_IMAGE:
            case FuseboxAttachmentType.ATTACHMENT_FILE:
                if (attachment.thumbnail != null) {
                    return attachment.thumbnail;
                }
                break;
            case FuseboxAttachmentType.ATTACHMENT_TAB:
                Bitmap favicon =
                        OmniboxResourceProvider.getFaviconBitmapForTab(
                                assumeNonNull(attachment.tab));
                return FuseboxTabUtils.getDrawableForTabFavicon(
                        context,
                        favicon,
                        context.getResources()
                                .getDimensionPixelSize(R.dimen.fusebox_attachment_visible_height));
        }
        return OmniboxResourceProvider.getDrawable(context, R.drawable.ic_attach_file_24dp);
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
