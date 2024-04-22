// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.fakepdf;

import androidx.annotation.NonNull;
import androidx.fragment.app.Fragment;
import androidx.fragment.app.FragmentManager;

/** Fake PDF viewer API. Will be removed once real APIs become available. */
public class PdfViewerFragment extends Fragment {
    public PdfViewerFragment() {}

    public void loadRequest(
            @NonNull PdfDocumentRequest request, @NonNull PdfDocumentListener documentListener) {}

    public void show(
            @NonNull FragmentManager manager, @NonNull String tag, @NonNull int containerViewId) {}
}
