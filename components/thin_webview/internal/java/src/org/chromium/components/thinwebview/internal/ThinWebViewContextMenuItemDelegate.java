// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.components.thinwebview.internal;

import org.chromium.build.annotations.NullMarked;
import org.chromium.components.embedder_support.contextmenu.ContextMenuItemDelegate;
import org.chromium.content_public.browser.WebContents;
import org.chromium.ui.base.Clipboard;
import org.chromium.url.GURL;

/** Handles the context menu item functionality in WebView. */
@NullMarked
public class ThinWebViewContextMenuItemDelegate implements ContextMenuItemDelegate {
    private final WebContents mWebContents;

    /** Builds a {@link ThinWebViewContextMenuItemDelegate} instance. */
    public ThinWebViewContextMenuItemDelegate(WebContents webContents) {
        mWebContents = webContents;
    }

    @Override
    public void onDestroy() {}

    @Override
    public String getPageTitle() {
        return mWebContents.getTitle();
    }

    @Override
    public WebContents getWebContents() {
        return mWebContents;
    }

    @Override
    public void onSaveToClipboard(String text, int clipboardType) {
        Clipboard.getInstance().setText(text);
    }

    @Override
    public void onOpenInDefaultBrowser(GURL url) {
        // Intentionally empty.
    }

    @Override
    public GURL getPageUrl() {
        return mWebContents.getVisibleUrl();
    }
}
