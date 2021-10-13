// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.user_education;

import android.graphics.Rect;
import android.view.View;

import androidx.annotation.Nullable;

import org.chromium.components.browser_ui.widget.highlight.ViewHighlighter.HighlightParams;
import org.chromium.ui.widget.AnchoredPopupWindow;
import org.chromium.ui.widget.ViewRectProvider;

/**
 * Class encapsulating the data needed to show in-product help (IPH).
 */
public class IPHCommand {
    public final String featureName;
    public final String contentString;
    public final String accessibilityText;
    public final boolean dismissOnTouch;
    public final View anchorView;
    @Nullable
    public final Runnable onDismissCallback;
    @Nullable
    public final Runnable onShowCallback;
    @Nullable
    public final Runnable onBlockedCallback;
    public final Rect insetRect;
    public final long autoDismissTimeout;
    public final ViewRectProvider viewRectProvider;
    @Nullable
    public final HighlightParams highlightParams;
    public final Rect anchorRect;
    public final boolean removeArrow;
    @AnchoredPopupWindow.VerticalOrientation
    public final int preferredVerticalOrientation;

    IPHCommand(String featureName, String contentString, String accessibilityText,
            boolean dismissOnTouch, View anchorView, Runnable onDismissCallback,
            Runnable onShowCallback, Runnable onBlockedCallback, Rect insetRect,
            long autoDismissTimeout, ViewRectProvider viewRectProvider, HighlightParams params,
            Rect anchorRect, boolean removeArrow,
            @AnchoredPopupWindow.VerticalOrientation int preferredVerticalOrientation) {
        this.featureName = featureName;
        this.contentString = contentString;
        this.accessibilityText = accessibilityText;
        this.dismissOnTouch = dismissOnTouch;
        this.anchorView = anchorView;
        this.onDismissCallback = onDismissCallback;
        this.onShowCallback = onShowCallback;
        this.onBlockedCallback = onBlockedCallback;
        this.insetRect = insetRect;
        this.autoDismissTimeout = autoDismissTimeout;
        this.viewRectProvider = viewRectProvider;
        this.highlightParams = params;
        this.anchorRect = anchorRect;
        this.removeArrow = removeArrow;
        this.preferredVerticalOrientation = preferredVerticalOrientation;
    }
}
