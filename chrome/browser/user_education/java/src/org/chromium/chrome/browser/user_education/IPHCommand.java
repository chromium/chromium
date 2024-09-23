// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.user_education;

import android.content.res.Resources;
import android.graphics.Rect;
import android.view.View;

import androidx.annotation.Nullable;

import org.chromium.components.browser_ui.widget.highlight.ViewHighlighter.HighlightParams;
import org.chromium.ui.widget.AnchoredPopupWindow;
import org.chromium.ui.widget.ViewRectProvider;

/** Class encapsulating the data needed to show in-product help (IPH). */
public class IPHCommand {
    private Resources mResources;
    public final String featureName;
    public final int stringId;
    public Object[] stringArgs;
    public String contentString;
    public final int accessibilityStringId;
    public Object[] accessibilityStringArgs;
    public String accessibilityText;
    public final boolean dismissOnTouch;
    public final View anchorView;
    @Nullable public final Runnable onDismissCallback;
    @Nullable public final Runnable onShowCallback;
    @Nullable public final Runnable onBlockedCallback;
    public Rect insetRect;
    public final long autoDismissTimeout;
    public final ViewRectProvider viewRectProvider;
    @Nullable public final HighlightParams highlightParams;
    public final Rect anchorRect;
    public final boolean removeArrow;
    public final boolean showTextBubble;
    @AnchoredPopupWindow.VerticalOrientation public final int preferredVerticalOrientation;

    public void fetchFromResources() {
        if (contentString == null) {
            assert mResources != null;
            if (stringArgs != null) {
                contentString = mResources.getString(stringId, stringArgs);
            } else {
                contentString = mResources.getString(stringId);
            }
        }

        if (accessibilityText == null) {
            assert mResources != null;
            if (accessibilityStringArgs != null) {
                accessibilityText =
                        mResources.getString(accessibilityStringId, accessibilityStringArgs);
            } else {
                accessibilityText = mResources.getString(accessibilityStringId);
            }
        }

        if (insetRect == null && anchorRect == null) {
            int yInsetPx =
                    mResources.getDimensionPixelOffset(R.dimen.iph_text_bubble_menu_anchor_y_inset);
            insetRect = new Rect(0, 0, 0, yInsetPx);
        }
    }

    IPHCommand(
            Resources resources,
            String featureName,
            int stringId,
            Object[] stringArgs,
            int accessibilityStringId,
            Object[] accessibilityStringArgs,
            boolean dismissOnTouch,
            View anchorView,
            Runnable onDismissCallback,
            Runnable onShowCallback,
            Runnable onBlockedCallback,
            long autoDismissTimeout,
            ViewRectProvider viewRectProvider,
            HighlightParams params,
            Rect anchorRect,
            boolean removeArrow,
            boolean showTextBubble,
            @AnchoredPopupWindow.VerticalOrientation int preferredVerticalOrientation,
            Rect insetRect) {
        this.mResources = resources;
        this.featureName = featureName;
        this.stringId = stringId;
        this.stringArgs = stringArgs;
        this.accessibilityStringId = accessibilityStringId;
        this.accessibilityStringArgs = accessibilityStringArgs;
        this.dismissOnTouch = dismissOnTouch;
        this.anchorView = anchorView;
        this.onDismissCallback = onDismissCallback;
        this.onShowCallback = onShowCallback;
        this.onBlockedCallback = onBlockedCallback;
        this.autoDismissTimeout = autoDismissTimeout;
        this.viewRectProvider = viewRectProvider;
        this.highlightParams = params;
        this.anchorRect = anchorRect;
        this.removeArrow = removeArrow;
        this.showTextBubble = showTextBubble;
        this.preferredVerticalOrientation = preferredVerticalOrientation;
        this.insetRect = insetRect;
    }
}
