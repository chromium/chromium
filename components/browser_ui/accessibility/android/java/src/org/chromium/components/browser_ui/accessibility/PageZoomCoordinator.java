// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.components.browser_ui.accessibility;

import android.view.View;
import android.view.ViewGroup.MarginLayoutParams;
import android.view.animation.Animation;
import android.view.animation.AnimationUtils;

import androidx.annotation.Nullable;

import org.chromium.base.ResettersForTesting;
import org.chromium.content_public.browser.BrowserContextHandle;
import org.chromium.content_public.browser.ContentFeatureList;
import org.chromium.content_public.browser.ContentFeatureMap;
import org.chromium.content_public.browser.LoadCommittedDetails;
import org.chromium.content_public.browser.Visibility;
import org.chromium.content_public.browser.WebContents;
import org.chromium.content_public.browser.WebContentsObserver;
import org.chromium.ui.modelutil.PropertyModel;
import org.chromium.ui.modelutil.PropertyModelChangeProcessor;

/**
 * Coordinator for the page zoom feature. Created by the |RootUiCoordinator| and acts as the
 * public API for the component. Classes outside the component wishing to interact with page
 * zoom should be calling methods in this class only.
 */
public class PageZoomCoordinator {
    private final PageZoomCoordinatorDelegate mDelegate;
    private final PropertyModel mModel;
    private final PageZoomMediator mMediator;

    private WebContentsObserver mWebContentsObserver;
    private int mBottomControlsOffset;
    private Runnable mDismissalCallback;

    private View mView;
    private BrowserContextHandle mBrowserContextHandle;

    private static Boolean sShouldShowMenuItemForTesting;

    public PageZoomCoordinator(PageZoomCoordinatorDelegate delegate) {
        mDelegate = delegate;
        mModel = new PropertyModel.Builder(PageZoomProperties.ALL_KEYS).build();
        mModel.set(PageZoomProperties.USER_INTERACTION_CALLBACK, this::onViewInteraction);
        mMediator = new PageZoomMediator(mModel);
        mDismissalCallback = () -> hide();
    }

    /**
     * Returns true if the AppMenu item for Zoom should be displayed, false otherwise.
     * @return boolean
     */
    public static boolean shouldShowMenuItem() {
        if (sShouldShowMenuItemForTesting != null) return sShouldShowMenuItemForTesting;
        return PageZoomUtils.shouldShowZoomMenuItem();
    }

    /**
     * Show the zoom feature UI to the user.
     * @param webContents   WebContents that this zoom UI will control.
     */
    public void show(WebContents webContents) {
        PageZoomUma.logAppMenuSliderOpenedHistogram();

        // If inflating for the first time or showing from hidden, start animation
        if (mView == null) {
            // If the view has not been created, lazily inflate from the view stub.
            mView = mDelegate.getZoomControlView();
            PropertyModelChangeProcessor.create(mModel, mView, PageZoomViewBinder::bind);
            mView.startAnimation(getInAnimation());
        } else if (mView.getVisibility() != View.VISIBLE) {
            mView.setVisibility(View.VISIBLE);
            mView.startAnimation(getInAnimation());
        }

        if (mBrowserContextHandle == null) {
            mBrowserContextHandle = mDelegate.getBrowserContextHandle();
        }

        mModel.set(
                PageZoomProperties.DEFAULT_ZOOM_FACTOR,
                PageZoomUtils.getDefaultZoomLevelAsZoomFactor(mBrowserContextHandle));

        adjustPadding();

        // Consume hover events so screen readers do not select web contents behind slider.
        mView.setOnHoverListener((v, event) -> true);

        mModel.set(
                PageZoomProperties.RESET_ZOOM_VISIBLE,
                ContentFeatureMap.isEnabled(
                        ContentFeatureList.ACCESSIBILITY_PAGE_ZOOM_ENHANCEMENTS));

        // Adjust bottom margin for any bottom controls
        setBottomMargin(mBottomControlsOffset);

        mMediator.setWebContents(webContents);
        mWebContentsObserver =
                new WebContentsObserver(webContents) {
                    @Override
                    public void navigationEntryCommitted(LoadCommittedDetails details) {
                        // When navigation occurs (i.e. navigate to another link, forward/backward
                        // navigation), hide the dialog Only on navigationEntryCommitted to avoid
                        // premature dismissal during transient didStartNavigation events
                        hide();
                    }

                    @Override
                    public void onVisibilityChanged(@Visibility int visibility) {
                        if (visibility != Visibility.VISIBLE) {
                            // When the web contents are hidden or occluded (i.e. navigate to
                            // another tab), hide the dialog
                            hide();
                        }
                    }

                    @Override
                    public void onWebContentsLostFocus() {
                        // When the web contents loses focus (i.e. omnibox selected), hide the
                        // dialog
                        hide();
                    }
                };

        onViewInteraction(null);
    }

    /** Hide the zoom feature UI from the user. */
    public void hide() {
        // TODO(mschillaci): Add a FrameLayout wrapper so the view can be removed.
        if (mView != null && mView.getVisibility() == View.VISIBLE) {
            Animation animation = getOutAnimation();
            mView.startAnimation(animation);
            mView.setVisibility(View.GONE);

            // Ensure that the user has set a zoom value during this session.
            double zoomValue = mMediator.latestZoomValue();
            if (zoomValue != 0.0) {
                mMediator.logZoomLevelUKM(zoomValue);
                PageZoomUma.logAppMenuSliderZoomLevelChangedHistogram();
                PageZoomUma.logAppMenuSliderZoomLevelValueHistogram(zoomValue);
            }
        }
    }

    /**
     * Handle when height of bottom controls changes
     *
     * @param bottomControlsOffset the height of the bottom controls (if they are visible) by which
     *         the slider should be offset in the y direction. 0 otherwise.
     */
    public void onBottomControlsHeightChanged(int bottomControlsOffset) {
        mBottomControlsOffset = bottomControlsOffset;

        // Set margin in case view is currently visible
        setBottomMargin(mBottomControlsOffset);
    }

    /** Clean-up views and children during destruction. */
    public void destroy() {
        if (mWebContentsObserver != null) {
            mWebContentsObserver.destroy();
        }

        if (mView != null) {
            mView.removeCallbacks(mDismissalCallback);
        }
    }

    /**
     * Used for testing only, allows a mocked value for the {@link shouldShowMenuItem} method.
     * @param isEnabled     Should show the menu item or not.
     */
    public static void setShouldShowMenuItemForTesting(@Nullable Boolean isEnabled) {
        sShouldShowMenuItemForTesting = isEnabled;
        ResettersForTesting.register(() -> sShouldShowMenuItemForTesting = null);
    }

    /** Handle when the user interacts with the view */
    private void onViewInteraction(Void unused) {
        mView.removeCallbacks(mDismissalCallback);
        mView.postDelayed(mDismissalCallback, PageZoomUtils.LAST_INTERACTION_DISMISSAL);
    }

    private Animation getInAnimation() {
        Animation a = AnimationUtils.makeInChildBottomAnimation(mView.getContext());
        return a;
    }

    private Animation getOutAnimation() {
        Animation a =
                AnimationUtils.loadAnimation(mView.getContext(), R.anim.slide_out_child_bottom);
        a.setStartTime(AnimationUtils.currentAnimationTimeMillis());
        return a;
    }

    private void setBottomMargin(int bottomOffset) {
        if (mView != null) {
            MarginLayoutParams layout = (MarginLayoutParams) mView.getLayoutParams();
            layout.setMargins(
                    layout.leftMargin,
                    layout.topMargin,
                    layout.rightMargin,
                    mView.getContext()
                                    .getResources()
                                    .getDimensionPixelSize(R.dimen.page_zoom_view_margins)
                            + bottomOffset);
        }
    }

    private void adjustPadding() {
        if (mView != null) {
            int displayWidth = mView.getContext().getResources().getDisplayMetrics().widthPixels;
            int maxMobileWidth =
                    mView.getContext()
                            .getResources()
                            .getDimensionPixelSize(R.dimen.page_zoom_view_tablet_mode_min_width);
            int defaultPadding =
                    mView.getContext()
                            .getResources()
                            .getDimensionPixelSize(R.dimen.page_zoom_view_padding);

            if (displayWidth > maxMobileWidth) {
                int maxWidth =
                        mView.getContext()
                                .getResources()
                                .getDimensionPixelSize(R.dimen.page_zoom_view_max_width);
                int padding = (displayWidth - maxWidth) / 2;
                mView.setPadding(padding, defaultPadding, padding, defaultPadding);
            } else {
                mView.setPadding(defaultPadding, defaultPadding, defaultPadding, defaultPadding);
            }
        }
    }
}
