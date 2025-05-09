// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.user_education;

import android.content.res.Resources;
import android.graphics.Rect;
import android.view.View;

import org.chromium.build.annotations.NullMarked;
import org.chromium.build.annotations.Nullable;
import org.chromium.components.browser_ui.widget.highlight.ViewHighlighter.HighlightParams;
import org.chromium.ui.widget.AnchoredPopupWindow;
import org.chromium.ui.widget.ViewRectProvider;

/** Class encapsulating the data needed to show in-product help (IPH). */
@NullMarked
public class IphCommand {
    private final Resources mResources;
    public final String featureName;
    public final int stringId;
    public Object @Nullable [] stringArgs;
    public @Nullable String contentString;
    public final int accessibilityStringId;
    public Object @Nullable [] accessibilityStringArgs;
    public @Nullable String accessibilityText;
    public final boolean dismissOnTouch;
    public final @Nullable View anchorView;
    public final Runnable onDismissCallback;
    public final Runnable onShowCallback;
    public final Runnable onBlockedCallback;
    public @Nullable Rect insetRect;
    public final long autoDismissTimeout;
    public final long dismissOnTouchTimeout;
    public final @Nullable ViewRectProvider viewRectProvider;
    public final @Nullable HighlightParams highlightParams;
    public final @Nullable Rect anchorRect;
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

    IphCommand(
            Resources resources,
            String featureName,
            int stringId,
            Object @Nullable [] stringArgs,
            @Nullable String contentString,
            int accessibilityStringId,
            Object @Nullable [] accessibilityStringArgs,
            @Nullable String accessibilityText,
            boolean dismissOnTouch,
            @Nullable View anchorView,
            Runnable onDismissCallback,
            Runnable onShowCallback,
            Runnable onBlockedCallback,
            long autoDismissTimeout,
            long dismissOnTouchTimeout,
            @Nullable ViewRectProvider viewRectProvider,
            @Nullable HighlightParams params,
            @Nullable Rect anchorRect,
            boolean removeArrow,
            boolean showTextBubble,
            @AnchoredPopupWindow.VerticalOrientation int preferredVerticalOrientation,
            @Nullable Rect insetRect) {
        this.mResources = resources;
        this.featureName = featureName;
        this.stringId = stringId;
        this.stringArgs = stringArgs;
        this.contentString = contentString;
        this.accessibilityStringId = accessibilityStringId;
        this.accessibilityStringArgs = accessibilityStringArgs;
        this.accessibilityText = accessibilityText;
        this.dismissOnTouch = dismissOnTouch;
        this.anchorView = anchorView;
        this.onDismissCallback = onDismissCallback;
        this.onShowCallback = onShowCallback;
        this.onBlockedCallback = onBlockedCallback;
        this.autoDismissTimeout = autoDismissTimeout;
        this.dismissOnTouchTimeout = dismissOnTouchTimeout;
        this.viewRectProvider = viewRectProvider;
        this.highlightParams = params;
        this.anchorRect = anchorRect;
        this.removeArrow = removeArrow;
        this.showTextBubble = showTextBubble;
        this.preferredVerticalOrientation = preferredVerticalOrientation;
        this.insetRect = insetRect;
    }
}
