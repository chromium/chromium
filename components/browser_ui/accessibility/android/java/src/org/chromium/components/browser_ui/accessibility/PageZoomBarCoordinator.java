// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.components.browser_ui.accessibility;

import static org.chromium.build.NullUtil.assumeNonNull;

import android.annotation.SuppressLint;
import android.view.View;
import android.view.animation.Animation;
import android.view.animation.AnimationUtils;
import android.widget.LinearLayout;
import android.widget.LinearLayout.LayoutParams;

import org.chromium.base.Callback;
import org.chromium.base.supplier.MonotonicObservableSupplier;
import org.chromium.build.annotations.NullMarked;
import org.chromium.build.annotations.Nullable;
import org.chromium.components.browser_ui.bottomsheet.BottomSheetController;
import org.chromium.components.browser_ui.bottomsheet.BottomSheetObserver;
import org.chromium.components.browser_ui.bottomsheet.EmptyBottomSheetObserver;
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
    private final MonotonicObservableSupplier<BottomSheetController> mBottomSheetControllerSupplier;
    private @Nullable BottomSheetController mBottomSheetController;
    private @Nullable BottomSheetObserver mBottomSheetObserver;
    private final Callback<BottomSheetController> mBottomSheetControllerCallback =
            this::onBottomSheetControllerAvailable;

    /**
     * @param delegate Used to interact with the coordinator.
     * @param manager The manager used to interact with the zoom functionality.
     * @param useSlider Whether the page zoom UI should use the material slider.
     * @param bottomSheetControllerSupplier Supplier for the BottomSheetController.
     */
    public PageZoomBarCoordinator(
            PageZoomBarCoordinatorDelegate delegate,
            PageZoomManager manager,
            boolean useSlider,
            MonotonicObservableSupplier<BottomSheetController> bottomSheetControllerSupplier) {
        mDelegate = delegate;
        mManager = manager;
        mModel =
                new PropertyModel.Builder(PageZoomProperties.ALL_KEYS)
                        .with(PageZoomProperties.USE_SLIDER, useSlider)
                        .build();
        mMediator = new PageZoomBarMediator(mModel, mManager, this::onViewInteraction);
        mDismissalCallback = () -> hide();

        mBottomSheetControllerSupplier = bottomSheetControllerSupplier;
        mBottomSheetControllerSupplier.addSyncObserverAndCallIfNonNull(
                mBottomSheetControllerCallback);
    }

    private void onBottomSheetControllerAvailable(BottomSheetController controller) {
        if (mBottomSheetController != null && mBottomSheetObserver != null) {
            mBottomSheetController.removeObserver(mBottomSheetObserver);
        }
        mBottomSheetController = controller;
        if (mBottomSheetController != null) {
            if (mBottomSheetObserver == null) {
                mBottomSheetObserver =
                        new EmptyBottomSheetObserver() {
                            @Override
                            public void onSheetOpened(int reason) {
                                hide();
                            }

                            @Override
                            public void onSheetOffsetChanged(float heightFraction, float offsetPx) {
                                updateTranslation();
                            }
                        };
            }
            mBottomSheetController.addObserver(mBottomSheetObserver);
            if (mBottomSheetController.isSheetOpen()) {
                hide();
            }
        }
    }

    /**
     * Show the zoom feature UI to the user.
     *
     * @param webContents WebContents that this zoom UI will control.
     */
    @SuppressLint("ClickableViewAccessibility")
    public void show(@Nullable WebContents webContents) {
        // If a bottom sheet is currently expanded above peek, do not show the zoom bar.
        if (mBottomSheetController != null && mBottomSheetController.isSheetOpen()) {
            return;
        }

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

        // Consume touch events so they do not fall through to the web contents behind.
        mView.setOnTouchListener((v, event) -> true);

        // Consume generic motion events so they do not fall through to the web contents behind.
        mView.setOnGenericMotionListener((v, event) -> true);

        // Adjust translation for any bottom controls or bottom sheet
        updateTranslation();

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
        updateTranslation();
    }

    private void updateTranslation() {
        int sheetOffset = 0;
        boolean isAnchored = false;
        boolean actsAsBrowserControls = false;
        if (mBottomSheetController != null) {
            sheetOffset = mBottomSheetController.getCurrentOffset();
            isAnchored = mBottomSheetController.isAnchoredToBottomControls();
            actsAsBrowserControls = mDelegate.isSheetActingAsBrowserControls();
        }

        int totalOffset;
        // When the bottom sheet is anchored to the bottom controls, it sits on top of them.
        // Therefore, we must sum their offsets to clear both. Otherwise, they overlap and
        // we take the maximum.
        if (isAnchored) {
            if (actsAsBrowserControls) {
                // If the sheet acts as browser controls, mBottomControlsOffset already
                // includes the sheet's peek height. In PEEK state (which is the only state
                // we care about since the bar is dismissed in other states), this is equal
                // to the sheet's offset. So we don't need to sum them.
                totalOffset = mBottomControlsOffset;
            } else {
                totalOffset = mBottomControlsOffset + sheetOffset;
            }
        } else {
            totalOffset = Math.max(mBottomControlsOffset, sheetOffset);
        }
        setTranslation(totalOffset);
    }

    /** Clean-up views and children during destruction. */
    public void destroy() {
        mBottomSheetControllerSupplier.removeObserver(mBottomSheetControllerCallback);
        if (mBottomSheetController != null && mBottomSheetObserver != null) {
            mBottomSheetController.removeObserver(mBottomSheetObserver);
        }
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

    private void setTranslation(int bottomOffset) {
        if (mView != null) {
            mView.setTranslationY(-bottomOffset);
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
        LayoutParams textParams =
                (LinearLayout.LayoutParams)
                        mView.findViewById(R.id.page_zoom_current_zoom_level).getLayoutParams();
        LayoutParams resetParams =
                (LinearLayout.LayoutParams)
                        mView.findViewById(R.id.page_zoom_reset_zoom_button).getLayoutParams();

        LayoutParams boundingParams =
                new LayoutParams(
                        Math.max(textParams.width, resetParams.width),
                        Math.max(textParams.height, resetParams.height));

        mView.findViewById(R.id.page_zoom_current_zoom_level).setLayoutParams(boundingParams);
        mView.findViewById(R.id.page_zoom_reset_zoom_button).setLayoutParams(boundingParams);
    }
}
