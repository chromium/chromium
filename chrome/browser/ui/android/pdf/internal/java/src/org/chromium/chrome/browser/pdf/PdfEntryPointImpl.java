// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.pdf;

import static org.chromium.build.NullUtil.assertNonNull;

import android.app.Activity;
import android.view.View;

import androidx.fragment.app.Fragment;
import androidx.fragment.app.FragmentActivity;
import androidx.pdf.viewer.fragment.PdfViewerFragment;

import org.chromium.build.annotations.NullMarked;
import org.chromium.build.annotations.Nullable;
import org.chromium.chrome.browser.profiles.Profile;
import org.chromium.chrome.browser.ui.native_page.NativePageHost;

import java.util.ArrayList;
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
            PdfFragmentViewTracker pdfFragmentViewTracker) {
        return new PdfCoordinator(
                (NativePageHost) host,
                (Profile) profile,
                activity,
                filepath,
                title,
                tabId,
                url,
                pdfFragmentViewTracker);
    }

    @Override
    public List<View> findAllPdfFragmentViews(FragmentActivity activity) {
        List<View> allViews = new ArrayList<>();
        var fragmentManager = activity.getSupportFragmentManager();
        for (Fragment fragment : fragmentManager.getFragments()) {
            if (fragment instanceof PdfViewerFragment) {
                allViews.add(assertNonNull(fragment.getView()));
            }
        }
        return allViews;
    }
}
