// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.pdf;

import android.app.Activity;
import android.view.View;

import androidx.fragment.app.FragmentActivity;

import org.chromium.build.annotations.NullMarked;
import org.chromium.build.annotations.Nullable;

import java.util.List;

/** Interface for the PDF viewer module. */
@NullMarked
public interface PdfEntryPoint {
    PdfCoordinatorInterface createPdfCoordinator(
            Object host,
            Object profile,
            Activity activity,
            String url,
            @Nullable String filepath,
            String title,
            int tabId,
            PdfFragmentViewTracker pdfFragmentViewTracker);

    List<View> findAllPdfFragmentViews(FragmentActivity activity);
}
