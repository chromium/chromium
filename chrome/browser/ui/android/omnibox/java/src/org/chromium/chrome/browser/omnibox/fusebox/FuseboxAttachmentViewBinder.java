// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.omnibox.fusebox;

import static org.chromium.build.NullUtil.assumeNonNull;

import android.content.Context;
import android.content.res.ColorStateList;
import android.graphics.Bitmap;
import android.graphics.drawable.Drawable;
import android.text.TextUtils;
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
import org.chromium.chrome.browser.ui.theme.BrandedColorScheme;
import org.chromium.ui.modelutil.PropertyKey;
import org.chromium.ui.modelutil.PropertyModel;
import org.chromium.ui.util.ColorUtils;

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
            updateViewForUploadState(model, attachment, view);
        } else if (propertyKey == FuseboxAttachmentProperties.COLOR_SCHEME) {
            adjustColorsForScheme(model, view);
            FuseboxAttachment attachment = model.get(FuseboxAttachmentProperties.ATTACHMENT);
            if (attachment != null) {
                updateViewForUploadState(model, attachment, view);
            }
        } else if (propertyKey == FuseboxAttachmentProperties.ON_REMOVE) {
            view.findViewById(R.id.attachment_remove_button)
                    .setOnClickListener(
                            v -> model.get(FuseboxAttachmentProperties.ON_REMOVE).run());
        }
    }

    private static void updateViewForUploadState(
            PropertyModel model, FuseboxAttachment attachment, View view) {
        View progressView = view.findViewById(R.id.attachment_spinner);
        ImageView imageView = view.findViewById(R.id.attachment_thumbnail);
        ViewGroup.LayoutParams layoutParams = view.getLayoutParams();
        if (attachment.isUploadComplete()) {
            progressView.setVisibility(View.GONE);
            imageView.setVisibility(View.VISIBLE);
            imageView.setImageDrawable(getThumbnailDrawable(model, attachment, view.getContext()));
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

    static @Nullable Drawable getThumbnailDrawable(
            PropertyModel model, FuseboxAttachment attachment, Context context) {

        @BrandedColorScheme
        int brandedColorScheme = model.get(FuseboxAttachmentProperties.COLOR_SCHEME);

        return switch (attachment.type) {
            case FuseboxAttachmentType.ATTACHMENT_IMAGE -> imageThumbnail(attachment);
            case FuseboxAttachmentType.ATTACHMENT_IMAGE_NO_THUMBNAIL ->
                    imageFallbackThumbnail(context, brandedColorScheme);
            case FuseboxAttachmentType.ATTACHMENT_FILE ->
                    fileThumbnail(context, brandedColorScheme);
            case FuseboxAttachmentType.ATTACHMENT_PDF -> pdfThumbnail(context);
            case FuseboxAttachmentType.ATTACHMENT_TAB ->
                    tabThumbnail(context, brandedColorScheme, attachment);
            default -> null;
        };
    }

    private static Drawable imageFallbackThumbnail(
            Context context, @BrandedColorScheme int brandedColorScheme) {
        Drawable fileIcon =
                OmniboxResourceProvider.getDrawable(context, R.drawable.ic_attach_image_24dp);
        fileIcon.setTint(OmniboxResourceProvider.getDefaultIconColor(context, brandedColorScheme));
        return fileIcon;
    }

    private static @Nullable Drawable imageThumbnail(FuseboxAttachment attachment) {
        if (attachment.thumbnail != null) {
            return attachment.thumbnail;
        }
        return null;
    }

    private static Drawable fileThumbnail(
            Context context, @BrandedColorScheme int brandedColorScheme) {
        Drawable fileIcon =
                OmniboxResourceProvider.getDrawable(context, R.drawable.ic_attach_file_24dp);
        fileIcon.setTint(OmniboxResourceProvider.getDefaultIconColor(context, brandedColorScheme));
        return fileIcon;
    }

    private static Drawable pdfThumbnail(Context context) {
        return OmniboxResourceProvider.getDrawable(context, R.drawable.ic_attach_pdf_24dp);
    }

    private static Drawable tabThumbnail(
            Context context,
            @BrandedColorScheme int brandedColorScheme,
            FuseboxAttachment attachment) {
        Bitmap favicon =
                OmniboxResourceProvider.getFaviconBitmapForTab(assumeNonNull(attachment.tab));
        Drawable drawable =
                FuseboxTabUtils.getDrawableForTabFavicon(
                        context,
                        favicon,
                        context.getResources()
                                .getDimensionPixelSize(R.dimen.fusebox_attachment_visible_height));
        // Only the fallback needs to be tinted, website favicons should be unchanged.
        if (favicon == null) {
            drawable.setTint(
                    OmniboxResourceProvider.getDefaultIconColor(context, brandedColorScheme));
        }
        return drawable;
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

    private static void adjustColorsForScheme(PropertyModel model, View view) {
        Context context = view.getContext();
        @BrandedColorScheme
        int brandedColorScheme = model.get(FuseboxAttachmentProperties.COLOR_SCHEME);
        ProgressBar progressView = view.findViewById(R.id.attachment_spinner);
        progressView.setIndeterminateTintList(
                ColorStateList.valueOf(
                        OmniboxResourceProvider.getColorPrimary(context, brandedColorScheme)));

        View backgroundView = view.findViewById(R.id.attachment_background);
        backgroundView
                .getBackground()
                .setTint(OmniboxResourceProvider.getAiModeButtonColor(context, brandedColorScheme));

        TextView titleView = view.findViewById(R.id.attachment_title);
        if (titleView != null) {
            titleView.setTextAppearance(
                    OmniboxResourceProvider.getImageGenButtonTextRes(brandedColorScheme));
        }
        ImageButton closeButton = view.findViewById(R.id.attachment_remove_button);
        closeButton.setColorFilter(
                OmniboxResourceProvider.getColorOnSurface(context, brandedColorScheme));
        @ColorInt
        int colorSurface = OmniboxResourceProvider.getColorSurface(context, brandedColorScheme);
        @ColorInt int closeBgColor = ColorUtils.setAlphaComponentWithFloat(colorSurface, 0.64f);
        closeButton.getBackground().setTint(closeBgColor);
    }
}
