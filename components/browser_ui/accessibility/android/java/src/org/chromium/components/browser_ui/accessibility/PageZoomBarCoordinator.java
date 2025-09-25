// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.components.browser_ui.accessibility;

import static org.chromium.build.NullUtil.assumeNonNull;

import android.view.View;
import android.view.ViewGroup.MarginLayoutParams;
import android.view.animation.Animation;
import android.view.animation.AnimationUtils;
import android.widget.LinearLayout;
import android.widget.LinearLayout.LayoutParams;

import org.chromium.build.annotations.NullMarked;
import org.chromium.build.annotations.Nullable;
import org.chromium.content_public.browser.LoadCommittedDetails;
import org.chromium.content_public.browser.Visibility;
import org.chromium.content_public.browser.WebContents;
import org.chromium.content_public.browser.WebContentsObserver;
import org.chromium.ui.modelutil.PropertyModel;
import org.chromium.ui.modelutil.PropertyModelChangeProcessor;

/**
 * Coordinator for the page zoom feature. Created by the |RootUiCoordinator| and acts as the public
 * API for the component. Classes outside the component wishing to interact with page zoom should be
 * calling methods in this class only.
 */
@NullMarked
public class PageZoomBarCoordinator {
    private final PageZoomBarCoordinatorDelegate mDelegate;
    private final PropertyModel mModel;
    private final PageZoomManager mManager;
    private final PageZoomBarMediator mMediator;

    private @Nullable WebContentsObserver mWebContentsObserver;
    private int mBottomControlsOffset;
    private final Runnable mDismissalCallback;

    private @Nullable View mView;

    /**
     * @param delegate Used to interact with the coordinator.
     * @param manager The manager used to interact with the zoom functionality.
     * @param useSlider Whether the page zoom UI should use the material slider.
     */
    public PageZoomBarCoordinator(
            PageZoomBarCoordinatorDelegate delegate, PageZoomManager manager, boolean useSlider) {
        mDelegate = delegate;
        mManager = manager;
        mModel =
                new PropertyModel.Builder(PageZoomProperties.ALL_KEYS)
                        .with(PageZoomProperties.USE_SLIDER, useSlider)
                        .build();
        mMediator = new PageZoomBarMediator(mModel, mManager, this::onViewInteraction);
        mDismissalCallback = () -> hide();
    }

    /**
     * Show the zoom feature UI to the user.
     *
     * @param webContents WebContents that this zoom UI will control.
     */
    public void show(WebContents webContents) {
        PageZoomUma.logAppMenuSliderOpenedHistogram();

        // If inflating for the first time or showing from hidden, start animation
        if (mView == null) {
            // If the view has not been created, lazily inflate from the view stub.
            mView = mDelegate.getZoomControlView();
            PropertyModelChangeProcessor.create(mModel, mView, PageZoomBarViewBinder::bind);
            mView.startAnimation(getInAnimation());
        } else if (mView.getVisibility() != View.VISIBLE) {
            mView.setVisibility(View.VISIBLE);
            mView.startAnimation(getInAnimation());
        }

        adjustPadding();
        adjustResetSymmetry();

        // Consume hover events so screen readers do not select web contents behind slider.
        mView.setOnHoverListener((v, event) -> true);

        // Adjust bottom margin for any bottom controls
        setBottomMargin(mBottomControlsOffset);

        mMediator.pushProperties();
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
        if (mView != null) {
            mView.removeCallbacks(mDismissalCallback);
        }

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
     *     the slider should be offset in the y direction. 0 otherwise.
     */
    public void onBottomControlsHeightChanged(int bottomControlsOffset) {
        mBottomControlsOffset = bottomControlsOffset;

        // Set margin in case view is currently visible
        setBottomMargin(mBottomControlsOffset);
    }

    /** Clean-up views and children during destruction. */
    public void destroy() {
        if (mWebContentsObserver != null) {
            mWebContentsObserver.observe(null);
        }

        if (mView != null) {
            mView.removeCallbacks(mDismissalCallback);
        }
    }

    /** Used for testing only, resets the zoom level to 100%. */
    public void resetZoomForTesting() {
        mMediator.handleResetClicked();
    }

    /** Handle when the user interacts with the view */
    private void onViewInteraction(@Nullable Void unused) {
        assumeNonNull(mView);
        mView.removeCallbacks(mDismissalCallback);
        mView.postDelayed(mDismissalCallback, PageZoomUtils.LAST_INTERACTION_DISMISSAL);
    }

    private Animation getInAnimation() {
        assumeNonNull(mView);
        Animation a = AnimationUtils.makeInChildBottomAnimation(mView.getContext());
        return a;
    }

    private Animation getOutAnimation() {
        assumeNonNull(mView);
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

    private void adjustResetSymmetry() {
        assumeNonNull(mView);

        // Both the 'Reset' button and current zoom value text have wrap_content LayoutParams,
        // and we want to set them each to the max of the two to maintain symmetry.
        LayoutParams text_params =
                (LinearLayout.LayoutParams)
                        mView.findViewById(R.id.page_zoom_current_zoom_level).getLayoutParams();
        LayoutParams reset_params =
                (LinearLayout.LayoutParams)
                        mView.findViewById(R.id.page_zoom_reset_zoom_button).getLayoutParams();

        LayoutParams bounding_params =
                new LayoutParams(
                        Math.max(text_params.width, reset_params.width),
                        Math.max(text_params.height, reset_params.height));

        mView.findViewById(R.id.page_zoom_current_zoom_level).setLayoutParams(bounding_params);
        mView.findViewById(R.id.page_zoom_reset_zoom_button).setLayoutParams(bounding_params);
    }
}
