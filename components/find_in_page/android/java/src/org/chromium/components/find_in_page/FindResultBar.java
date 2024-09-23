// Copyright 2015 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.components.find_in_page;

import android.animation.Animator;
import android.animation.AnimatorListenerAdapter;
import android.animation.ObjectAnimator;
import android.annotation.SuppressLint;
import android.content.Context;
import android.content.res.Resources;
import android.graphics.Canvas;
import android.graphics.Paint;
import android.graphics.RectF;
import android.view.Gravity;
import android.view.MotionEvent;
import android.view.View;
import android.view.ViewGroup;
import android.widget.FrameLayout;

import org.chromium.base.MathUtils;
import org.chromium.ui.base.LocalizationUtils;
import org.chromium.ui.base.WindowAndroid;
import org.chromium.ui.interpolators.Interpolators;

import java.util.ArrayList;
import java.util.Arrays;
import java.util.Collections;
import java.util.Comparator;
import java.util.List;

/**
 * The view that shows the positions of the find in page matches and allows scrubbing
 * between the entries.
 */
public class FindResultBar extends View {
    private static final int VISIBILITY_ANIMATION_DURATION_MS = 200;

    private final int mBackgroundColor;
    private final int mBackgroundBorderColor;
    private final int mResultColor;
    private final int mResultBorderColor;
    private final int mActiveColor;
    private final int mActiveBorderColor;

    private final int mBarTouchWidth;
    private final int mBarDrawWidth;
    private final int mResultMinHeight;
    private final int mActiveMinHeight;
    private final int mBarVerticalPadding;
    private final int mMinGapBetweenStacks;
    private final int mStackedResultHeight;

    private FindInPageBridge mFindInPageBridge;
    private final WindowAndroid mWindowAndroid;

    private int mRectsVersion = -1;
    private RectF[] mMatches = new RectF[0];
    private RectF mActiveMatch;

    private ArrayList<Tickmark> mTickmarks = new ArrayList<Tickmark>(0);
    private int mBarHeightForWhichTickmarksWereCached = -1;

    private Animator mVisibilityAnimation;
    private boolean mDismissing;

    private final Paint mFillPaint;
    private final Paint mStrokePaint;

    private boolean mWaitingForActivateAck;

    private static Comparator<RectF> sComparator =
            new Comparator<RectF>() {
                @Override
                public int compare(RectF a, RectF b) {
                    int top = Float.compare(a.top, b.top);
                    if (top != 0) return top;
                    return Float.compare(a.left, b.left);
                }
            };

    /**
     * Creates an instance of a {@link FindResultBar}. Also adds it to a parent {@link FrameLayout}
     * and animates itself into view.
     * @param context The Context to create this {@link FindResultBar} under.
     * @param contentView The FrameLayout that will hold this FindResultBar.
     * @param windowAndroid The WindowAndroid hosting the WebContents under search.
     * @param findInPageBridge Facilitator for user interactions.
     */
    public FindResultBar(
            Context context,
            FrameLayout parent,
            WindowAndroid windowAndroid,
            FindInPageBridge findInPageBridge) {
        super(context);

        Resources res = context.getResources();
        mBackgroundColor = context.getColor(R.color.find_result_bar_background_color);
        mBackgroundBorderColor = context.getColor(R.color.find_result_bar_background_border_color);
        mResultColor = context.getColor(R.color.find_result_bar_result_color);
        mResultBorderColor = context.getColor(R.color.find_result_bar_result_border_color);
        mActiveColor = context.getColor(R.color.find_result_bar_active_color);
        mActiveBorderColor = context.getColor(R.color.find_result_bar_active_border_color);
        mBarTouchWidth = res.getDimensionPixelSize(R.dimen.find_result_bar_touch_width);
        mBarDrawWidth =
                res.getDimensionPixelSize(R.dimen.find_result_bar_draw_width)
                        + res.getDimensionPixelSize(R.dimen.find_result_bar_separator_width);
        mResultMinHeight = res.getDimensionPixelSize(R.dimen.find_result_bar_result_min_height);
        mActiveMinHeight = res.getDimensionPixelSize(R.dimen.find_result_bar_active_min_height);
        mBarVerticalPadding = res.getDimensionPixelSize(R.dimen.find_result_bar_vertical_padding);
        mMinGapBetweenStacks =
                res.getDimensionPixelSize(R.dimen.find_result_bar_min_gap_between_stacks);
        mStackedResultHeight =
                res.getDimensionPixelSize(R.dimen.find_result_bar_stacked_result_height);

        mFillPaint = new Paint();
        mStrokePaint = new Paint();
        mFillPaint.setAntiAlias(true);
        mStrokePaint.setAntiAlias(true);
        mFillPaint.setStyle(Paint.Style.FILL);
        mStrokePaint.setStyle(Paint.Style.STROKE);
        mStrokePaint.setStrokeWidth(1.0f);

        mFindInPageBridge = findInPageBridge;

        parent.addView(
                this,
                new FrameLayout.LayoutParams(
                        mBarTouchWidth, ViewGroup.LayoutParams.MATCH_PARENT, Gravity.END));
        setTranslationX(MathUtils.flipSignIf(mBarTouchWidth, LocalizationUtils.isLayoutRtl()));

        mVisibilityAnimation = ObjectAnimator.ofFloat(this, TRANSLATION_X, 0);
        mVisibilityAnimation.setDuration(VISIBILITY_ANIMATION_DURATION_MS);
        mVisibilityAnimation.setInterpolator(Interpolators.LINEAR_OUT_SLOW_IN_INTERPOLATOR);

        mWindowAndroid = windowAndroid;
        if (windowAndroid == null) {
            throw new IllegalArgumentException("WindowAndroid must be non null.");
        }
        windowAndroid.startAnimationOverContent(mVisibilityAnimation);
    }

    /** Dismisses this results bar by removing it from the view hierarchy. */
    public void dismiss() {
        mDismissing = true;
        mFindInPageBridge = null;
        if (mVisibilityAnimation != null && mVisibilityAnimation.isRunning()) {
            mVisibilityAnimation.cancel();
        }

        mVisibilityAnimation =
                ObjectAnimator.ofFloat(
                        this,
                        TRANSLATION_X,
                        MathUtils.flipSignIf(mBarTouchWidth, LocalizationUtils.isLayoutRtl()));
        mVisibilityAnimation.setDuration(VISIBILITY_ANIMATION_DURATION_MS);
        mVisibilityAnimation.setInterpolator(Interpolators.FAST_OUT_LINEAR_IN_INTERPOLATOR);
        mWindowAndroid.startAnimationOverContent(mVisibilityAnimation);
        mVisibilityAnimation.addListener(
                new AnimatorListenerAdapter() {
                    @Override
                    public void onAnimationEnd(Animator animation) {
                        super.onAnimationEnd(animation);

                        if (getParent() != null) {
                            ((ViewGroup) getParent()).removeView(FindResultBar.this);
                        }
                    }
                });
    }

    /** Setup the tickmarks to draw using the rects of the find results. */
    public void setMatchRects(int version, RectF[] rects, RectF activeRect) {
        if (mRectsVersion != version) {
            mRectsVersion = version;
            assert rects != null;
            mMatches = rects;
            mTickmarks.clear();
            Arrays.sort(mMatches, sComparator);
            mBarHeightForWhichTickmarksWereCached = -1;
        }
        mActiveMatch = activeRect; // Can be null.
        invalidate();
    }

    /** Clears the tickmarks. */
    public void clearMatchRects() {
        setMatchRects(-1, new RectF[0], null);
    }

    /** To be called after a find result has come back. */
    public void onFindResult() {
        mWaitingForActivateAck = false;
    }

    /** Returns the last version passed to setMatchRects. */
    public int getRectsVersion() {
        return mRectsVersion;
    }

    @Override
    @SuppressLint("ClickableViewAccessibility")
    public boolean onTouchEvent(MotionEvent event) {
        if (!mDismissing
                && mTickmarks.size() > 0
                && mTickmarks.size() == mMatches.length
                && !mWaitingForActivateAck
                && event.getAction() != MotionEvent.ACTION_CANCEL) {
            // We decided it's more important to get the keyboard out of the
            // way asap; the user can compensate if their next MotionEvent
            // scrolls somewhere unintended.
            mWindowAndroid.getKeyboardDelegate().hideKeyboard(this);

            // Identify which drawn tickmark is closest to the user's finger.
            int closest =
                    Collections.binarySearch(mTickmarks, new Tickmark(event.getY(), event.getY()));
            if (closest < 0) {
                // No exact match, so must determine nearest.
                int insertionPoint = -1 - closest;
                if (insertionPoint == 0) {
                    closest = 0;
                } else if (insertionPoint == mTickmarks.size()) {
                    closest = mTickmarks.size() - 1;
                } else {
                    float distanceA =
                            Math.abs(event.getY() - mTickmarks.get(insertionPoint - 1).centerY());
                    float distanceB =
                            Math.abs(event.getY() - mTickmarks.get(insertionPoint).centerY());
                    closest = insertionPoint - (distanceA <= distanceB ? 1 : 0);
                }
            }

            // Now activate the find match corresponding to that tickmark.
            // Since mTickmarks may be outdated, we can't just pass the index.
            // Instead we send the renderer the coordinates of the center of the
            // find match's rect (as originally received in setMatchRects), and
            // it will activate whatever find result is currently closest to
            // that point (which will usually be the same one).
            mWaitingForActivateAck = true;
            mFindInPageBridge.activateNearestFindResult(
                    mMatches[closest].centerX(), mMatches[closest].centerY());
        }
        return true; // Consume the event, whether or not we acted upon it.
    }

    @Override
    protected void onSizeChanged(int w, int h, int oldw, int oldh) {
        super.onSizeChanged(w, h, oldw, oldh);
        // Check for new rects, as they may move if the document size changes.
        if (!mDismissing && mMatches.length > 0) {
            mFindInPageBridge.requestFindMatchRects(mRectsVersion);
        }
    }

    @Override
    protected void onDraw(Canvas canvas) {
        super.onDraw(canvas);

        int leftMargin = getLeftMargin();
        mFillPaint.setColor(mBackgroundColor);
        mStrokePaint.setColor(mBackgroundBorderColor);
        canvas.drawRect(leftMargin, 0, leftMargin + mBarDrawWidth, getHeight(), mFillPaint);
        float lineX =
                LocalizationUtils.isLayoutRtl()
                        ? leftMargin + mBarDrawWidth - 0.5f
                        : leftMargin + 0.5f;
        canvas.drawLine(lineX, 0, lineX, getHeight(), mStrokePaint);

        if (mMatches.length == 0) {
            return;
        }

        if (mBarHeightForWhichTickmarksWereCached != getHeight()) {
            calculateTickmarks();
        }

        // Draw all matches (since they're sorted by increasing y-position
        // overlapping tickmarks will form nice stacks).
        mFillPaint.setColor(mResultColor);
        mStrokePaint.setColor(mResultBorderColor);
        for (Tickmark tickmark : mTickmarks) {
            RectF rect = tickmark.toRectF();
            canvas.drawRoundRect(rect, 2, 2, mFillPaint);
            canvas.drawRoundRect(rect, 2, 2, mStrokePaint);
        }

        // Draw the active tickmark on top (covering up the inactive tickmark
        // we probably already drew for it).
        if (mActiveMatch != null && !mActiveMatch.isEmpty()) {
            int i = Arrays.binarySearch(mMatches, mActiveMatch, sComparator);
            if (i >= 0) {
                // We've already generated a tickmark for all rects in mMatches,
                // so use the corresponding one. However it was generated
                // assuming the match would be inactive. Keep the position, but
                // re-expand it using mActiveMinHeight.
                Tickmark tickmark = expandTickmarkToMinHeight(mTickmarks.get(i), true);
                RectF rect = tickmark.toRectF();
                mFillPaint.setColor(mActiveColor);
                mStrokePaint.setColor(mActiveBorderColor);
                canvas.drawRoundRect(rect, 2, 2, mFillPaint);
                canvas.drawRoundRect(rect, 2, 2, mStrokePaint);
            }
        }
    }

    private int getLeftMargin() {
        return LocalizationUtils.isLayoutRtl() ? 0 : getWidth() - mBarDrawWidth;
    }

    private void calculateTickmarks() {
        // TODO(johnme): Simplify calculation, and switch to integer arithmetic
        // where possible (tickmarks within groups will still need fractional
        // y-positions for anti-aliasing, but the start and end positions of
        // groups can and should be integer-aligned [will give crisp borders],
        // and the intermediary logic uses more floats than necessary).
        // TODO(johnme): Consider adding unit tests for this.

        mBarHeightForWhichTickmarksWereCached = getHeight();

        // Generate tickmarks, neatly clustering any overlapping matches.
        mTickmarks = new ArrayList<Tickmark>(mMatches.length);
        int i = 0;
        Tickmark nextTickmark = tickmarkForRect(mMatches[i], false);
        float lastGroupEnd = -mMinGapBetweenStacks;
        while (i < mMatches.length) {
            // Find next cluster of overlapping tickmarks.
            List<Tickmark> cluster = new ArrayList<Tickmark>();
            cluster.add(nextTickmark);
            i++;
            while (i < mMatches.length) {
                nextTickmark = tickmarkForRect(mMatches[i], false);
                if (nextTickmark.mTop
                        <= cluster.get(cluster.size() - 1).mBottom + mMinGapBetweenStacks) {
                    cluster.add(nextTickmark);
                    i++;
                } else {
                    break;
                }
            }

            // Draw cluster.
            int cn = cluster.size();
            float minStart = lastGroupEnd + mMinGapBetweenStacks;
            lastGroupEnd = cluster.get(cn - 1).mBottom;
            float preferredStart =
                    lastGroupEnd - (cn - 1) * mStackedResultHeight - mResultMinHeight;
            float maxStart = cluster.get(0).mTop;
            float start = Math.round(MathUtils.clamp(preferredStart, minStart, maxStart));
            float scale =
                    start >= preferredStart
                            ? 1.0f
                            : (lastGroupEnd - start) / (lastGroupEnd - preferredStart);
            float spacing =
                    cn == 1 ? 0 : (lastGroupEnd - start - scale * mResultMinHeight) / (cn - 1);
            for (int j = 0; j < cn; j++) {
                Tickmark tickmark = cluster.get(j);
                tickmark.mTop = start + j * spacing;
                if (j != cn - 1) {
                    tickmark.mBottom = tickmark.mTop + scale * mResultMinHeight;
                }
                mTickmarks.add(tickmark);
            }
        }
    }

    private Tickmark tickmarkForRect(RectF r, boolean active) {
        // Ratio of results bar height to page height
        float vScale = mBarHeightForWhichTickmarksWereCached - 2 * mBarVerticalPadding;
        Tickmark tickmark =
                new Tickmark(
                        r.top * vScale + mBarVerticalPadding,
                        r.bottom * vScale + mBarVerticalPadding);
        return expandTickmarkToMinHeight(tickmark, active);
    }

    private Tickmark expandTickmarkToMinHeight(Tickmark tickmark, boolean active) {
        int minHeight = active ? mActiveMinHeight : mResultMinHeight;
        float missingHeight = minHeight - tickmark.height();
        if (missingHeight > 0) {
            return new Tickmark(
                    tickmark.mTop - missingHeight / 2.0f, tickmark.mBottom + missingHeight / 2.0f);
        }
        return tickmark;
    }

    /** Like android.graphics.RectF, but without a left or right. */
    private class Tickmark implements Comparable<Tickmark> {
        float mTop;
        float mBottom;

        Tickmark(float top, float bottom) {
            this.mTop = top;
            this.mBottom = bottom;
        }

        float height() {
            return mBottom - mTop;
        }

        float centerY() {
            return (mTop + mBottom) * 0.5f;
        }

        RectF toRectF() {
            int leftMargin = getLeftMargin();
            RectF rect = new RectF(leftMargin, mTop, leftMargin + mBarDrawWidth, mBottom);
            rect.inset(2.0f, 0.5f);
            rect.offset(LocalizationUtils.isLayoutRtl() ? -0.5f : 0.5f, 0);
            return rect;
        }

        @Override
        public int compareTo(Tickmark other) {
            return Float.compare(centerY(), other.centerY());
        }
    }
}
