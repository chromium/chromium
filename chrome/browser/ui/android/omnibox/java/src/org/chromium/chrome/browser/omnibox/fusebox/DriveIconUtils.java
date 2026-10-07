// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.omnibox.fusebox;

import androidx.annotation.DrawableRes;

import org.chromium.build.annotations.NullMarked;
import org.chromium.build.annotations.Nullable;
import org.chromium.chrome.browser.omnibox.R;
import org.chromium.chrome.browser.omnibox.fusebox.FuseboxMetrics.DriveDocumentType;
import org.chromium.ui.base.MimeTypeUtils;

import java.util.Locale;

/** Utility for classifying Drive files by MIME type or extension and mapping them to icons. */
@NullMarked
public final class DriveIconUtils {
    public static final String MIME_TYPE_GOOGLE_DOCS = "application/vnd.google-apps.document";
    public static final String MIME_TYPE_GOOGLE_SHEETS = "application/vnd.google-apps.spreadsheet";
    public static final String MIME_TYPE_GOOGLE_SLIDES = "application/vnd.google-apps.presentation";

    private DriveIconUtils() {}

    /**
     * Classifies a Drive file based on its MIME type, falling back to the file name extension.
     *
     * @param mimeType MIME type of the file, if known.
     * @param fileName File name or title, used for extension fallback.
     * @return The {@link DriveDocumentType} of the file.
     */
    public static @DriveDocumentType int getDriveDocumentType(
            @Nullable String mimeType, @Nullable String fileName) {
        if (mimeType != null) {
            int type = getTypeFromMimeType(mimeType);
            if (type != DriveDocumentType.OTHER) return type;
        }
        if (fileName == null) return DriveDocumentType.OTHER;
        return getTypeFromFileName(fileName);
    }

    /**
     * Resolves the appropriate vector drawable icon for a file based on its MIME type and name.
     *
     * @param mimeType MIME type of the file, if known.
     * @param fileName File name or title, used for extension fallback.
     * @return Drawable resource ID for the file icon.
     */
    public static @DrawableRes int getIconForDriveFile(
            @Nullable String mimeType, @Nullable String fileName) {
        return switch (getDriveDocumentType(mimeType, fileName)) {
            case DriveDocumentType.DOCS -> R.drawable.ic_drive_docs_24dp;
            case DriveDocumentType.SHEETS -> R.drawable.ic_drive_sheets_24dp;
            case DriveDocumentType.SLIDES -> R.drawable.ic_drive_slides_24dp;
            case DriveDocumentType.PDF -> R.drawable.ic_attach_pdf_24dp;
            case DriveDocumentType.IMAGE -> R.drawable.ic_drive_image_colored_24dp;
            case DriveDocumentType.VIDEO -> R.drawable.ic_drive_video_colored_24dp;
            default -> R.drawable.ic_attach_file_24dp;
        };
    }

    private static @DriveDocumentType int getTypeFromMimeType(String mimeType) {
        return switch (mimeType) {
            case MIME_TYPE_GOOGLE_DOCS -> DriveDocumentType.DOCS;
            case MIME_TYPE_GOOGLE_SHEETS -> DriveDocumentType.SHEETS;
            case MIME_TYPE_GOOGLE_SLIDES -> DriveDocumentType.SLIDES;
            default -> getTypeFromStandardMimeType(mimeType);
        };
    }

    private static @DriveDocumentType int getTypeFromStandardMimeType(String mimeType) {
        return switch (MimeTypeUtils.getTypeFromMimeType(mimeType)) {
            case MimeTypeUtils.Type.IMAGE -> DriveDocumentType.IMAGE;
            case MimeTypeUtils.Type.VIDEO -> DriveDocumentType.VIDEO;
            case MimeTypeUtils.Type.PDF -> DriveDocumentType.PDF;
            default -> DriveDocumentType.OTHER;
        };
    }

    private static @DriveDocumentType int getTypeFromFileName(String fileName) {
        String lower = fileName.toLowerCase(Locale.ROOT);
        int dotIndex = lower.lastIndexOf('.');
        if (dotIndex == -1 || dotIndex == lower.length() - 1) {
            return DriveDocumentType.OTHER;
        }
        String ext = lower.substring(dotIndex + 1);
        return switch (ext) {
            case "pdf" -> DriveDocumentType.PDF;
            case "jpg", "jpeg", "png", "gif", "webp", "bmp", "svg" -> DriveDocumentType.IMAGE;
            case "mp4", "mov", "avi", "mkv", "webm", "3gp" -> DriveDocumentType.VIDEO;
            default -> DriveDocumentType.OTHER;
        };
    }
}
