// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.pdf;

import android.text.TextUtils;

import androidx.annotation.VisibleForTesting;

import org.chromium.base.UserData;
import org.chromium.base.UserDataHost;
import org.chromium.build.annotations.NullMarked;
import org.chromium.build.annotations.Nullable;
import org.chromium.chrome.browser.tab.EmptyTabObserver;
import org.chromium.chrome.browser.tab.Tab;
import org.chromium.ui.base.WindowAndroid;
import org.chromium.url.GURL;

/**
 * Manages the lifetime of PDF content streams for a Tab.
 *
 * <p>Design Philosophy: The PDF content stream (especially for Incognito mode where it is
 * memory-backed) should logically belong to the {@link Tab} session, not the transient {@link
 * PdfPage} view. When a tab is reparented (e.g., via Drag and Drop to a new window), the {@link
 * PdfPage} view is destroyed and recreated, but the {@link Tab} object persists. By binding the
 * stream's lifetime to the {@link Tab} via this helper, we ensure the PDF stream survives window
 * swaps without requiring arbitrary timers.
 *
 * <p>Cleanup is deterministically triggered when: 1. The tab is destroyed (permanently closed). 2.
 * The tab navigates away to a non-PDF page.
 */
@NullMarked
public class PdfTabHelper extends EmptyTabObserver implements UserData {
    private static final Class<PdfTabHelper> USER_DATA_KEY = PdfTabHelper.class;

    private final Tab mTab;
    private @Nullable String mPdfUrl;

    /** Retrieves the {@link PdfTabHelper} for the given tab, creating it if it doesn't exist. */
    public static PdfTabHelper from(Tab tab) {
        UserDataHost host = tab.getUserDataHost();
        PdfTabHelper helper = host.getUserData(USER_DATA_KEY);
        if (helper == null) {
            helper = host.setUserData(USER_DATA_KEY, new PdfTabHelper(tab));
        }
        return helper;
    }

    private PdfTabHelper(Tab tab) {
        mTab = tab;
        mTab.addObserver(this);
    }

    public void setPdfUrl(String pdfUrl) {
        mPdfUrl = pdfUrl;
        mTab.addObserver(this);
    }

    @Override
    public void onDestroyed(Tab tab) {
        cleanUp();
    }

    @Override
    public void onPageLoadStarted(Tab tab, GURL url) {
        // Do not clean up if the PDF URL hasn't been set yet or if the navigation is to the same
        // PDF.
        if (mPdfUrl == null || isSamePdf(url.getSpec())) {
            return;
        }
        cleanUp();
    }

    @Override
    public void onActivityAttachmentChanged(Tab tab, @Nullable WindowAndroid window) {
        // Intentionally do nothing to prevent automatic observer removal on detachment.
    }

    @VisibleForTesting
    boolean isSamePdf(String newUrl) {
        if (mPdfUrl == null) return false;
        if (TextUtils.equals(newUrl, mPdfUrl)) return true;

        String newDecoded = PdfUtils.decodePdfPageUrl(newUrl);
        String currentDecoded = PdfUtils.decodePdfPageUrl(mPdfUrl);

        String newPdfUrl = newDecoded != null ? newDecoded : newUrl;
        String currentPdfUrl = currentDecoded != null ? currentDecoded : mPdfUrl;

        return TextUtils.equals(newPdfUrl, currentPdfUrl);
    }

    @Override
    public void destroy() {
        cleanUp();
    }

    private void cleanUp() {
        mPdfUrl = null;
        mTab.removeObserver(this);
        PdfContentProvider.removeStreamsForTab(String.valueOf(mTab.getId()));
        UserDataHost host = mTab.getUserDataHost();
        if (host != null) {
            try {
                if (host.getUserData(USER_DATA_KEY) == this) {
                    host.removeUserData(USER_DATA_KEY);
                }
            } catch (IllegalStateException e) {
                // Host is already destroyed or key is already removed.
            }
        }
    }
}
