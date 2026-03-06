// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.ui.side_panel_container.dev;

import org.chromium.build.annotations.NullMarked;
import org.chromium.build.annotations.Nullable;
import org.chromium.chrome.browser.ui.side_panel_container.SidePanelContent;
import org.chromium.components.thinwebview.ThinWebView;
import org.chromium.content_public.browser.WebContents;

/** A plain old data class holding all objects in the content of {@link SidePanelDevFeature}. */
@NullMarked
final class SidePanelDevFeatureContent {

    /**
     * Content for {@link
     * org.chromium.chrome.browser.ui.side_panel_container.SidePanelContainerCoordinator#populateContent}.
     *
     * <p>It should hold {@link #mThinWebView} as {@link SidePanelContent#mView}.
     */
    @Nullable SidePanelContent mSidePanelContent;

    /** The View holding {@link #mWebContents}. */
    @Nullable ThinWebView mThinWebView;

    /**
     * {@link WebContents} for {@link SidePanelDevFeature}.
     *
     * <p>It should be attached to {@link #mThinWebView}.
     */
    @Nullable WebContents mWebContents;

    SidePanelDevFeatureContent(ThinWebView thinWebView, WebContents webContents) {
        mThinWebView = thinWebView;
        mWebContents = webContents;
        mSidePanelContent = new SidePanelContent(mThinWebView.getView());
    }

    /** Destroys all objects in this data class and sets their references to {@code null}. */
    void destroy() {
        mSidePanelContent = null;

        if (mWebContents != null) {
            mWebContents.destroy();
            mWebContents = null;
        }

        if (mThinWebView != null) {
            mThinWebView.destroy();
            mThinWebView = null;
        }
    }
}
