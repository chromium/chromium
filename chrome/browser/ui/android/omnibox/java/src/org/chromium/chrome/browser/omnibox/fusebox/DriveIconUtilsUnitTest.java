// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.omnibox.fusebox;

import static org.junit.Assert.assertEquals;

import org.junit.Test;
import org.junit.runner.RunWith;

import org.chromium.base.test.BaseRobolectricTestRunner;
import org.chromium.chrome.browser.omnibox.R;

/** Unit tests for {@link DriveIconUtils}. */
@RunWith(BaseRobolectricTestRunner.class)
public class DriveIconUtilsUnitTest {
    @Test
    public void mimeTypeMapping() {
        assertEquals(
                R.drawable.ic_drive_docs_24dp,
                DriveIconUtils.getIconForDriveFile(DriveIconUtils.MIME_TYPE_GOOGLE_DOCS, null));
        assertEquals(
                R.drawable.ic_drive_sheets_24dp,
                DriveIconUtils.getIconForDriveFile(DriveIconUtils.MIME_TYPE_GOOGLE_SHEETS, null));
        assertEquals(
                R.drawable.ic_drive_slides_24dp,
                DriveIconUtils.getIconForDriveFile(DriveIconUtils.MIME_TYPE_GOOGLE_SLIDES, null));
        assertEquals(
                R.drawable.ic_attach_pdf_24dp,
                DriveIconUtils.getIconForDriveFile("application/pdf", null));
        assertEquals(
                R.drawable.ic_drive_image_colored_24dp,
                DriveIconUtils.getIconForDriveFile("image/png", null));
        assertEquals(
                R.drawable.ic_drive_video_colored_24dp,
                DriveIconUtils.getIconForDriveFile("video/mp4", null));
        assertEquals(
                R.drawable.ic_attach_file_24dp,
                DriveIconUtils.getIconForDriveFile("application/vnd.google-apps.form", null));
    }

    @Test
    public void fileNameFallbackMapping() {
        assertEquals(
                R.drawable.ic_attach_pdf_24dp,
                DriveIconUtils.getIconForDriveFile(null, "file.pdf"));
        assertEquals(
                R.drawable.ic_drive_image_colored_24dp,
                DriveIconUtils.getIconForDriveFile(null, "photo.jpg"));
        assertEquals(
                R.drawable.ic_drive_video_colored_24dp,
                DriveIconUtils.getIconForDriveFile(null, "movie.mp4"));
    }

    @Test
    public void unknownTypeFallbackToAttachFile() {
        assertEquals(
                R.drawable.ic_attach_file_24dp,
                DriveIconUtils.getIconForDriveFile("application/octet-stream", "file.xyz"));
        assertEquals(
                R.drawable.ic_attach_file_24dp,
                DriveIconUtils.getIconForDriveFile(null, "document.docx"));
        assertEquals(
                R.drawable.ic_attach_file_24dp, DriveIconUtils.getIconForDriveFile(null, null));
    }
}
