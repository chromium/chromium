// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.pdf;

import android.net.Uri;

import org.chromium.build.annotations.NullMarked;

/** Interface to handle actions from the PDF viewer. */
@NullMarked
public interface PdfActionsDelegate {
    /**
     * Called when a link in the PDF is clicked.
     *
     * @param uri The uri of the link that was clicked.
     * @return True if the link was handled, false otherwise.
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
}
