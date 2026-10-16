// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.omnibox.fusebox;

import static org.chromium.build.NullUtil.assumeNonNull;

import android.content.Context;
import android.content.res.ColorStateList;
import android.graphics.Bitmap;
import android.graphics.drawable.Drawable;
import android.graphics.drawable.RippleDrawable;
import android.view.View;
import android.view.ViewGroup;
import android.widget.ImageButton;
import android.widget.ImageView;
import android.widget.ProgressBar;
import android.widget.TextView;

import androidx.annotation.ColorInt;

import org.chromium.build.annotations.NullMarked;
import org.chromium.build.annotations.Nullable;
import org.chromium.chrome.browser.omnibox.R;
import org.chromium.chrome.browser.omnibox.fusebox.FuseboxAttachmentRecyclerViewAdapter.FuseboxAttachmentType;
import org.chromium.chrome.browser.omnibox.styles.OmniboxResourceProvider;
import org.chromium.ui.modelutil.PropertyKey;
import org.chromium.ui.modelutil.PropertyModel;
import org.chromium.ui.util.ColorUtils;

/** Binds the Fusebox Attachment Item properties to the view. */
@NullMarked
class FuseboxAttachmentViewBinder {
    private final OmniboxResourceProvider mResourceProvider;

    public FuseboxAttachmentViewBinder(OmniboxResourceProvider resourceProvider) {
        mResourceProvider = resourceProvider;
    }

    /**
     * @see PropertyModelChangeProcessor.ViewBinder#bind(Object, Object, Object)
     */
    public void bind(PropertyModel model, View view, PropertyKey propertyKey) {
        if (propertyKey == FuseboxAttachmentProperties.ATTACHMENT) {
            FuseboxAttachment attachment = model.get(FuseboxAttachmentProperties.ATTACHMENT);
            assert attachment != null : "FuseboxAttachment cannot be null";
            updateViewForUploadState(attachment, view);
        } else if (propertyKey == FuseboxAttachmentProperties.COLOR_SCHEME) {
            adjustColorsForScheme(view);
            FuseboxAttachment attachment = model.get(FuseboxAttachmentProperties.ATTACHMENT);
            if (attachment != null) {
                updateViewForUploadState(attachment, view);
            }
        } else if (propertyKey == FuseboxAttachmentProperties.ON_REMOVE) {
            view.findViewById(R.id.attachment_remove_button)
                    .setOnClickListener(
                            v -> model.get(FuseboxAttachmentProperties.ON_REMOVE).run());
        } else if (propertyKey == FuseboxAttachmentProperties.REMOVE_BUTTON_SELECTED) {
            view.findViewById(R.id.attachment_remove_button)
                    .setSelected(model.get(FuseboxAttachmentProperties.REMOVE_BUTTON_SELECTED));
        }
    }

    private void updateViewForUploadState(FuseboxAttachment attachment, View view) {
        View progressView = view.findViewById(R.id.attachment_spinner);
        ImageView imageView = view.findViewById(R.id.attachment_thumbnail);
        ViewGroup.LayoutParams layoutParams = view.getLayoutParams();
        if (attachment.isUploadComplete() || attachment.isSuggestedTab) {
            progressView.setVisibility(View.GONE);
            imageView.setVisibility(View.VISIBLE);
            imageView.setImageDrawable(getThumbnailDrawable(attachment, view.getContext()));
            applyTitleAndDescriptionIfPresent(attachment, view);
        } else {
            progressView.setVisibility(View.VISIBLE);
            imageView.setVisibility(View.INVISIBLE);
            TextView titleView = view.findViewById(R.id.attachment_title);
            if (titleView != null) {
                titleView.setText(attachment.title);
                titleView.setVisibility(View.INVISIBLE);
            }
        }
        view.setLayoutParams(layoutParams);
    }

    @Nullable Drawable getThumbnailDrawable(FuseboxAttachment attachment, Context context) {
        return switch (attachment.type) {
            case FuseboxAttachmentType.ATTACHMENT_IMAGE -> imageThumbnail(attachment);
            case FuseboxAttachmentType.ATTACHMENT_IMAGE_NO_THUMBNAIL -> imageFallbackThumbnail();
            case FuseboxAttachmentType.ATTACHMENT_FILE -> fileThumbnail();
            case FuseboxAttachmentType.ATTACHMENT_PDF -> pdfThumbnail();
            case FuseboxAttachmentType.ATTACHMENT_DRIVE -> driveThumbnail(attachment);
            case FuseboxAttachmentType.ATTACHMENT_TAB -> tabThumbnail(context, attachment);
            default -> null;
        };
    }

    private static @Nullable Drawable driveThumbnail(FuseboxAttachment attachment) {
        return attachment.thumbnail;
    }

    private Drawable imageFallbackThumbnail() {
        Drawable fileIcon = mResourceProvider.getDrawable(R.drawable.ic_attach_image_24dp);
        fileIcon.setTint(mResourceProvider.getDefaultIconColor());
        return fileIcon;
    }

    private static @Nullable Drawable imageThumbnail(FuseboxAttachment attachment) {
        if (attachment.thumbnail != null) {
            return attachment.thumbnail;
        }
        return null;
    }

    private Drawable fileThumbnail() {
        Drawable fileIcon = mResourceProvider.getDrawable(R.drawable.ic_attach_file_24dp);
        fileIcon.setTint(mResourceProvider.getDefaultIconColor());
        return fileIcon;
    }

    private Drawable pdfThumbnail() {
        return mResourceProvider.getDrawable(R.drawable.ic_attach_pdf_24dp);
    }

    private Drawable tabThumbnail(Context context, FuseboxAttachment attachment) {
        Bitmap favicon = mResourceProvider.getTabFavicon(assumeNonNull(attachment.tab));
        Drawable drawable =
                FuseboxTabUtils.getDrawableForTabFavicon(
                        context,
                        favicon,
                        context.getResources()
                                .getDimensionPixelSize(R.dimen.fusebox_attachment_visible_height));
        // Only the fallback needs to be tinted, website favicons should be unchanged.
        if (favicon == null) {
            drawable.setTint(mResourceProvider.getDefaultIconColor());
        }
        return drawable;
    }

    private static void applyTitleAndDescriptionIfPresent(FuseboxAttachment attachment, View view) {
        TextView titleView = view.findViewById(R.id.attachment_title);
        if (titleView == null) return;

        titleView.setVisibility(View.VISIBLE);
        titleView.setText(attachment.title);
    }

    private void adjustColorsForScheme(View view) {
        Context context = view.getContext();
        ProgressBar progressView = view.findViewById(R.id.attachment_spinner);
        progressView.setIndeterminateTintList(
                ColorStateList.valueOf(mResourceProvider.getColorPrimary()));

        View backgroundView = view.findViewById(R.id.attachment_background);
        backgroundView.getBackground().setTint(mResourceProvider.getRequestTypeButtonColor());

        TextView titleView = view.findViewById(R.id.attachment_title);
        if (titleView != null) {
            titleView.setTextAppearance(mResourceProvider.getRequestTypeButtonTextRes());
        }
        ImageButton closeButton = view.findViewById(R.id.attachment_remove_button);
        closeButton.setColorFilter(mResourceProvider.getColorOnSurface());
        @ColorInt int colorSurface = mResourceProvider.getColorSurface();
        @ColorInt int closeBgColor = ColorUtils.setAlphaComponentWithFloat(colorSurface, 0.64f);
        Drawable closeBg = closeButton.getBackground();
        closeBg.setTint(closeBgColor);
        if (closeBg instanceof RippleDrawable ripple) {
            ripple.setRadius(
                    context.getResources()
                            .getDimensionPixelSize(
                                    R.dimen.fusebox_attachment_remove_highlight_inset));
        }
    }
}
