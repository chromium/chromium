// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.user_education;

import android.content.res.Resources;
import android.graphics.Rect;
import android.view.View;

import androidx.annotation.StringRes;

import org.chromium.components.browser_ui.widget.textbubble.TextBubble;
import org.chromium.ui.widget.ViewRectProvider;

/**
 * Builder for (@see IPHCommand.java). Use this instead of constructing an IPHCommand directly.
 */
public class IPHCommandBuilder {
    private static final Runnable NO_OP_RUNNABLE = () -> {};

    private Resources mResources;
    private final String mFeatureName;
    private String mContentString;
    private String mAccessibilityText;
    private boolean mShouldHighlight;
    private boolean mCircleHighlight = true;
    private boolean mDismissOnTouch = true;
    @StringRes
    private int mStringId;
    @StringRes
    private int mAccessibilityStringId;
    private View mAnchorView;
    private Runnable mOnShowCallback;
    private Runnable mOnDismissCallback;
    private Rect mInsetRect;
    private long mAutoDismissTimeout = TextBubble.NO_TIMEOUT;
    private ViewRectProvider mViewRectProvider;

    /**
     * Constructor for IPHCommandBuilder when you would like your strings to be resolved for you.
     * @param resources Resources object used to resolve strings and dimensions.
     * @param featureName String identifier for the feature from FeatureConstants.
     * @param stringId Resource id of the string displayed to the use.
     * @param accessibilityStringId resource id of the string to use for accessibility.
     */
    public IPHCommandBuilder(Resources resources, String featureName, @StringRes int stringId,
            @StringRes int accessibilityStringId) {
        mResources = resources;
        mFeatureName = featureName;
        mStringId = stringId;
        mAccessibilityStringId = accessibilityStringId;
    }

    /**
     * Constructor for IPHCommandBuilder when you have your strings pre-resolved.
     * @param resources Resources object used to resolve strings and dimensions.
     * @param featureName String identifier for the feature from FeatureConstants.
     * @param contentString String displayed to the user.
     * @param accessibilityText String to use for accessibility.
     */
    public IPHCommandBuilder(Resources resources, String featureName, String contentString,
            String accessibilityText) {
        mResources = resources;
        mFeatureName = featureName;
        mContentString = contentString;
        mAccessibilityText = accessibilityText;
    }

    /**
     *
     * @param circleHighlight whether the highlight should be circular.
     */
    public IPHCommandBuilder setCircleHighlight(boolean circleHighlight) {
        mCircleHighlight = circleHighlight;
        return this;
    }

    /**
     *
     * @param shouldHighlight whether the anchor view should be highlighted.
     */
    public IPHCommandBuilder setShouldHighlight(boolean shouldHighlight) {
        mShouldHighlight = shouldHighlight;
        return this;
    }

    /**
     *
     * @param anchorView the view that the IPH bubble should be anchored to.
     */
    public IPHCommandBuilder setAnchorView(View anchorView) {
        mAnchorView = anchorView;
        return this;
    }

    /**
     *
     * @param onShowCallback callback to invoke when the IPH bubble is first shown.
     */
    public IPHCommandBuilder setOnShowCallback(Runnable onShowCallback) {
        mOnShowCallback = onShowCallback;
        return this;
    }

    /**
     *
     * @param onDismissCallback callback to invoke when the IPH bubble is dismissed.
     */
    public IPHCommandBuilder setOnDismissCallback(Runnable onDismissCallback) {
        mOnDismissCallback = onDismissCallback;
        return this;
    }

    /**
     *
     * @param insetRect The inset rectangle to use when shrinking the anchor view to show the IPH
     * bubble.
     */
    public IPHCommandBuilder setInsetRect(Rect insetRect) {
        mInsetRect = insetRect;
        return this;
    }

    /**
     *
     * @param dismissOnTouch Whether the IPH bubble should be dismissed when the user performs a
     *         touch interaction.
     */
    public IPHCommandBuilder setDismissOnTouch(boolean dismissOnTouch) {
        mDismissOnTouch = dismissOnTouch;
        return this;
    }

    /**
     *
     * @param timeout Timeout in milliseconds to auto-dismiss the IPH bubble.
     */
    public IPHCommandBuilder setAutoDismissTimeout(int timeout) {
        mAutoDismissTimeout = timeout;
        return this;
    }

    /**
     *
     * @param viewRectProvider Custom ViewRectProvider to replace the default one. Note that the
     *                         provided insets will still be applied on the rectangle from the
     *                         custom provider.
     */
    public IPHCommandBuilder setViewRectProvider(ViewRectProvider viewRectProvider) {
        mViewRectProvider = viewRectProvider;
        return this;
    }

    /**
     *
     * @return an (@see IPHCommand) containing the accumulated state of this builder.
     */
    public IPHCommand build() {
        if (mOnDismissCallback == null) {
            mOnDismissCallback = NO_OP_RUNNABLE;
        }
        if (mOnShowCallback == null) {
            mOnShowCallback = NO_OP_RUNNABLE;
        }

        if (mContentString == null) {
            assert mResources != null;
            mContentString = mResources.getString(mStringId);
        }

        if (mAccessibilityText == null) {
            assert mResources != null;
            mAccessibilityText = mResources.getString(mAccessibilityStringId);
        }

        if (mInsetRect == null) {
            int yInsetPx =
                    mResources.getDimensionPixelOffset(R.dimen.iph_text_bubble_menu_anchor_y_inset);
            mInsetRect = new Rect(0, 0, 0, yInsetPx);
        }

        return new IPHCommand(mFeatureName, mContentString, mAccessibilityText, mCircleHighlight,
                mShouldHighlight, mDismissOnTouch, mAnchorView, mOnDismissCallback, mOnShowCallback,
                mInsetRect, mAutoDismissTimeout, mViewRectProvider);
    }
}
