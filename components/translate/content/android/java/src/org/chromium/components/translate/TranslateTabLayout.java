// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.components.translate;

import android.animation.Animator;
import android.animation.AnimatorListenerAdapter;
import android.animation.ObjectAnimator;
import android.annotation.SuppressLint;
import android.content.Context;
import android.content.res.TypedArray;
import android.util.AttributeSet;
import android.view.LayoutInflater;
import android.view.MotionEvent;

import androidx.annotation.NonNull;

import com.google.android.material.tabs.TabLayout;

import org.chromium.base.StrictModeContext;
import org.chromium.ui.interpolators.Interpolators;

/** TabLayout shown in the TranslateCompactInfoBar. */
public class TranslateTabLayout extends TabLayout {
    /** The tab in which a spinning progress bar is showing. */
    private Tab mTabShowingProgressBar;

    /** The amount of waiting time before starting the scrolling animation. */
    private static final long START_POSITION_WAIT_DURATION_MS = 1000;

    /** The amount of time it takes to scroll to the end during the scrolling animation. */
    private static final long SCROLL_DURATION_MS = 300;

    /** We define the keyframes of the scrolling animation in this object. */
    ObjectAnimator mScrollToEndAnimator;

    /** Start padding of a Tab.  Used for width calculation only.  Will not be applied to views. */
    private int mTabPaddingStart;

    /** End padding of a Tab.  Used for width calculation only.  Will not be applied to views. */
    private int mTabPaddingEnd;

    /** Constructor for inflating from XML. */
    @SuppressLint("CustomViewStyleable") // TODO(crbug.com/40560764): Remove and fix.
    public TranslateTabLayout(Context context, AttributeSet attrs) {
        super(context, attrs);

        TypedArray a =
                context.obtainStyledAttributes(
                        attrs,
                        R.styleable.TabLayout,
                        0,
                        R.style.Widget_MaterialComponents_TabLayout);
        mTabPaddingStart =
                mTabPaddingEnd = a.getDimensionPixelSize(R.styleable.TabLayout_tabPadding, 0);
        mTabPaddingStart =
                a.getDimensionPixelSize(R.styleable.TabLayout_tabPaddingStart, mTabPaddingStart);
        mTabPaddingEnd =
                a.getDimensionPixelSize(R.styleable.TabLayout_tabPaddingEnd, mTabPaddingEnd);
    }

    /**
     * Add new Tabs with title strings.
     * @param titles Titles of the tabs to be added.
     */
    public void addTabs(CharSequence... titles) {
        for (CharSequence title : titles) {
            addTabWithTitle(title);
        }
    }

    /**
     * Add a new Tab with the title string.
     * @param tabTitle Title string of the new tab.
     */
    public void addTabWithTitle(CharSequence tabTitle) {
        TranslateTabContent tabContent;
        // LayoutInflater may trigger accessing the disk.
        try (StrictModeContext ignored = StrictModeContext.allowDiskReads()) {
            tabContent =
                    (TranslateTabContent)
                            LayoutInflater.from(getContext())
                                    .inflate(R.layout.infobar_translate_tab_content, this, false);
        }
        // Set text color using tabLayout's ColorStateList.  So that the title text will change
        // color when selected and unselected.
        tabContent.setTextColor(getTabTextColors());
        tabContent.setText(tabTitle);

        Tab tab = newTab();
        tab.setCustomView(tabContent);
        tab.setContentDescription(tabTitle);
        super.addTab(tab);
    }

    /**
     * Replace the title string of a tab.
     * @param tabPos   The position of the tab to modify.
     * @param tabTitle The new title string.
     */
    public void replaceTabTitle(int tabPos, CharSequence tabTitle) {
        if (tabPos < 0 || tabPos >= getTabCount()) {
            return;
        }
        Tab tab = getTabAt(tabPos);
        ((TranslateTabContent) tab.getCustomView()).setText(tabTitle);
        tab.setContentDescription(tabTitle);
    }

    /**
     * Show the spinning progress bar on a specified tab.
     * @param tabPos The position of the tab to show the progress bar.
     */
    public void showProgressBarOnTab(int tabPos) {
        if (tabPos < 0 || tabPos >= getTabCount() || mTabShowingProgressBar != null) {
            return;
        }
        mTabShowingProgressBar = getTabAt(tabPos);

        // TODO(martiw) See if we need to setContentDescription as "Translating" here.

        if (tabIsSupported(mTabShowingProgressBar)) {
            ((TranslateTabContent) mTabShowingProgressBar.getCustomView()).showProgressBar();
        }
    }

    /** Hide the spinning progress bar in the tabs. */
    public void hideProgressBar() {
        if (mTabShowingProgressBar == null) return;

        if (tabIsSupported(mTabShowingProgressBar)) {
            ((TranslateTabContent) mTabShowingProgressBar.getCustomView()).hideProgressBar();
        }

        mTabShowingProgressBar = null;
    }

    // Overrided to block children's touch event when showing progress bar.
    @Override
    public boolean onInterceptTouchEvent(MotionEvent ev) {
        // Allow touches to propagate to children only if the layout can be interacted with.
        if (mTabShowingProgressBar != null) {
            return true;
        }
        endScrollingAnimationIfPlaying();
        return super.onInterceptTouchEvent(ev);
    }

    /** Check if the tab is supported in TranslateTabLayout. */
    private boolean tabIsSupported(Tab tab) {
        return (tab.getCustomView() instanceof TranslateTabContent);
    }

    // Overrided to make sure only supported Tabs can be added.
    @Override
    public void addTab(@NonNull Tab tab, int position, boolean setSelected) {
        if (!tabIsSupported(tab)) {
            throw new IllegalArgumentException();
        }
        super.addTab(tab, position, setSelected);
    }

    // Overrided to make sure only supported Tabs can be added.
    @Override
    public void addTab(@NonNull Tab tab, boolean setSelected) {
        if (!tabIsSupported(tab)) {
            throw new IllegalArgumentException();
        }
        super.addTab(tab, setSelected);
    }

    /**
     * Calculate and return the width of a specified tab.  Tab doesn't provide a means of getting
     * the width so we need to calculate the width by summing up the tab paddings and content width.
     * @param position Tab position.
     * @return Tab's width in pixels.
     */
    private int getTabWidth(int position) {
        if (getTabAt(position) == null) return 0;
        return getTabAt(position).getCustomView().getWidth() + mTabPaddingStart + mTabPaddingEnd;
    }

    /**
     * Calculate the total width of all tabs and return it.
     * @return Total width of all tabs in pixels.
     */
    private int getTabsTotalWidth() {
        int totalWidth = 0;
        for (int i = 0; i < getTabCount(); i++) {
            totalWidth += getTabWidth(i);
        }
        return totalWidth;
    }

    /**
     * Calculate the maximum scroll distance (by subtracting layout width from total width of tabs)
     * and return it.
     * @return Maximum scroll distance in pixels.
     */
    private int maxScrollDistance() {
        int scrollDistance = getTabsTotalWidth() - getWidth();
        return scrollDistance > 0 ? scrollDistance : 0;
    }

    /** Perform the scrolling animation if this tablayout has any scrollable distance. */
    // TODO(crbug.com/40600572): Figure out whether setScrollX is actually available.
    @SuppressLint("ObjectAnimatorBinding")
    public void startScrollingAnimationIfNeeded() {
        int maxScrollDistance = maxScrollDistance();
        if (maxScrollDistance == 0) {
            return;
        }
        // The steps of the scrolling animation:
        //   1. wait for START_POSITION_WAIT_DURATION_MS.
        //   2. scroll to the end in SCROLL_DURATION_MS.
        mScrollToEndAnimator =
                ObjectAnimator.ofInt(
                        this,
                        "scrollX",
                        getLayoutDirection() == LAYOUT_DIRECTION_RTL ? 0 : maxScrollDistance);
        mScrollToEndAnimator.setStartDelay(START_POSITION_WAIT_DURATION_MS);
        mScrollToEndAnimator.setDuration(SCROLL_DURATION_MS);
        mScrollToEndAnimator.setInterpolator(Interpolators.DECELERATE_INTERPOLATOR);
        mScrollToEndAnimator.addListener(
                new AnimatorListenerAdapter() {
                    @Override
                    public void onAnimationEnd(Animator animation) {
                        mScrollToEndAnimator = null;
                    }
                });
        mScrollToEndAnimator.start();
    }

    /** End the scrolling animation if it is playing. */
    public void endScrollingAnimationIfPlaying() {
        if (mScrollToEndAnimator != null) mScrollToEndAnimator.end();
    }
}
