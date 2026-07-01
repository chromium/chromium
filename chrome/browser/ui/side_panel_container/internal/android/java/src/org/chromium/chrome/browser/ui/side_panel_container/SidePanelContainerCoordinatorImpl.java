// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.ui.side_panel_container;

import static org.chromium.chrome.browser.ui.side_panel.SidePanelUtils.log;

import android.app.Activity;
import android.graphics.Rect;
import android.view.LayoutInflater;
import android.view.View;
import android.view.ViewGroup.LayoutParams;
import android.widget.FrameLayout;

import androidx.annotation.Px;
import androidx.annotation.VisibleForTesting;

import org.chromium.base.ResettersForTesting;
import org.chromium.base.ThreadUtils;
import org.chromium.build.annotations.NullMarked;
import org.chromium.build.annotations.Nullable;
import org.chromium.chrome.browser.ui.side_panel.SidePanelCoordinatorAndroid;
import org.chromium.chrome.browser.ui.side_panel_container.dev.SidePanelDevFeature;
import org.chromium.chrome.browser.ui.side_panel_container.dev.SidePanelDevFeatureImpl;
import org.chromium.chrome.browser.ui.side_ui.SideUiContainer;
import org.chromium.chrome.browser.ui.side_ui.SideUiCoordinator;
import org.chromium.chrome.browser.ui.side_ui.SideUiCoordinator.AnchorSide;
import org.chromium.chrome.browser.ui.side_ui.SideUiCoordinator.SideUiId;
import org.chromium.chrome.browser.ui.side_ui.SideUiCoordinator.UiUpdateRequest;
import org.chromium.ui.base.ViewUtils;

/** Implementation of {@link SidePanelContainerCoordinator}. */
@NullMarked
final class SidePanelContainerCoordinatorImpl
        implements SidePanelContainerCoordinator, SideUiContainer {
    private static final String TAG = "SidePanelContainerCoordinatorImpl";

    private static final @AnchorSide int SIDE_PANEL_DEFAULT_ANCHOR_SIDE = AnchorSide.RIGHT;

    /** Used to override the return value of {@link #hasContentToShow()} for tests. */
    private static @Nullable Boolean sHasContentToShowForTesting;

    private final Activity mParentActivity;
    private final FrameLayout mContainerView;
    private final SideUiCoordinator mSideUiCoordinator;

    // TODO(crbug.com/496407828): Use this to notify native side of events like panel opened/closed.
    private @Nullable SidePanelCoordinatorAndroid mSidePanelCoordinatorAndroid;

    private @Nullable SidePanelDevFeatureImpl mSidePanelPureJavaDevFeature;

    private @Nullable SidePanelContent mCurrentContent;

    /**
     * Whether {@link #onWillAutoClose} is running.
     *
     * <p>This flag prevents {@link #onWillAutoClose} from triggering another UI update, which isn't
     * allowed.
     *
     * <p>The C++ {@code SidePanelCoordinatorAndroid} calls {@link #startClosingPanel} during {@link
     * #onWillAutoClose}. {@link #startClosingPanel} is also for non-auto-closing cases where a call
     * to {@link SideUiCoordinator#updateUi} is required, so we need this flag to avoid calling
     * {@link SideUiCoordinator#updateUi} for the auto-close case.
     *
     * <p>TODO(crbug.com/527985639): Refactor the C++ side and remove this flag.
     */
    private boolean mIsPreparingForAutoClose;

    /**
     * Whether {@link #onWillAutoRestore} is running.
     *
     * <p>This flag prevents {@link #onWillAutoRestore} from triggering another UI update, which
     * isn't allowed.
     *
     * <p>TODO(crbug.com/527985639): Refactor the C++ side and remove this flag.
     *
     * @see #mIsPreparingForAutoClose
     */
    private boolean mIsPreparingForAutoRestore;

    /**
     * Constructs a concrete implementation of the SidePanelContainerCoordinator interface.
     *
     * @param parentActivity Parent Activity that will own this instance.
     * @param sideUiCoordinator Coordinator for the Side Panel UI anchoring view.
     */
    SidePanelContainerCoordinatorImpl(
            Activity parentActivity, SideUiCoordinator sideUiCoordinator) {
        log(TAG, "constructor", parentActivity, sideUiCoordinator);
        mParentActivity = parentActivity;
        mSideUiCoordinator = sideUiCoordinator;
        mContainerView =
                (FrameLayout)
                        LayoutInflater.from(mParentActivity)
                                .inflate(R.layout.side_panel_container, /* root= */ null);
    }

    ///////////////////////////////////////////////////////////////////////////////////////////////
    //              Start of SidePanelContainerCoordinator Implementation                        //
    ///////////////////////////////////////////////////////////////////////////////////////////////

    @Override
    public void init(
            SidePanelCoordinatorAndroid sidePanelCoordinatorAndroid,
            @Nullable SidePanelDevFeature sidePanelDevFeature) {
        log(TAG, "init");
        ThreadUtils.assertOnUiThread();
        mSideUiCoordinator.registerSideUiContainer(this);

        // SidePanelCoordinatorAndroid connects the Java UI with the state management logic in C++.
        // We should _not_ initialize SidePanelCoordinatorAndroid for the pure-Java dev feature,
        // otherwise the pure-Java dev feature will drive the C++ side into invalid states.
        if (sidePanelDevFeature instanceof SidePanelDevFeatureImpl) {
            mSidePanelPureJavaDevFeature = (SidePanelDevFeatureImpl) sidePanelDevFeature;
        } else {
            mSidePanelCoordinatorAndroid = sidePanelCoordinatorAndroid;
            mSidePanelCoordinatorAndroid.init();
        }
    }

    @Override
    public void startOpeningPanel(
            SidePanelContent content,
            Runnable onPanelOpened,
            @Nullable Rect startingBounds,
            boolean suppressAnimations) {
        log(TAG, "startOpeningPanel", content, startingBounds, suppressAnimations);
        ThreadUtils.assertOnUiThread();

        // TODO(crbug.com/513302000): assert the side panel is currently closed.

        mCurrentContent = content;
        mContainerView.removeAllViews();
        mContainerView.addView(content.mView);

        assert !mIsPreparingForAutoClose;
        if (!mIsPreparingForAutoRestore) {
            mSideUiCoordinator.updateUi(
                    new UiUpdateRequest(SideUiId.SIDE_PANEL, suppressAnimations));
        }

        // TODO(crbug.com/496407828): Move this around so it actually runs after the animation is
        //  finished.
        onPanelOpened.run();
    }

    @Override
    public void startClosingPanel(Runnable onPanelClosed, boolean suppressAnimations) {
        log(TAG, "startClosingPanel", suppressAnimations);
        ThreadUtils.assertOnUiThread();

        assert !mIsPreparingForAutoRestore;
        if (!mIsPreparingForAutoClose) {
            mSideUiCoordinator.updateUi(
                    new UiUpdateRequest(SideUiId.SIDE_PANEL, suppressAnimations));
        }

        // TODO(crbug.com/496407828): Move this around so it actually runs after the animation is
        //  finished.
        onPanelClosed.run();
    }

    @Override
    public void startReplacingPanelContent(
            SidePanelContent newContent, Runnable onPanelContentReplaced) {
        log(TAG, "startReplacingPanelContent", newContent);
        ThreadUtils.assertOnUiThread();

        // TODO(crbug.com/513302000): assert the side panel is currently open.
        // TODO(crbug.com/513302000): assert the side panel isn't preparing for auto-restore/close.

        mCurrentContent = newContent;

        // TODO(crbug.com/505895733): Delay removing the old View to prevent UI flickers.
        mContainerView.removeAllViews();
        mContainerView.addView(newContent.mView);
        onPanelContentReplaced.run();
    }

    @Override
    public boolean isShowing(SidePanelContent sidePanelContent) {
        log(TAG, "isShowing", sidePanelContent);
        ThreadUtils.assertOnUiThread();
        return sidePanelContent == mCurrentContent;
    }

    @Override
    public @Nullable View getContentView() {
        ThreadUtils.assertOnUiThread();
        return mCurrentContent != null ? mCurrentContent.mView : null;
    }

    @Override
    public void destroy() {
        log(TAG, "destroy");
        ThreadUtils.assertOnUiThread();
        mSideUiCoordinator.unregisterSideUiContainer(this);
    }

    @Override
    public View getViewForTesting() {
        log(TAG, "getViewForTesting");
        ThreadUtils.assertOnUiThread();
        return mContainerView;
    }

    ///////////////////////////////////////////////////////////////////////////////////////////////
    //              End of SidePanelContainerCoordinator Implementation                          //
    ///////////////////////////////////////////////////////////////////////////////////////////////

    ///////////////////////////////////////////////////////////////////////////////////////////////
    //              Start of SideUiContainer Implementation                                      //
    ///////////////////////////////////////////////////////////////////////////////////////////////

    @Override
    public View getView() {
        log(TAG, "getView");
        ThreadUtils.assertOnUiThread();
        return mContainerView;
    }

    @Override
    public @SideUiId int getSideUiId() {
        return SideUiId.SIDE_PANEL;
    }

    @Override
    @AnchorSide
    public int getAnchorSide() {
        log(TAG, "getAnchorSide");
        ThreadUtils.assertOnUiThread();
        return SIDE_PANEL_DEFAULT_ANCHOR_SIDE;
    }

    @Override
    @Px
    public int determineShowableWidth(@Px int availableWidth, @Px int windowWidth) {
        log(TAG, "determineShowableWidth", availableWidth, windowWidth);
        ThreadUtils.assertOnUiThread();

        int availableWidthDp = ViewUtils.pxToDp(mParentActivity, availableWidth);
        int windowWidthDp = ViewUtils.pxToDp(mParentActivity, windowWidth);

        int horizontalPaddingDp =
                ViewUtils.pxToDp(
                        mParentActivity,
                        mContainerView.getPaddingLeft() + mContainerView.getPaddingRight());
        int minSidePanelContainerWidthDp = horizontalPaddingDp + MIN_SIDE_PANEL_CONTENT_WIDTH_DP;

        int showableWidthDp =
                determineShowableWidthDp(
                        availableWidthDp, windowWidthDp, minSidePanelContainerWidthDp);
        return ViewUtils.dpToPx(mParentActivity, showableWidthDp);
    }

    @Override
    public boolean hasContentToShow() {
        ThreadUtils.assertOnUiThread();
        if (sHasContentToShowForTesting != null) {
            return sHasContentToShowForTesting;
        }

        // The pure-Java dev feature doesn't use SidePanelCoordinatorAndroid since
        // SidePanelCoordinatorAndroid is a bridge to the C++ side panel state management.
        if (mSidePanelPureJavaDevFeature != null) {
            return mSidePanelPureJavaDevFeature.hasDevContentToShow();
        }

        if (mSidePanelCoordinatorAndroid != null) {
            return mSidePanelCoordinatorAndroid.hasContentToShow();
        }

        return false;
    }

    @Override
    public void setWidth(@Px int width) {
        log(TAG, "setWidth", width);
        ThreadUtils.assertOnUiThread();

        LayoutParams layoutParams = mContainerView.getLayoutParams();
        assert layoutParams != null
                : "setWidth() should be called after the container View is attached";
        assert layoutParams.height == LayoutParams.MATCH_PARENT
                : "the container View's height should match its parent";

        if (layoutParams.width != width) {
            layoutParams.width = width;
            mContainerView.setLayoutParams(layoutParams);
        }

        // Remove the content if setting the width the 0 (i.e. hiding the panel).
        if (width == 0) {
            mContainerView.removeAllViews();
            mCurrentContent = null;
        }

        // TODO(http://crbug.com/488047364): Notify the SidePanelContent View of the width change.
    }

    @Override
    public void onUiUpdateCompleted(@Px int oldWidth, @Px int newWidth) {}

    @Override
    public void onWillAutoClose() {
        // The pure-Java dev feature doesn't need onWillAutoClose() or SidePanelCoordinatorAndroid.
        // SidePanelCoordinatorAndroid is a bridge to the C++ side panel state management.
        if (mSidePanelPureJavaDevFeature != null) {
            return;
        }

        if (mSidePanelCoordinatorAndroid != null) {
            mIsPreparingForAutoClose = true;
            mSidePanelCoordinatorAndroid.onWillAutoClose();
            mIsPreparingForAutoClose = false;
        }
    }

    @Override
    public void onWillAutoRestore() {
        // The pure-Java dev feature doesn't need onWillAutoRestore() or
        // SidePanelCoordinatorAndroid.
        // SidePanelCoordinatorAndroid is a bridge to the C++ side panel state management.
        if (mSidePanelPureJavaDevFeature != null) {
            return;
        }

        if (mSidePanelCoordinatorAndroid != null) {
            mIsPreparingForAutoRestore = true;
            mSidePanelCoordinatorAndroid.onWillAutoRestore();
            mIsPreparingForAutoRestore = false;
        }
    }

    ///////////////////////////////////////////////////////////////////////////////////////////////
    //              End of SideUiContainer Implementation                                        //
    ///////////////////////////////////////////////////////////////////////////////////////////////

    /**
     * Returns the final width (in dp) of the side panel container given the available width in the
     * window, the window width, and the minimum side panel container width.
     */
    @VisibleForTesting
    static int determineShowableWidthDp(
            int availableWidthDp, int windowWidthDp, int minSidePanelContainerWidthDp) {
        // 1. Check if we can use the fixed, larger width.
        if (windowWidthDp >= MIN_WINDOW_WIDTH_DP_FOR_WIDE_SIDE_PANEL) {
            assert availableWidthDp >= WIDE_SIDE_PANEL_WIDTH_DP;
            return WIDE_SIDE_PANEL_WIDTH_DP;
        }

        // 2. Check if we can use the fixed, smaller width.
        if (availableWidthDp >= NARROW_SIDE_PANEL_WIDTH_DP) {
            return NARROW_SIDE_PANEL_WIDTH_DP;
        }

        // 3. If we can't use the fixed, smaller width, but the available space is more than the
        // minimum container width, we'll fill the available space.
        if (availableWidthDp >= minSidePanelContainerWidthDp) {
            return availableWidthDp;
        }

        // 4. Return 0 if available space can't accommodate the minimum side panel width.
        return 0;
    }

    static void setHasContentToShowForTesting(boolean hasContentToShow) {
        sHasContentToShowForTesting = hasContentToShow;
        ResettersForTesting.register(() -> sHasContentToShowForTesting = null);
    }
}
