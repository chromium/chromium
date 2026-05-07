// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.pdf;

import android.app.Activity;

import org.chromium.build.annotations.NullMarked;
import org.chromium.components.module_installer.builder.ModuleInterface;

/** Interface for the PDF viewer module. */
@ModuleInterface(module = "on_demand", impl = "org.chromium.chrome.browser.pdf.PdfEntryPointImpl")
@NullMarked
public interface PdfEntryPoint {
    PdfCoordinatorInterface createPdfCoordinator(
            Object host,
            Object profile,
            Activity activity,
            String url,
            @org.chromium.build.annotations.Nullable String filepath,
            String title,
            int tabId);
}
