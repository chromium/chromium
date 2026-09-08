// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.pdf;

import android.net.Uri;
import android.os.ParcelFileDescriptor;

import androidx.pdf.view.PdfView;

import org.chromium.build.annotations.NullMarked;
import org.chromium.build.annotations.Nullable;

import java.io.File;

/** Interface to handle actions from the PDF viewer. */
@NullMarked
public interface PdfActionsDelegate {
    /**
     * Called when a link in the PDF is clicked.
     *
     * @param uri The uri of the link that was clicked.
     * @return True if the navigation was initiated, false if the link had a disallowed scheme and
     *     was dropped.
     */
    boolean onLinkClicked(Uri uri);

    /**
     * Called when the PDF document is successfully loaded.
     *
     * @param pageCount The total page count.
     */
    void onDocumentLoaded(int pageCount);

    /**
     * Called when the PDF page is changed.
     *
     * @param pageIndex The 0-based index of the current page.
     * @param zoomLevel The current zoom level.
     */
    void onViewportChanged(int pageIndex, float zoomLevel);

    /**
     * Loads the PdfSelectionCoordinator.
     *
     * @param pdfView The PdfView to use for the coordinator.
     */
    void loadPdfSelectionCoordinator(PdfView pdfView);

    /** Called when the PDF document fails to load. */
    void onDocumentLoadFailed();

    /** Called when the edit mode changes. */
    void onEditModeChanged(boolean editMode);

    /** Returns whether the page navigation and edit button are visible in the top toolbar. */
    boolean isPageNavAndEditVisible();

    /** Returns the URI of the PDF document. */
    @Nullable Uri getUri();

    /** Returns whether the current PDF is loaded in Incognito mode. */
    boolean isIncognito();

    /** Called when edits are successfully applied to the PDF. */
    void onEditsApplied();

    /**
     * Called when edits are saved.
     *
     * @param tempFile The temporary file containing the saved PDF content (null in Incognito).
     * @param pfd The ParcelFileDescriptor containing the saved PDF content in memory (null in
     *     non-Incognito).
     * @param onDone The callback to execute when the update has completed.
     */
    void onPdfEditsSaved(
            @Nullable File tempFile, @Nullable ParcelFileDescriptor pfd, Runnable onDone);

    /** Called when saving edits to a file failed. */
    void onPdfEditsSaveFailed();
}


