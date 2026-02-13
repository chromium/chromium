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
}
