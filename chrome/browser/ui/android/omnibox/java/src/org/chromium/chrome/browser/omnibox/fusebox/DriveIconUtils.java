// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.omnibox.fusebox;

import android.content.res.Resources;

import androidx.annotation.DrawableRes;

import org.chromium.build.annotations.NullMarked;
import org.chromium.build.annotations.Nullable;
import org.chromium.chrome.browser.omnibox.R;
import org.chromium.ui.base.MimeTypeUtils;

import java.util.Locale;

/** Utility for mapping Drive file MIME types and extensions to drawable resources. */
@NullMarked
public final class DriveIconUtils {
    public static final String MIME_TYPE_GOOGLE_DOCS = "application/vnd.google-apps.document";
    public static final String MIME_TYPE_GOOGLE_SHEETS = "application/vnd.google-apps.spreadsheet";
    public static final String MIME_TYPE_GOOGLE_SLIDES = "application/vnd.google-apps.presentation";

    private DriveIconUtils() {}

    /**
     * Resolves the appropriate vector drawable icon for a file based on its MIME type and name.
     *
     * @param mimeType MIME type of the file, if known.
     * @param fileName File name or title, used for extension fallback.
     * @return Drawable resource ID for the file icon.
     */
    public static @DrawableRes int getIconForDriveFile(
            @Nullable String mimeType, @Nullable String fileName) {
        if (mimeType != null) {
            int icon = getIconFromMimeType(mimeType);
            if (icon != Resources.ID_NULL) return icon;
        }

        if (fileName != null) {
            int icon = getIconFromFileName(fileName);
            if (icon != Resources.ID_NULL) return icon;
        }

        return R.drawable.ic_attach_file_24dp;
    }

    private static @DrawableRes int getIconFromMimeType(String mimeType) {
        return switch (mimeType) {
            case MIME_TYPE_GOOGLE_DOCS -> R.drawable.ic_drive_docs_24dp;
            case MIME_TYPE_GOOGLE_SHEETS -> R.drawable.ic_drive_sheets_24dp;
            case MIME_TYPE_GOOGLE_SLIDES -> R.drawable.ic_drive_slides_24dp;
            default -> getIconFromStandardMimeType(mimeType);
        };
    }

    private static @DrawableRes int getIconFromStandardMimeType(String mimeType) {
        return switch (MimeTypeUtils.getTypeFromMimeType(mimeType)) {
            case MimeTypeUtils.Type.IMAGE -> R.drawable.ic_drive_image_colored_24dp;
            case MimeTypeUtils.Type.VIDEO -> R.drawable.ic_drive_video_colored_24dp;
            case MimeTypeUtils.Type.PDF -> R.drawable.ic_attach_pdf_24dp;
            default -> Resources.ID_NULL;
        };
    }

    private static @DrawableRes int getIconFromFileName(String fileName) {
        String lower = fileName.toLowerCase(Locale.ROOT);
        int dotIndex = lower.lastIndexOf('.');
        if (dotIndex == -1 || dotIndex == lower.length() - 1) {
            return Resources.ID_NULL;
        }
        String ext = lower.substring(dotIndex + 1);
        return switch (ext) {
            case "pdf" -> R.drawable.ic_attach_pdf_24dp;
            case "jpg", "jpeg", "png", "gif", "webp", "bmp", "svg" ->
                    R.drawable.ic_drive_image_colored_24dp;
            case "mp4", "mov", "avi", "mkv", "webm", "3gp" ->
                    R.drawable.ic_drive_video_colored_24dp;
            default -> Resources.ID_NULL;
        };
    }
}
