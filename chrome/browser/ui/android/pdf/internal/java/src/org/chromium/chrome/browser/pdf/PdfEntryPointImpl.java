// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.pdf;

import android.app.Activity;
import android.view.View;

import org.chromium.build.annotations.NullMarked;
import org.chromium.build.annotations.Nullable;
import org.chromium.chrome.browser.profiles.Profile;
import org.chromium.chrome.browser.ui.native_page.NativePageHost;

import java.util.List;

/** Implementation of PdfEntryPoint. */
@NullMarked
public class PdfEntryPointImpl implements PdfEntryPoint {
    @Override
    public PdfCoordinatorInterface createPdfCoordinator(
            Object host,
            Object profile,
            Activity activity,
            String url,
            @Nullable String filepath,
            String title,
            int tabId,
            List<View> pdfFragmentViews) {
        return new PdfCoordinator(
                (NativePageHost) host,
                (Profile) profile,
                activity,
                filepath,
                title,
                tabId,
                url,
                pdfFragmentViews);
    }
}
