// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.omnibox.suggestions.base;

import android.content.Context;
import android.graphics.Rect;
import android.view.View;
import android.view.ViewGroup;

import androidx.annotation.IntDef;
import androidx.annotation.NonNull;
import androidx.annotation.Px;
import androidx.annotation.VisibleForTesting;

import org.chromium.chrome.browser.omnibox.R;
import org.chromium.chrome.browser.omnibox.styles.OmniboxResourceProvider;
import org.chromium.chrome.browser.omnibox.suggestions.base.SuggestionLayout.LayoutParams.SuggestionViewType;
import org.chromium.components.browser_ui.widget.RoundedCornerOutlineProvider;

import java.lang.annotation.Retention;
import java.lang.annotation.RetentionPolicy;

/**
 * SuggestionLayout is a container aiming to quickly and correctly arrange encompassed items. The
 * operation is similar to that of ConstraintLayout, with the exception that the purpose of every
 * item is known ahead of time. This layout is highly optimized around view types, and bypasses
 * certain measurement calls, where the size of the view is known ahead of time.
 */
class SuggestionLayout extends ViewGroup {
    @VisibleForTesting public final @Px int mDecorationIconWidthPx;
    @VisibleForTesting public final @Px int mLargeDecorationIconWidthPx;
    @VisibleForTesting public final @Px int mContentHeightPx;
    @VisibleForTesting public final @Px int mCompactContentHeightPx;
    @VisibleForTesting public final @NonNull RoundedCornerOutlineProvider mOutlineProvider;
    private final @Px int mActionButtonWidthPx;
    private final @Px int mContentPaddingPx;
    private final @Px int mMinimumContentPadding;
    private boolean mUseLargeDecoration;
    private boolean mShowDecoration;

    /**
     * SuggestionLayout's LayoutParams.
     *
     * <p>Additional parameters define the role of an element..
     */
    public static class LayoutParams extends ViewGroup.LayoutParams {

        @IntDef({
            SuggestionViewType.CONTENT,
            SuggestionViewType.DECORATION,
            SuggestionViewType.ACTION_BUTTON,
            SuggestionViewType.FOOTER
        })
        @Retention(RetentionPolicy.SOURCE)
        /// Defines suggestion building blocks.
        /// The placement of every SuggestionViewType element is as follows:
        ///
        /// +---+------------------+---+---+---+
        /// | 0 | 1                | 2 | 2 | 2 |
        /// +---+------------------+---+---+---+
        /// | 3                                |
        /// +----------------------------------+
        /// | 3                                |
        /// +----------------------------------+
        ///
        /// The lines on the diagram above are for reference purpose to outline the boundaries
        /// of individual suggestion view types.
        ///
        /// Unless otherwise specified, focus ripples through all suggestion views.
        /// To override this behavior, individual views need to override their background.
        public @interface SuggestionViewType {
            /// Main content, encompassing one or more lines of text. Must be horizontally and
            /// vertically resizable.
            /// Only a single CONTENT view is permitted right now.
            int CONTENT = 0;
            /// An image presented on the left hand side of the CONTENT, such as an icon
            /// (magnifying glass, globe), site favicon, solid color or image.
            /// Multiple DECORATION buttons are presently not permitted.
            int DECORATION = 1;
            /// An action button presented on the right hand side of the CONTENT. Buttons are
            /// aligned to the top/end edge of the suggestion, and build towards the beginning
            /// of the content view (left in LTR, or right in RTL layout directions).
            /// Multiple ACTION_BUTTON elements are permitted.
            int ACTION_BUTTON = 2;
            /// FOOTER element is always added at the bottom of the suggestion, and stretches from
            /// the beginning to the end of the entire suggestion view.
            /// Multiple FOOTER elements will be stacked one on top of another.
            int FOOTER = 3;
        }

        /// The role of the associated view in the SuggestionView.
        private final @SuggestionViewType int mSuggestionViewType;
        private final @NonNull Rect mPlacement;
        private final boolean mIsLargeDecoration;

        private LayoutParams(
                int width, int height, @SuggestionViewType int type, boolean isLargeDecoration) {
            super(width, height);
            mPlacement = new Rect();
            mSuggestionViewType = type;
            mIsLargeDecoration = isLargeDecoration;
        }

        /** Create LayoutParams for particular SuggestionViewType. */
        public static LayoutParams forViewType(@SuggestionViewType int type) {
            return new LayoutParams(WRAP_CONTENT, WRAP_CONTENT, type, false);
        }

        /** Create LayoutParams for particular SuggestionViewType. */
        public static LayoutParams forLargeDecorationIcon() {
            return new LayoutParams(
                    WRAP_CONTENT, WRAP_CONTENT, SuggestionViewType.DECORATION, true);
        }

        /**
         * @return The role of the view.
         */
        private @SuggestionViewType int getViewType() {
            return mSuggestionViewType;
        }

        /**
         * @return The placement of the view, relative to Suggestion area start.
         */
        private @NonNull Rect getPlacement() {
            return mPlacement;
        }

        /**
         * Specify the position of the view relative to the SuggestionLayout's Top/Start corner.
         *
         * <p>Placement is LayoutDirection agnostic. Callers should assume that the offsets are
         * relative to the start position of the view, and sizes expand in the direction appropriate
         * for the currently used LayoutDirection.
         *
         * @param start The offset from the start edge (left for LTR layouts, right for RTL layouts)
         *     of the SuggestionLayout.
         * @param top The offset from the top edge of the SuggestionLayout.
         * @param width The width of the view (expands to the right for LTR layouts, and to the left
         *     for RTL layouts).
         * @param height The height of the view.
         */
        private void setPlacement(int start, int top, int width, int height) {
            mPlacement.set(start, top, start + width, top + height);
        }
    }

    public SuggestionLayout(Context context) {
        super(context);

        var res = context.getResources();

        int endSpace = res.getDimensionPixelSize(R.dimen.omnibox_suggestion_end_padding);
        setPaddingRelative(0, 0, endSpace, 0);

        mDecorationIconWidthPx =
                OmniboxResourceProvider.getSuggestionDecorationIconSizeWidth(context);
        mLargeDecorationIconWidthPx =
                context.getResources()
                        .getDimensionPixelSize(R.dimen.omnibox_suggestion_icon_area_size_large);

        mActionButtonWidthPx =
                res.getDimensionPixelSize(R.dimen.omnibox_suggestion_action_button_width);
        mCompactContentHeightPx =
                res.getDimensionPixelSize(R.dimen.omnibox_suggestion_compact_content_height);
        mContentHeightPx = res.getDimensionPixelSize(R.dimen.omnibox_suggestion_content_height);

        mContentPaddingPx = res.getDimensionPixelSize(R.dimen.omnibox_suggestion_content_padding);
        mMinimumContentPadding = res.getDimensionPixelSize(R.dimen.omnibox_simple_card_leadin);

        mOutlineProvider =
                new RoundedCornerOutlineProvider(
                        res.getDimensionPixelSize(
                                R.dimen.omnibox_suggestion_bg_round_corner_radius));
        setOutlineProvider(mOutlineProvider);
        setRoundingEdges(false, false);
    }

    public void setRoundingEdges(boolean roundTopEdge, boolean roundBottomEdge) {
        boolean needUpdate =
                mOutlineProvider.isTopEdgeRounded() != roundTopEdge
                        || mOutlineProvider.isBottomEdgeRounded() != roundBottomEdge;

        if (!needUpdate) return;

        mOutlineProvider.setRoundingEdges(true, roundTopEdge, true, roundBottomEdge);
        setClipToOutline(roundTopEdge || roundBottomEdge);
        // Make sure the view redraws. Otherwise, the on-screen visuals may not reflect our desired
        // rounding effect.
        invalidateOutline();
    }

    @Override
    protected void onMeasure(int widthSpec, int heightSpec) {
        // The only measure spec we know is the WIDTH of the suggestion and padding around the
        // content.
        var suggestionWidthPx =
                MeasureSpec.getSize(widthSpec) - getPaddingLeft() - getPaddingRight();

        // Check to see if and how large of a decoration icon we're going to render
        mUseLargeDecoration = getUseLargeDecoration();
        mShowDecoration = isDecorationShown();
        // First, compute the width of the content area.
        // We know the size of every DECORATION and ACTION_BUTTON, which surround the CONTENT.
        var measuredContentWidthPx = measureContentViewsWidthPx(suggestionWidthPx);

        // Next, compute the height of the CONTENT area. CONTENT may hold multiple lines of wrapped
        // text, such as dictionary entries (type: "define dictionary" for an example entry).
        // As such, these views may have varying heights.
        var measuredContentHeightPx = measureContentViewHeightPx(measuredContentWidthPx);

        // Now that we know how tall the CONTENT area is, apply that measurement to the views
        // surrounding the CONTENT area. This will permit proper content scaling and placement.
        measureDecorationIconAndActionButtons(measuredContentHeightPx);

        // Compute the height of all FOOTER views, such as Action Chips (type: "play dino game"
        // to see example action chip).
        // Multiple footers are permitted and will stack on top of each other.
        // Note that, unlike CONTENT, FOOTER views are not surrounded by DECORATION or ACTION
        // BUTTON views.
        var measuredFooterHeightPx = measureFooterViewsHeightPx(suggestionWidthPx);

        // Finally, compute the placements of every view relative to the START of the view,
        // including our own.
        // Note that while START means different things for RTL and LTR layout directions, these
        // offsets are RELATIVE, and will expand in appropriate direction during Layout phase.
        applySuggestionViewPlacements(
                suggestionWidthPx, measuredContentWidthPx, measuredContentHeightPx);
        setMeasuredDimension(
                widthSpec,
                MeasureSpec.makeMeasureSpec(
                        measuredContentHeightPx
                                + measuredFooterHeightPx
                                + getPaddingTop()
                                + getPaddingBottom(),
                        MeasureSpec.EXACTLY));
    }

    @VisibleForTesting
    boolean getUseLargeDecoration() {
        for (int index = 0; index < getChildCount(); ++index) {
            var view = getChildAt(index);
            if (view.getVisibility() == GONE) continue;

            var params = (LayoutParams) view.getLayoutParams();
            if (params.getViewType() == SuggestionViewType.DECORATION) {
                return params.mIsLargeDecoration;
            }
        }
        return false;
    }

    /**
     * Returns whether the decoration view is visible or not. Also returns true if there is no
     * decoration view present.
     */
    private boolean isDecorationShown() {
        // Default to true so that we reserve space for alignment purposes even when there is no
        // decoration icon.
        var decorationShown = true;
        for (int index = 0; index < getChildCount(); ++index) {
            var view = getChildAt(index);

            var params = (LayoutParams) view.getLayoutParams();
            if (params.getViewType() == SuggestionViewType.DECORATION) {
                decorationShown = view.getVisibility() == VISIBLE;
                break;
            }
        }

        return decorationShown;
    }

    private int getDecorationIconWidthPx() {
        return mUseLargeDecoration ? mLargeDecorationIconWidthPx : mDecorationIconWidthPx;
    }

    private int getContentStart() {
        return mShowDecoration ? getDecorationIconWidthPx() : mMinimumContentPadding;
    }

    @Override
    protected void onLayout(boolean changed, int left, int top, int right, int bottom) {
        var topPx = getPaddingTop();
        var layoutDirectionRTL = getLayoutDirection() == LAYOUT_DIRECTION_RTL;
        var startPx = layoutDirectionRTL ? right - left - getPaddingRight() : getPaddingLeft();

        for (int index = 0; index < getChildCount(); ++index) {
            var view = getChildAt(index);
            if (view.getVisibility() == GONE) continue;

            var params = (LayoutParams) view.getLayoutParams();

            // All our placements are relative to the view START:
            // - the LEFT edge, when the layout direction is LTR,
            // - the RIGHT edge, when the layout direction is RTL.
            // and grow in the direction appropriate for the layout direction.
            // - to the RIGHT, when the layout direction is LTR,
            // - to the LEFT, when the layout direction is RTL.
            var placement = params.getPlacement();
            if (layoutDirectionRTL) {
                view.layout(
                        startPx - placement.right,
                        topPx + placement.top,
                        startPx - placement.left,
                        topPx + placement.bottom);
            } else {
                view.layout(
                        startPx + placement.left,
                        topPx + placement.top,
                        startPx + placement.right,
                        topPx + placement.bottom);
            }
        }
    }

    /**
     * Given the SuggestionView width, compute the width available to the CONTENT views. CONTENT
     * views are surrounded by DECORATION and ACTION_BUTTON view types.
     *
     * <p>NOTE: this is invoked directly by the measure cycle. Do not allocate memory, and avoid any
     * calls that trigger measurement invalidation and restart.
     *
     * @param suggestionWidthPx The width of the suggestion content area
     * @return The computed width of the CONTENT views.
     */
    private @Px int measureContentViewsWidthPx(@Px int suggestionWidthPx) {
        // Reserve space for the decoration view if it's present. Otherwise, ensure a minimal
        // padding.
        var contentWidthPx = suggestionWidthPx - getContentStart();

        // Measure all other views surrounding the CONTENT area. Currently these are only
        // ACTION_BUTTONs.
        for (int index = 0; index < getChildCount(); ++index) {
            var view = getChildAt(index);
            if (view.getVisibility() == GONE) continue;

            var params = (LayoutParams) view.getLayoutParams();
            if (params.getViewType() == LayoutParams.SuggestionViewType.ACTION_BUTTON) {
                contentWidthPx -= mActionButtonWidthPx;
            }
        }
        return contentWidthPx;
    }

    /**
     * Given the CONTENT width, compute the height of the CONTENT view.
     *
     * <p>NOTE: this is invoked directly by the measure cycle. Do not allocate memory, and avoid any
     * calls that trigger measurement invalidation and restart.
     *
     * @param contentWidthPx The width of the CONTENT view.
     * @return The measured height of the CONTENT view, no less than the minimum size.
     */
    private @Px int measureContentViewHeightPx(@Px int contentWidthPx) {
        int contentHeightPx = 0;
        boolean hasFooter = false;
        View contentView = null;

        for (int index = 0; index < getChildCount(); ++index) {
            var view = getChildAt(index);
            if (view.getVisibility() == GONE) continue;

            var params = (LayoutParams) view.getLayoutParams();
            if (params.getViewType() == LayoutParams.SuggestionViewType.CONTENT) {
                assert contentHeightPx == 0 : "Content view already defined";
                // Content views' width is constrained by how much space the decoration views
                // allocate. These views may, as a result, wrap around to one or more extra lines of
                // text.
                contentView = view;
                view.measure(
                        MeasureSpec.makeMeasureSpec(contentWidthPx, MeasureSpec.EXACTLY),
                        MeasureSpec.makeMeasureSpec(0, MeasureSpec.UNSPECIFIED));
                contentHeightPx = view.getMeasuredHeight();
            } else if (params.getViewType() == LayoutParams.SuggestionViewType.FOOTER) {
                hasFooter = true;
            }
        }

        // It is possible for the measured CONTENT area to be smaller than our minimum
        // suggestion height. Apply necessary corrections here. We currently expect the
        // CONTENT view to properly utilize the LAYOUT GRAVITY to position its content
        // around the center, if its measured height is smaller than our minimum.
        assert contentView != null : "No content views";

        // Pad suggestion around to guarantee appropriate spacing around suggestions.
        // Modernized UI present their content in distinc blocks, and the extra space
        // does not break visually the relationship between the content and footer parts.
        contentHeightPx += mContentPaddingPx;

        // Guarantee that the suggestion height meets our required minimum tap target size.
        var height =
                Math.max(contentHeightPx, hasFooter ? mCompactContentHeightPx : mContentHeightPx);
        // Some views (e.g. TextView) won't render correctly unless measure specs are explicitly
        // supplied, failing to properly center the content.
        contentView.measure(
                MeasureSpec.makeMeasureSpec(contentWidthPx, MeasureSpec.EXACTLY),
                MeasureSpec.makeMeasureSpec(height, MeasureSpec.EXACTLY));
        return height;
    }

    /**
     * Given the SuggestionView width, compute the height of all FOOTER views.
     *
     * <p>NOTE: this is invoked directly by the measure cycle. Do not allocate memory, and avoid any
     * calls that trigger measurement invalidation and restart.
     *
     * @param suggestionWidthPx The width of the suggestion content area
     * @return The cumulative height of the FOOTER views.
     */
    private @Px int measureFooterViewsHeightPx(@Px int suggestionWidthPx) {
        int footerHeightPx = 0;

        for (int index = 0; index < getChildCount(); ++index) {
            var view = getChildAt(index);
            if (view.getVisibility() == GONE) continue;

            var params = (LayoutParams) view.getLayoutParams();
            if (params.getViewType() == LayoutParams.SuggestionViewType.FOOTER) {
                // FOOTER views stretch from one end of the Suggestion view to the other.
                // Unlike CONTENT views, FOOTERs are not surrounded by DECORATION or ACTION_BUTTON
                // views.
                view.measure(
                        MeasureSpec.makeMeasureSpec(suggestionWidthPx, MeasureSpec.EXACTLY),
                        MeasureSpec.makeMeasureSpec(0, MeasureSpec.UNSPECIFIED));
                footerHeightPx += view.getMeasuredHeight();
            }
        }

        return footerHeightPx;
    }

    /**
     * Given the CONTENT area dimensions, apply measurements and placement of all elements
     * surrounding the CONTENT view (currently: DECORATION and ACTION_BUTTONs).
     *
     * <p>NOTE: this is invoked directly by the measure cycle. Do not allocate memory, and avoid any
     * calls that trigger measurement invalidation and restart.
     *
     * @param contentHeightPx The height of the CONTENT area.
     */
    private void measureDecorationIconAndActionButtons(@Px int contentHeightPx) {
        var contentHeightSpec = MeasureSpec.makeMeasureSpec(contentHeightPx, MeasureSpec.AT_MOST);

        for (int index = 0; index < getChildCount(); ++index) {
            var view = getChildAt(index);
            if (view.getVisibility() == GONE) continue;

            // Capture the measure spec of the area available to DECORATION and ACTION_BUTTONs.
            // Note that at this stage everything else has already been measured.
            var viewWidthSpec = 0;
            var params = (LayoutParams) view.getLayoutParams();
            if (params.getViewType() == LayoutParams.SuggestionViewType.DECORATION) {
                viewWidthSpec =
                        getChildMeasureSpec(
                                MeasureSpec.makeMeasureSpec(
                                        getDecorationIconWidthPx(), MeasureSpec.AT_MOST),
                                0,
                                params.width);
            } else if (params.getViewType() == LayoutParams.SuggestionViewType.ACTION_BUTTON) {
                viewWidthSpec =
                        MeasureSpec.makeMeasureSpec(mActionButtonWidthPx, MeasureSpec.EXACTLY);
            } else {
                continue;
            }

            view.measure(viewWidthSpec, getChildMeasureSpec(contentHeightSpec, 0, params.height));
        }
    }

    /**
     * Apply placements to all the views.
     *
     * <p>The views are placed linearly, offering offset from the START of the encompassing view's
     * padded area. The concept of LayoutDirection is irrelevant at this point for simplicity, and
     * can be assumed "any": the placements computed here will expand views
     *
     * <ul>
     *   <li>to the RIGHT, when the layout direction is LTR, and
     *   <li>to the LEFT, when the layout direction ir RTL.
     * </ul>
     *
     * @param suggestionWidthPx The width of the Suggestion area.
     * @param contentViewsWidth The width of the CONTENT area.
     * @param contentViewHeight The height of the CONTENT area.
     */
    private void applySuggestionViewPlacements(
            @Px int suggestionWidthPx, @Px int contentViewsWidth, @Px int contentViewHeight) {
        int contentStart = getContentStart();
        var nextActionButtonStartPx = contentStart + contentViewsWidth;
        var nextFooterViewTopPx = contentViewHeight;

        for (int index = 0; index < getChildCount(); ++index) {
            var view = getChildAt(index);
            if (view.getVisibility() == GONE) continue;

            var params = (LayoutParams) view.getLayoutParams();
            switch (params.getViewType()) {
                case LayoutParams.SuggestionViewType.DECORATION:
                    // DECORATION space is square, but the image inside does not have to be.
                    // Retrieve the information about target image height and center it with the
                    // CONTENT.
                    var decorationWidth = view.getMeasuredWidth();
                    var decorationHeight = view.getMeasuredHeight();
                    var decorationLeft = (getDecorationIconWidthPx() - decorationWidth) / 2;
                    var decorationTop = (contentViewHeight - decorationHeight) / 2;
                    params.setPlacement(
                            decorationLeft, decorationTop, decorationWidth, decorationHeight);
                    break;

                case LayoutParams.SuggestionViewType.CONTENT:
                    params.setPlacement(contentStart, 0, contentViewsWidth, contentViewHeight);
                    break;

                case LayoutParams.SuggestionViewType.ACTION_BUTTON:
                    params.setPlacement(
                            nextActionButtonStartPx, 0, mActionButtonWidthPx, contentViewHeight);
                    // Horizontally line up ACTION_BUTTONs.
                    nextActionButtonStartPx += mActionButtonWidthPx;
                    break;

                case LayoutParams.SuggestionViewType.FOOTER:
                    var footerViewHeight = view.getMeasuredHeight();
                    params.setPlacement(
                            0, nextFooterViewTopPx, suggestionWidthPx, footerViewHeight);
                    // Vertically Stack FOOTERs.
                    nextFooterViewTopPx += footerViewHeight;
                    break;
            }
        }
    }
}
