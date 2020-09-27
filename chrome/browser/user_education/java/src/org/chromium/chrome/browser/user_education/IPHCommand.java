// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.user_education;

import android.graphics.Rect;
import android.view.View;

import androidx.annotation.Nullable;

import org.chromium.ui.widget.ViewRectProvider;

/**
 * Class encapsulating the data needed to show in-product help (IPH).
 */
public class IPHCommand {
    public final String featureName;
    public final String contentString;
    public final String accessibilityText;
    public final boolean circleHighlight;
    public final boolean shouldHighlight;
    public final boolean dismissOnTouch;
    public final View anchorView;
    @Nullable
    public final Runnable onDismissCallback;
    @Nullable
    public final Runnable onShowCallback;
    public final Rect insetRect;
    public final long autoDismissTimeout;
    public final ViewRectProvider viewRectProvider;

    IPHCommand(String featureName, String contentString, String accessibilityText,
            boolean circleHighlight, boolean shouldHighlight, boolean dismissOnTouch,
            View anchorView, Runnable onDismissCallback, Runnable onShowCallback, Rect insetRect,
            long autoDismissTimeout, ViewRectProvider viewRectProvider) {
        this.featureName = featureName;
        this.contentString = contentString;
        this.accessibilityText = accessibilityText;
        this.circleHighlight = circleHighlight;
        this.shouldHighlight = shouldHighlight;
        this.dismissOnTouch = dismissOnTouch;
        this.anchorView = anchorView;
        this.onDismissCallback = onDismissCallback;
        this.onShowCallback = onShowCallback;
        this.insetRect = insetRect;
        this.autoDismissTimeout = autoDismissTimeout;
        this.viewRectProvider = viewRectProvider;
    }
}