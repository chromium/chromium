// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.user_education;

import android.content.res.Resources;
import android.graphics.Rect;
import android.view.View;

import androidx.annotation.Nullable;
import androidx.annotation.StringRes;

import org.chromium.base.CallbackUtils;
import org.chromium.base.TraceEvent;
import org.chromium.components.browser_ui.widget.highlight.ViewHighlighter.HighlightParams;
import org.chromium.components.browser_ui.widget.textbubble.TextBubble;
import org.chromium.ui.widget.AnchoredPopupWindow;
import org.chromium.ui.widget.ViewRectProvider;

/** Builder for (@see IphCommand.java). Use this instead of constructing an IphCommand directly. */
public class IphCommandBuilder {

    private Resources mResources;
    private final String mFeatureName;
    private boolean mDismissOnTouch = true;
    @StringRes private int mStringId;
    private Object[] mStringArgs;
    @StringRes private int mAccessibilityStringId;
    private Object[] mAccessibilityStringArgs;
    private View mAnchorView;
    private Runnable mOnShowCallback;
    private Runnable mOnBlockedCallback;
    private Runnable mOnDismissCallback;
    private Rect mInsetRect;
    private long mAutoDismissTimeout = TextBubble.NO_TIMEOUT;
    private ViewRectProvider mViewRectProvider;
    @Nullable private HighlightParams mHighlightParams;
    private Rect mAnchorRect;
    private boolean mRemoveArrow;
    private boolean mShowTextBubble = true;

    @AnchoredPopupWindow.VerticalOrientation
    private int mPreferredVerticalOrientation =
            AnchoredPopupWindow.VerticalOrientation.MAX_AVAILABLE_SPACE;

    /**
     * Constructor for IphCommandBuilder when you would like your strings to be resolved for you.
     *
     * @param resources Resources object used to resolve strings and dimensions.
     * @param featureName String identifier for the feature from FeatureConstants.
     * @param stringId Resource id of the string displayed to the user.
     * @param accessibilityStringId Resource id of the string to use for accessibility.
     */
    public IphCommandBuilder(
            Resources resources,
            String featureName,
            @StringRes int stringId,
            @StringRes int accessibilityStringId) {
        mResources = resources;
        mFeatureName = featureName;
        mStringId = stringId;
        mAccessibilityStringId = accessibilityStringId;
    }

    /**
     * Constructor for IphCommandBuilder when you would like your parameterized strings to be
     * resolved for you.
     *
     * @param resources Resources object used to resolve strings and dimensions.
     * @param featureName String identifier for the feature from FeatureConstants.
     * @param stringId Resource id of the string displayed to the user.
     * @param stringArgs Ordered arguments to use during parameterized string resolution of
     *     stringId.
     * @param accessibilityStringId Resource id of the string to use for accessibility.
     * @param accessibilityStringArgs Ordered arguments to use during parameterized string
     *     resolution of accessibilityStringId.
     */
    public IphCommandBuilder(
            Resources resources,
            String featureName,
            @StringRes int stringId,
            Object[] stringArgs,
            @StringRes int accessibilityStringId,
            Object[] accessibilityStringArgs) {
        mResources = resources;
        mFeatureName = featureName;
        mStringId = stringId;
        mStringArgs = stringArgs;
        mAccessibilityStringId = accessibilityStringId;
        mAccessibilityStringArgs = accessibilityStringArgs;
    }

    /**
     * Constructor for IphCommandBuilder when you have your strings pre-resolved.
     *
     * @param resources Resources object used to resolve strings and dimensions.
     * @param featureName String identifier for the feature from FeatureConstants.
     * @param contentString String displayed to the user.
     * @param accessibilityText String to use for accessibility.
     */
    public IphCommandBuilder(
            Resources resources,
            String featureName,
            String contentString,
            String accessibilityText) {
        mResources = resources;
        mFeatureName = featureName;
    }

    /**
     * @param anchorView the view that the IPH bubble should be anchored to.
     */
    public IphCommandBuilder setAnchorView(View anchorView) {
        mAnchorView = anchorView;
        return this;
    }

    /**
     * @param onShowCallback callback to invoke when the IPH bubble is first shown.
     */
    public IphCommandBuilder setOnShowCallback(Runnable onShowCallback) {
        mOnShowCallback = onShowCallback;
        return this;
    }

    /**
     * @param onBlockedCallback callback to invoke if the IPH bubble is finally not shown.
     */
    public IphCommandBuilder setOnNotShownCallback(Runnable onBlockedCallback) {
        mOnBlockedCallback = onBlockedCallback;
        return this;
    }

    /**
     * @param onDismissCallback callback to invoke when the IPH bubble is dismissed.
     */
    public IphCommandBuilder setOnDismissCallback(Runnable onDismissCallback) {
        mOnDismissCallback = onDismissCallback;
        return this;
    }

    /**
     * @param insetRect The inset rectangle to use when shrinking the anchor view to show the IPH
     *     bubble. Note that the inset rectangle can only be applied when the AnchorRect is set to
     *     null.
     */
    public IphCommandBuilder setInsetRect(Rect insetRect) {
        assert mAnchorRect == null;
        mInsetRect = insetRect;
        return this;
    }

    /**
     * @param dismissOnTouch Whether the IPH bubble should be dismissed when the user performs a
     *     touch interaction.
     */
    public IphCommandBuilder setDismissOnTouch(boolean dismissOnTouch) {
        mDismissOnTouch = dismissOnTouch;
        return this;
    }

    /**
     * @param timeout Timeout in milliseconds to auto-dismiss the IPH bubble.
     */
    public IphCommandBuilder setAutoDismissTimeout(int timeout) {
        mAutoDismissTimeout = timeout;
        return this;
    }

    /**
     * @param viewRectProvider Custom ViewRectProvider to replace the default one. Note that the
     *     provided insets will still be applied on the rectangle from the custom provider. In
     *     addition, the custom ViewRectProvider can only be set when the AnchorRect is set to null.
     */
    public IphCommandBuilder setViewRectProvider(ViewRectProvider viewRectProvider) {
        assert mAnchorRect == null;
        mViewRectProvider = viewRectProvider;
        return this;
    }

    /**
     * @param anchorRect The rectangle that the IPH bubble should be anchored to. Note that the
     *     anchor rectangle can only be set when the ViewRectProvider and the InsetRect are set to
     *     null.
     */
    public IphCommandBuilder setAnchorRect(Rect anchorRect) {
        assert mViewRectProvider == null;
        assert mInsetRect == null;
        mAnchorRect = anchorRect;
        return this;
    }

    /**
     * @param removeArrow Whether the IPH arrow should be removed.
     */
    public IphCommandBuilder setRemoveArrow(boolean removeArrow) {
        mRemoveArrow = removeArrow;
        return this;
    }

    /**
     * @param showTextBubble Whether to show the text bubble (tooltip)
     */
    public IphCommandBuilder setShowTextBubble(boolean showTextBubble) {
        mShowTextBubble = showTextBubble;
        return this;
    }

    /**
     * @param params Defines how to draw the Highlight within the view. If set to null, IPH without
     *     a highlight will requested.
     */
    public IphCommandBuilder setHighlightParams(HighlightParams params) {
        assert params != null;
        mHighlightParams = params;
        return this;
    }

    /**
     * @param preferredVerticalOrientation {@link AnchoredPopupWindow.VerticalOrientation} that
     *     determines the preferred location for the IPH.
     */
    public IphCommandBuilder setPreferredVerticalOrientation(
            @AnchoredPopupWindow.VerticalOrientation int preferredVerticalOrientation) {
        mPreferredVerticalOrientation = preferredVerticalOrientation;
        return this;
    }

    /**
     * @return an (@see IphCommand) containing the accumulated state of this builder.
     */
    public IphCommand build() {
        try (TraceEvent te = TraceEvent.scoped("IphCommandBuilder::build")) {
            if (mOnDismissCallback == null) {
                mOnDismissCallback = CallbackUtils.emptyRunnable();
            }
            if (mOnShowCallback == null) {
                mOnShowCallback = CallbackUtils.emptyRunnable();
            }

            if (mOnBlockedCallback == null) {
                mOnBlockedCallback = CallbackUtils.emptyRunnable();
            }

            return new IphCommand(
                    mResources,
                    mFeatureName,
                    mStringId,
                    mStringArgs,
                    mAccessibilityStringId,
                    mAccessibilityStringArgs,
                    mDismissOnTouch,
                    mAnchorView,
                    mOnDismissCallback,
                    mOnShowCallback,
                    mOnBlockedCallback,
                    mAutoDismissTimeout,
                    mViewRectProvider,
                    mHighlightParams,
                    mAnchorRect,
                    mRemoveArrow,
                    mShowTextBubble,
                    mPreferredVerticalOrientation,
                    mInsetRect);
        }
    }
}
