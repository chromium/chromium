// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.pdf;

import android.net.Uri;
import android.view.View;

import org.chromium.build.annotations.NullMarked;
import org.chromium.build.annotations.Nullable;

/** Interface for the PDF coordinator in the DFM. */
@NullMarked
public interface PdfCoordinatorInterface {
    /** Returns the intended view for PdfPage tab. */
    View getView();

    /** Returns the filepath of the pdf document. */
    @Nullable String getFilepath();

    /** Called after a pdf page has been removed from the view hierarchy. */
    void destroy();

    /** Reloads the pdf document. */
    void reload();

    /** Returns whether there are any unsaved or applied changes. */
    boolean hasChanges();

    /** Shows the reload confirmation dialog and runs onConfirm if confirmed. */
    void showReloadConfirmationDialog(Runnable onConfirm);

    /** Called after pdf download complete. */
    void onDownloadComplete(String pdfFilePath, String pdfFileName);

    /** Show pdf specific find in page UI. */
    boolean findInPage();

    /** Retrieve uri of the pdf document. */
    @Nullable Uri getUri();

    /** Reset PDF load state so that the page can be reloaded/refreshed. */
    void resetLoadState();

    /** Build structured data including content uri and grant permission. */
    @Nullable String requestAssistContent(String filename, boolean isWorkProfile);

    /** Retrieve uri of the pdf document and grant permission to the target package. */
    @Nullable Uri getFileUri(boolean isWorkProfile, @Nullable String targetPackage);

    /**
     * Changes the zoom level of the PDF page.
     *
     * @param decrease Whether to decrease the zoom level.
     * @return True if the PDF page can be zoomed out, false otherwise.
     */
    boolean changeZoomLevel(boolean decrease);

    /**
     * Resets the zoom level of the PDF page to the default zoom level.
     *
     * @return True if the PDF page was zoomed to the default zoom level, false otherwise.
     */
    boolean resetZoomLevel();
}
