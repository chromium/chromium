// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.pdf;

import android.view.ViewGroup;

import org.chromium.build.annotations.NullMarked;

/**
 * Manages and tracks the View of existing PdfFragments. The Views are often placed to a wrong
 * Fragment container view after activity restart. The tracker keeps the list of such Views, and
 * places them to the right one using the Views' tag.
 */
@NullMarked
public interface PdfFragmentViewTracker {

    /**
     * Looks through the given Pdf fragment container and the restored Pdf Fragments to move the
     * misplaced views to the right container.
     *
     * @param container Container view where the PdfFragment Views are placed.
     * @param tag Tag used as ID to match the container with the Pdf Fragment view.
     */
    void maybeRelocateViews(ViewGroup container, String tag);
}
