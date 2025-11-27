// Copyright 2015 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.toolbar.top;

import static org.chromium.build.NullUtil.assumeNonNull;

import android.content.Context;
import android.graphics.Canvas;
import android.graphics.Color;
import android.graphics.PorterDuff;
import android.graphics.Rect;
import android.graphics.Region;
import android.graphics.drawable.ColorDrawable;
import android.graphics.drawable.Drawable;
import android.graphics.drawable.LayerDrawable;
import android.os.Handler;
import android.os.Looper;
import android.util.AttributeSet;
import android.view.Gravity;
import android.view.MotionEvent;
import android.view.View;
import android.view.ViewGroup;
import android.view.ViewStub;

import androidx.annotation.IntDef;
import androidx.annotation.VisibleForTesting;
import androidx.coordinatorlayout.widget.CoordinatorLayout;
import androidx.core.content.res.ResourcesCompat;

import org.chromium.base.Callback;
import org.chromium.base.ObserverList;
import org.chromium.base.TraceEvent;
import org.chromium.base.library_loader.LibraryLoader;
import org.chromium.base.metrics.RecordHistogram;
import org.chromium.base.supplier.ObservableSupplier;
import org.chromium.base.supplier.ObservableSupplierImpl;
import org.chromium.base.supplier.OneshotSupplier;
import org.chromium.build.annotations.Initializer;
import org.chromium.build.annotations.NullMarked;
import org.chromium.build.annotations.Nullable;
import org.chromium.chrome.browser.browser_controls.BrowserStateBrowserControlsVisibilityDelegate;
import org.chromium.chrome.browser.flags.ChromeFeatureList;
import org.chromium.chrome.browser.fullscreen.FullscreenManager;
import org.chromium.chrome.browser.layouts.LayoutStateProvider;
import org.chromium.chrome.browser.layouts.LayoutType;
import org.chromium.chrome.browser.tab.Tab;
import org.chromium.chrome.browser.tasks.tab_management.TabUiThemeUtil;
import org.chromium.chrome.browser.toolbar.ConstraintsChecker;
import org.chromium.chrome.browser.toolbar.ControlContainer;
import org.chromium.chrome.browser.toolbar.R;
import org.chromium.chrome.browser.toolbar.ToolbarCaptureType;
import org.chromium.chrome.browser.toolbar.ToolbarDataProvider;
import org.chromium.chrome.browser.toolbar.ToolbarDataProvider.Observer;
import org.chromium.chrome.browser.toolbar.ToolbarFeatures;
import org.chromium.chrome.browser.toolbar.ToolbarHairlineView;
import org.chromium.chrome.browser.toolbar.ToolbarProgressBar;
import org.chromium.chrome.browser.toolbar.top.CaptureReadinessResult.TopToolbarBlockCaptureReason;
import org.chromium.components.browser_ui.desktop_windowing.AppHeaderState;
import org.chromium.components.browser_ui.desktop_windowing.DesktopWindowStateManager.AppHeaderObserver;
import org.chromium.components.browser_ui.widget.ClipDrawableProgressBar.DrawingInfo;
import org.chromium.components.browser_ui.widget.TouchEventObserver;
import org.chromium.components.browser_ui.widget.ViewResourceCoordinatorLayout;
import org.chromium.components.browser_ui.widget.gesture.SwipeGestureListener;
import org.chromium.components.browser_ui.widget.gesture.SwipeGestureListener.SwipeHandler;
import org.chromium.ui.KeyboardVisibilityDelegate;
import org.chromium.ui.base.DeviceFormFactor;
import org.chromium.ui.base.ViewUtils;
import org.chromium.ui.resources.Resource;
import org.chromium.ui.resources.dynamics.ViewResourceAdapter;
import org.chromium.ui.util.TokenHolder;
import org.chromium.ui.widget.OptimizedFrameLayout;

import java.lang.annotation.Retention;
import java.lang.annotation.RetentionPolicy;
import java.util.function.BooleanSupplier;
import java.util.function.Supplier;

/** Layout for the browser controls (omnibox, menu, tab strip, etc..). */
@NullMarked
public class ToolbarControlContainer extends OptimizedFrameLayout
        implements ControlContainer, AppHeaderObserver, Observer {
    private static final double SAMPLE_STALE_CAPTURE_PROBABILITY = 0.01;
    private static boolean sForceStaleCaptureHistogram;

    private boolean mIncognito;
    private boolean mMidVisibilityToggle;
    private boolean mIsCompositorInitialized;
    private @Nullable AppHeaderState mAppHeaderState;

    private Toolbar mToolbar;
    private ToolbarViewResourceCoordinatorLayout mToolbarContainer;

    private @Nullable SwipeGestureListener mSwipeGestureListener;
    private @Nullable OnDragListener mToolbarContainerDragListener;

    private boolean mIsAppInUnfocusedDesktopWindow;
    private int mToolbarLayoutHeight;
    private final Rect mToolbarCaptureSize = new Rect();

    private View mToolbarHairline;
    private ViewGroup mToolbarView;
    private boolean mShowLocationBarOnly;
    private @Nullable View mLocationBarView;
    private final ObserverList<TouchEventObserver> mTouchEventObservers = new ObserverList<>();
    private final Callback<Boolean> mOnXrSpaceModeChanged = this::onXrSpaceModeChanged;
    private final Callback<Resource> mOnResourceCaptureCallback = this::onToolbarCaptureUpdated;
    private @Nullable ObservableSupplier<Boolean> mXrSpaceModeObservableSupplier;
    private @Nullable ObservableSupplierImpl<Integer> mHeightChangedSupplier;
    private ToolbarDataProvider mToolbarDataProvider;

    /**
     * Constructs a new control container.
     *
     * <p>This constructor is used when inflating from XML.
     *
     * @param context The context used to build this view.
     * @param attrs The attributes used to determine how to construct this view.
     */
    public ToolbarControlContainer(Context context, AttributeSet attrs) {
        super(context, attrs);
    }

    @Override
    protected void onFinishInflate() {
        super.onFinishInflate();
        mToolbarHairline = findViewById(R.id.toolbar_hairline);
    }

    @Override
    public ViewResourceAdapter getToolbarResourceAdapter() {
        return mToolbarContainer.getResourceAdapter();
    }

    @Override
    public View getView() {
        return this;
    }

    @Override
    public void getProgressBarDrawingInfo(DrawingInfo drawingInfoOut) {
        if (mToolbar == null) return;
        // TODO(yusufo): Avoid casting to the layout without making the interface bigger.
        ToolbarProgressBar progressBar = mToolbar.getProgressBar();
        if (progressBar != null) progressBar.getDrawingInfo(drawingInfoOut);
    }

    @Override
    public int getToolbarBackgroundColor() {
        if (mToolbar == null) return 0;
        return mToolbar.getPrimaryColor();
    }

    @Override
    public int getToolbarHeight() {
        return mToolbarLayoutHeight;
    }

    @Override
    public int getToolbarHairlineHeight() {
        assert mToolbarHairline != null;
        return mToolbarHairline.getHeight();
    }

    @Override
    public void setSwipeHandler(SwipeHandler handler) {
        mSwipeGestureListener = new SwipeGestureListenerImpl(getContext(), handler);
    }

    @Override
    @Initializer
    public void initWithToolbar(int toolbarLayoutId, int toolbarLayoutHeightResId) {
        try (TraceEvent te = TraceEvent.scoped("ToolbarControlContainer.initWithToolbar")) {
            mToolbarContainer = findViewById(R.id.toolbar_container);
            mToolbarLayoutHeight = getResources().getDimensionPixelSize(toolbarLayoutHeightResId);
            ViewStub toolbarStub = findViewById(R.id.toolbar_stub);
            toolbarStub.setLayoutResource(toolbarLayoutId);
            View toolbar = toolbarStub.inflate();
            mutateHairlineLayoutParams().setAnchorId(toolbar.getId());
        }
    }

    @Override
    public void onTabOrModelChanged(boolean incognito) {
        if (!DeviceFormFactor.isNonMultiDisplayContextOnTablet(getContext())
                || getBackground() == null) {
            return;
        }

        if (mIncognito != incognito) {
            maybeUpdateTempTabStripDrawableBackground(incognito, mAppHeaderState);
            mIncognito = incognito;
        }
    }

    @Override
    protected void onSizeChanged(int newW, int newH, int oldW, int oldH) {
        if (newH != oldH && mHeightChangedSupplier != null) {
            mHeightChangedSupplier.set(newH);
        }
    }

    public void setOnHeightChangedListener(
            @Nullable ObservableSupplierImpl<Integer> heightChangedSupplier) {
        mHeightChangedSupplier = heightChangedSupplier;
    }

    public void onPageLoadStopped() {
        ((ToolbarViewResourceAdapter) getToolbarResourceAdapter()).onPageLoadStopped();
    }

    public void onContentViewScrollingStateChanged(boolean scrolling) {
        ((ToolbarViewResourceAdapter) getToolbarResourceAdapter())
                .onContentViewScrollingStateChanged(scrolling);
    }

    @Override
    public void setCompositorBackgroundInitialized() {
        mIsCompositorInitialized = true;
        setBackgroundResource(0);
    }

    @Override
    public CoordinatorLayout.LayoutParams mutateLayoutParams() {
        CoordinatorLayout.LayoutParams layoutParams =
                (CoordinatorLayout.LayoutParams) getLayoutParams();
        setLayoutParams(layoutParams);
        return layoutParams;
    }

    @Override
    public CoordinatorLayout.LayoutParams mutateHairlineLayoutParams() {
        CoordinatorLayout.LayoutParams hairlineParams =
                (CoordinatorLayout.LayoutParams) mToolbarHairline.getLayoutParams();
        mToolbarHairline.setLayoutParams(hairlineParams);
        return hairlineParams;
    }

    @Override
    public CoordinatorLayout.LayoutParams mutateToolbarLayoutParams() {
        CoordinatorLayout.LayoutParams toolbarLayoutParams =
                (CoordinatorLayout.LayoutParams) mToolbarView.getLayoutParams();
        mToolbarView.setLayoutParams(toolbarLayoutParams);
        return toolbarLayoutParams;
    }

    @Override
    public void toggleLocationBarOnlyMode(boolean showLocationBarOnly) {
        if (mShowLocationBarOnly == showLocationBarOnly) return;

        mShowLocationBarOnly = showLocationBarOnly;
        if (showLocationBarOnly) {
            mLocationBarView = mToolbar.removeLocationBarView();
            assert mLocationBarView != null
                    : "Trying to remove location bar view from toolbar when there is no location"
                            + " bar";
            mToolbar.getProgressBar().setVisibility(View.GONE);
            mToolbarView.setVisibility(View.GONE);
            mToolbarView.removeView(mLocationBarView);
            mToolbarContainer.addView(mLocationBarView);
            setBackgroundColor(mToolbarDataProvider.getPrimaryColor());
        } else {
            assert mLocationBarView != null
                    : "Trying to restore location bar view to toolbar without removing it first";
            mToolbar.getProgressBar().setVisibility(View.VISIBLE);
            mToolbarView.setVisibility(View.VISIBLE);
            mToolbarContainer.removeView(mLocationBarView);
            // CoordinatorLayout only updates its processed list of children at measure time, even
            // if a child is removed. This can cause problems if a reparented former child has a new
            // type of LayoutParams, triggering a ClassCastException. We work around this by forcing
            // a re-measure.
            mToolbarContainer.forceLayout();
            mToolbarContainer.measure(
                    mToolbarContainer.getMeasuredWidthAndState(),
                    mToolbarContainer.getMeasuredHeightAndState());
            mToolbar.restoreLocationBarView();
            setBackgroundColor(Color.TRANSPARENT);
        }
    }

    @Override
    public void addTouchEventObserver(TouchEventObserver observer) {
        mTouchEventObservers.addObserver(observer);
    }

    @Override
    public void removeTouchEventObserver(TouchEventObserver observer) {
        mTouchEventObservers.removeObserver(observer);
    }

    @Override
    public void destroy() {
        ((ToolbarViewResourceAdapter) getToolbarResourceAdapter()).destroy();
        if (mToolbarContainerDragListener != null) {
            mToolbarContainer.setOnDragListener(null);
            mToolbarContainerDragListener = null;
        }

        if (mXrSpaceModeObservableSupplier != null) {
            mXrSpaceModeObservableSupplier.removeObserver(mOnXrSpaceModeChanged);
        }
        if (mToolbarDataProvider != null) {
            mToolbarDataProvider.removeToolbarDataProviderObserver(this);
        }
    }

    @Override
    public void setVisibility(int visibility) {
        mMidVisibilityToggle = true;
        super.setVisibility(visibility);
        mMidVisibilityToggle = false;
    }

    @Override
    public void onAppHeaderStateChanged(AppHeaderState newState) {
        maybeUpdateTempTabStripDrawableBackground(mIncognito, newState);
        mAppHeaderState = newState;
    }

    // implements TabStripTransitionDelegate
    @Override
    public void onHeightChanged(int tabStripHeight, boolean applyScrimOverlay) {
        mutateToolbarLayoutParams().topMargin = tabStripHeight;

        int toolbarAndTabStripHeight = tabStripHeight + getToolbarHeight();
        mutateHairlineLayoutParams().topMargin = toolbarAndTabStripHeight;

        // Update the find toolbar view or view stub. We only do this for tablets
        // (find_toolbar_tablet_stub) since find_toolbar_stub is used for phone only.
        // TODO (crbug.com/1517059): Let FindToolbar itself decide how to set the top margin.
        {
            View findToolbar = findViewById(R.id.find_toolbar);
            if (findToolbar == null) {
                findToolbar = findViewById(R.id.find_toolbar_tablet_stub);
            }
            if (findToolbar == null) return;

            // Tablet FIP is anchored at the bottom of the toolbar.
            ViewGroup.MarginLayoutParams layoutParams =
                    (ViewGroup.MarginLayoutParams) findToolbar.getLayoutParams();
            layoutParams.topMargin = toolbarAndTabStripHeight;
            findToolbar.setLayoutParams(layoutParams);
        }
    }

    @Override
    public void onHeightTransitionFinished(boolean success) {
        if (!success) return;

        // Remeasure the control container in the next layout pass if needed. The post is needed due
        // to the existence of ToolbarProgressBar adjusting its position based on ToolbarLayout's
        // layout pass. The control container needs to remeasure based on the new margin in order to
        // retain the up-to-date size with size reduction for the tab strip.

        // This is not done during the transition as it could cause visual glitches.
        new Handler()
                .post(
                        () -> {
                            setMinimumHeight(
                                    mToolbar.getTabStripHeight()
                                            + getToolbarHeight()
                                            + getToolbarHairlineHeight());
                            ViewUtils.requestLayout(
                                    this, "ToolbarControlContainer.onHeightTransitionFinished");
                        });
    }

    private void maybeUpdateTempTabStripDrawableBackground(
            boolean incognito, @Nullable AppHeaderState appHeaderState) {
        // If compositor is initialized, we don't want to set the background drawable again since
        // it'll block the real tab strip in the compositor.
        if (mIsCompositorInitialized) return;

        boolean isInDesktopWindow = appHeaderState != null && appHeaderState.isInDesktopWindow();
        Drawable backgroundColor =
                new ColorDrawable(
                        TabUiThemeUtil.getTabStripBackgroundColor(
                                getContext(),
                                mIncognito,
                                isInDesktopWindow,
                                !mIsAppInUnfocusedDesktopWindow));
        Drawable backgroundTabImage =
                ResourcesCompat.getDrawable(
                        getContext().getResources(),
                        TabUiThemeUtil.getTabResource(),
                        getContext().getTheme());
        assumeNonNull(backgroundTabImage);
        backgroundTabImage.setTint(
                TabUiThemeUtil.getTabStripSelectedTabColor(getContext(), incognito));
        LayerDrawable backgroundDrawable =
                new LayerDrawable(new Drawable[] {backgroundColor, backgroundTabImage});

        final int backgroundTabImageIndex = 1;
        // Set image size to match tab size.
        backgroundDrawable.setPadding(0, 0, 0, 0);
        backgroundDrawable.setLayerSize(
                backgroundTabImageIndex,
                ViewUtils.dpToPx(getContext(), TabUiThemeUtil.getMaxTabStripTabWidthDp()),
                // TODO(crbug.com/335660381): We should use the tab strip height from resource
                // and add a top insets.
                mToolbar.getTabStripHeight());
        // Tab should show up at start of layer based on layout.
        backgroundDrawable.setLayerGravity(backgroundTabImageIndex, Gravity.START);

        // When app header state available, set the state accordingly.
        if (appHeaderState != null && appHeaderState.isInDesktopWindow()) {
            int topInset =
                    Math.max(0, appHeaderState.getAppHeaderHeight() - mToolbar.getTabStripHeight());
            backgroundDrawable.setLayerInset(
                    backgroundTabImageIndex,
                    appHeaderState.getLeftPadding(),
                    topInset,
                    appHeaderState.getRightPadding(),
                    0);
        }

        setBackground(backgroundDrawable);
    }

    /**
     * @param toolbar The toolbar contained inside this control container. Should be called after
     *     inflation is complete.
     * @param isIncognito Whether the toolbar should be initialized with incognito colors.
     * @param constraintsSupplier Used to access current constraints of the browser controls.
     * @param tabSupplier Used to access the current tab state.
     * @param compositorInMotionSupplier Whether there is an ongoing touch or gesture.
     * @param browserStateBrowserControlsVisibilityDelegate Used to keep controls locked when
     *     captures are stale and not able to be taken.
     * @param layoutStateProviderSupplier Used to check the current layout type.
     * @param fullscreenManager Used to check whether in fullscreen.
     */
    @Initializer
    public void setPostInitializationDependencies(
            Toolbar toolbar,
            ViewGroup toolbarView,
            boolean isIncognito,
            ObservableSupplier<@Nullable Integer> constraintsSupplier,
            Supplier<@Nullable Tab> tabSupplier,
            ObservableSupplier<Boolean> compositorInMotionSupplier,
            BrowserStateBrowserControlsVisibilityDelegate
                    browserStateBrowserControlsVisibilityDelegate,
            OneshotSupplier<LayoutStateProvider> layoutStateProviderSupplier,
            FullscreenManager fullscreenManager,
            ToolbarDataProvider toolbarDataProvider) {
        mToolbar = toolbar;
        mIncognito = isIncognito;
        mToolbarDataProvider = toolbarDataProvider;
        mToolbarDataProvider.addToolbarDataProviderObserver(this);

        BooleanSupplier isVisible = () -> this.getVisibility() == View.VISIBLE;
        mToolbarContainer.setPostInitializationDependencies(
                mToolbar,
                constraintsSupplier,
                tabSupplier,
                compositorInMotionSupplier,
                browserStateBrowserControlsVisibilityDelegate,
                isVisible,
                layoutStateProviderSupplier,
                fullscreenManager,
                () -> mMidVisibilityToggle,
                toolbarDataProvider);

        mToolbarView = toolbarView;
        assert mToolbarView != null;

        if (mToolbarView instanceof ToolbarTablet) {
            // On tablet, draw a fake tab strip and toolbar until the compositor is
            // ready to draw the real tab strip. (On phone, the toolbar is made entirely
            // of Android views, which are already initialized.)
            maybeUpdateTempTabStripDrawableBackground(isIncognito, mAppHeaderState);

            // Manually setting the top margin of the toolbar hairline. On high density tablets,
            // the rounding for dp -> px conversion can cause off-by-one error for the toolbar
            // hairline top margin, result in a sequence of top UI misalignment.
            // See https://crbug.com/40941027.
            var lp = (MarginLayoutParams) mToolbarHairline.getLayoutParams();
            lp.topMargin = mToolbar.getTabStripHeight() + mToolbarLayoutHeight;
            mToolbarHairline.setLayoutParams(lp);
        }

        assert mToolbarContainer.getResourceAdapter() != null;
        mToolbarContainer
                .getResourceAdapter()
                .addOnResourceReadyCallback(mOnResourceCaptureCallback);
    }

    @Override
    protected void onLayout(boolean changed, int left, int top, int right, int bottom) {
        super.onLayout(changed, left, top, right, bottom);
        if (changed) {
            // The size of the container has changed, so the texture capture of
            // the toolbar must be redone.
            // TODO(crbug.com/435771941): using surface sync to improve efficiency
            mToolbarContainer.invalidate();
            if (LibraryLoader.getInstance().isInitialized()) {
                mToolbarContainer.getResourceAdapter().onResourceRequested();
            }
        }
    }

    @Override
    // TODO(crbug.com/40779510): work out why this is causing a lint error
    @SuppressWarnings("Override")
    public boolean gatherTransparentRegion(Region region) {
        // Reset the translation on the control container before attempting to compute the
        // transparent region.
        float translateY = getTranslationY();
        setTranslationY(0);

        ViewUtils.gatherTransparentRegionsForOpaqueView(this, region);

        setTranslationY(translateY);

        return true;
    }

    /**
     * Update whether the control container is ready to have the bitmap representation of itself be
     * captured.
     */
    public void setReadyForBitmapCapture(boolean ready) {
        mToolbarContainer.mReadyForBitmapCapture = ready;
    }

    /**
     * Sets whether the current activity is starting in an unfocused desktop window. This state is
     * set exactly once at startup and is not updated thereafter.
     *
     * @param isAppInUnfocusedDesktopWindow Whether the current activity is in an unfocused desktop
     *     window.
     */
    public void setAppInUnfocusedDesktopWindow(boolean isAppInUnfocusedDesktopWindow) {
        // TODO (crbug/337132433): Observe window focus state changes to update this state.
        mIsAppInUnfocusedDesktopWindow = isAppInUnfocusedDesktopWindow;
    }

    /**
     * Sets drag listener for toolbar container.
     *
     * @param toolbarContainerDragListener Listener to set.
     */
    public void setToolbarContainerDragListener(
            @Nullable OnDragListener toolbarContainerDragListener) {
        mToolbarContainerDragListener = toolbarContainerDragListener;
        mToolbarContainer.setOnDragListener(mToolbarContainerDragListener);
    }

    @Override
    public void onPrimaryColorChanged() {
        if (mShowLocationBarOnly) {
            setBackgroundColor(mToolbarDataProvider.getPrimaryColor());
        }
    }

    /** The layout that handles generating the toolbar view resource. */
    // Only publicly visible due to lint warnings.
    public static class ToolbarViewResourceCoordinatorLayout extends ViewResourceCoordinatorLayout {
        private BooleanSupplier mIsMidVisibilityToggle;
        private boolean mReadyForBitmapCapture;

        public ToolbarViewResourceCoordinatorLayout(Context context, AttributeSet attrs) {
            super(context, attrs);
        }

        @Override
        protected ViewResourceAdapter createResourceAdapter() {
            return new ToolbarViewResourceAdapter(this);
        }

        /**
         * @see ToolbarViewResourceAdapter#setPostInitializationDependencies.
         */
        @Initializer
        public void setPostInitializationDependencies(
                Toolbar toolbar,
                ObservableSupplier<@Nullable Integer> constraintsSupplier,
                Supplier<@Nullable Tab> tabSupplier,
                ObservableSupplier<Boolean> compositorInMotionSupplier,
                BrowserStateBrowserControlsVisibilityDelegate
                        browserStateBrowserControlsVisibilityDelegate,
                BooleanSupplier isVisible,
                OneshotSupplier<LayoutStateProvider> layoutStateProviderSupplier,
                FullscreenManager fullscreenManager,
                BooleanSupplier isMidVisibilityToggle,
                ToolbarDataProvider toolbarDataProvider) {
            mIsMidVisibilityToggle = isMidVisibilityToggle;
            ToolbarViewResourceAdapter adapter =
                    ((ToolbarViewResourceAdapter) getResourceAdapter());
            adapter.setPostInitializationDependencies(
                    toolbar,
                    constraintsSupplier,
                    tabSupplier,
                    compositorInMotionSupplier,
                    browserStateBrowserControlsVisibilityDelegate,
                    isVisible,
                    layoutStateProviderSupplier,
                    fullscreenManager,
                    toolbarDataProvider);
        }

        @Override
        protected boolean isReadyForCapture() {
            // This method is checked when invalidateChildInParent happens. Returning false will
            // prevent the dirty bit from being set in ViewResourceAdapter. This is what we want
            // when the visibility of this view is being toggled. Many of our children report
            // material changes that propagate back up. But we don't care about any of this for
            // capturing as the captures occur below this frame layout.
            return mReadyForBitmapCapture
                    && getVisibility() == VISIBLE
                    && !mIsMidVisibilityToggle.getAsBoolean();
        }
    }

    @VisibleForTesting
    protected static class ToolbarViewResourceAdapter extends ViewResourceAdapter {
        /**
         * Emitted at various points during the in motion observer method. Note that it is not the
         * toolbar that is in motion, but the toolbar's handling of the compositor being in motion.
         * Treat this list as append only and keep it in sync with ToolbarInMotionStage in
         * enums.xml.
         */
        @IntDef({
            ToolbarInMotionStage.SUPPRESSION_ENABLED,
            ToolbarInMotionStage.READINESS_CHECKED,
            ToolbarInMotionStage.NUM_ENTRIES
        })
        @Retention(RetentionPolicy.SOURCE)
        @interface ToolbarInMotionStage {
            int SUPPRESSION_ENABLED = 0;
            int READINESS_CHECKED = 1;
            int NUM_ENTRIES = 2;
        }

        private final int[] mTempPosition = new int[2];
        private final Rect mLocationBarRect = new Rect();
        private final Rect mToolbarRect = new Rect();
        private final View mToolbarContainer;
        private final ToolbarHairlineView mToolbarHairline;
        private final Callback<Boolean> mOnCompositorInMotionChange =
                this::onCompositorInMotionChange;

        private Toolbar mToolbar;
        private ConstraintsChecker mConstraintsObserver;
        private Supplier<@Nullable Tab> mTabSupplier;
        private ObservableSupplier<Boolean> mCompositorInMotionSupplier;

        private BrowserStateBrowserControlsVisibilityDelegate
                mBrowserStateBrowserControlsVisibilityDelegate;

        private BooleanSupplier mControlContainerIsVisibleSupplier;
        private @Nullable LayoutStateProvider mLayoutStateProvider;
        private FullscreenManager mFullscreenManager;

        private int mControlsToken = TokenHolder.INVALID_TOKEN;

        private boolean mNeedCaptureAfterPageLoad;

        private ToolbarDataProvider mToolbarDataProvider;

        private CharSequence mMostRecentlyCapturedUrl;

        /** Builds the resource adapter for the toolbar. */
        public ToolbarViewResourceAdapter(View toolbarContainer) {
            super(toolbarContainer);
            mToolbarContainer = toolbarContainer;
            mToolbarHairline = mToolbarContainer.findViewById(R.id.toolbar_hairline);
        }

        /**
         * Set the toolbar after it has been dynamically inflated.
         *
         * @param toolbar The browser's toolbar.
         * @param constraintsSupplier Used to access current constraints of the browser controls.
         * @param tabSupplier Used to access the current tab state.
         * @param compositorInMotionSupplier Whether there is an ongoing touch or gesture.
         * @param browserStateBrowserControlsVisibilityDelegate Used to keep controls locked when
         *     captures are stale and not able to be taken.
         * @param controlContainerIsVisibleSupplier Whether the toolbar is visible.
         * @param layoutStateProviderSupplier Used to check the current layout type.
         * @param fullscreenManager Used to check whether in fullscreen.
         */
        @Initializer
        public void setPostInitializationDependencies(
                Toolbar toolbar,
                ObservableSupplier<@Nullable Integer> constraintsSupplier,
                Supplier<@Nullable Tab> tabSupplier,
                ObservableSupplier<Boolean> compositorInMotionSupplier,
                BrowserStateBrowserControlsVisibilityDelegate
                        browserStateBrowserControlsVisibilityDelegate,
                BooleanSupplier controlContainerIsVisibleSupplier,
                OneshotSupplier<LayoutStateProvider> layoutStateProviderSupplier,
                FullscreenManager fullscreenManager,
                ToolbarDataProvider toolbarDataProvider) {
            assert mToolbar == null;
            mToolbar = toolbar;

            // These dependencies only matter when ChromeFeatureList.SUPPRESS_TOOLBAR_CAPTURES is
            // enabled. Unfortunately this method is often called before native is initialized,
            // and so we do not know if we'll need them yet. Store all of them, and then
            // conditionally use them when captures are requested.
            mConstraintsObserver =
                    new ConstraintsChecker(this, constraintsSupplier, Looper.getMainLooper());
            mTabSupplier = tabSupplier;
            mCompositorInMotionSupplier = compositorInMotionSupplier;
            mCompositorInMotionSupplier.addObserver(mOnCompositorInMotionChange);
            mBrowserStateBrowserControlsVisibilityDelegate =
                    browserStateBrowserControlsVisibilityDelegate;
            mControlContainerIsVisibleSupplier = controlContainerIsVisibleSupplier;
            layoutStateProviderSupplier.onAvailable(
                    (layoutStateProvider) -> mLayoutStateProvider = layoutStateProvider);
            mFullscreenManager = fullscreenManager;
            mToolbarDataProvider = toolbarDataProvider;
            mMostRecentlyCapturedUrl = "";
        }

        @Override
        public boolean isDirty() {
            if (!super.isDirty()) {
                CaptureReadinessResult.logCaptureReasonFromResult(
                        CaptureReadinessResult.notReady(
                                TopToolbarBlockCaptureReason.VIEW_NOT_DIRTY));
                return false;
            }

            if (mFullscreenManager != null && mFullscreenManager.getPersistentFullscreenMode()) {
                // The toolbar is never shown during fullscreen, so no point in capturing. The
                // dimensions are likely wrong and will only be restored after fullscreen is
                // exited.
                CaptureReadinessResult.logCaptureReasonFromResult(
                        CaptureReadinessResult.notReady(TopToolbarBlockCaptureReason.FULLSCREEN));
                return false;
            }

            final @LayoutType int layoutType = getCurrentLayoutType();
            if (layoutType != LayoutType.TOOLBAR_SWIPE) {
                // With BCIV enabled, we need a capture after page load before the controls are
                // unlocked. So, only go into this section that potentially blocks the capture
                // if we didn't just load a page.
                if (!mNeedCaptureAfterPageLoad
                        && mConstraintsObserver != null
                        && mTabSupplier != null) {
                    Tab tab = mTabSupplier.get();

                    // TODO(crbug.com/40859837): Understand and fix this for native
                    // pages. It seems capturing is required for some part of theme observers to
                    // work correctly, but it shouldn't be.
                    boolean isNativePage = tab == null || tab.isNativePage();
                    if (!isNativePage && mConstraintsObserver.areControlsLocked()) {
                        mConstraintsObserver.scheduleRequestResourceOnUnlock();
                        CaptureReadinessResult.logCaptureReasonFromResult(
                                CaptureReadinessResult.notReady(
                                        TopToolbarBlockCaptureReason.BROWSER_CONTROLS_LOCKED));
                        return false;
                    }
                }

                // The heavy lifting is done by #onCompositorInMotionChange and the above
                // browser controls state check. This logic only needs to guard against a
                // capture when the controls were partially or fully scrolled off, in the middle
                // of motion, before the view became dirty.
                if (mCompositorInMotionSupplier != null) {
                    Boolean compositorInMotion = mCompositorInMotionSupplier.get();
                    if (Boolean.TRUE.equals(compositorInMotion)) {
                        CaptureReadinessResult.logCaptureReasonFromResult(
                                CaptureReadinessResult.notReady(
                                        TopToolbarBlockCaptureReason.COMPOSITOR_IN_MOTION));
                        return false;
                    }
                }
            }

            return checkCaptureReadinessResult();
        }

        /**
         * @return Whether a dirty check for invalidation makes sense at this time.
         *     <p>False if either the toolbar is not dirty, or the toolbar is dirty but a capture
         *     isn't required at this moment (see {@link TopToolbarBlockCaptureReason})
         *     <p>True if the toolbar is dirty and a new capture is needed.
         */
        private boolean checkCaptureReadinessResult() {
            CaptureReadinessResult isReadyResult =
                    mToolbar == null ? null : mToolbar.isReadyForTextureCapture();
            if (isReadyResult != null
                    && isReadyResult.blockReason == TopToolbarBlockCaptureReason.SNAPSHOT_SAME) {
                // If our view was invalidated but no meaningful properties have changed (which is
                // what SNAPSHOT_SAME implies), we can safely avoid re-checking until the next view
                // invalidation.
                setDirtyRectEmpty();
            }

            CaptureReadinessResult.logCaptureReasonFromResult(isReadyResult);
            return isReadyResult == null ? false : isReadyResult.isReady;
        }

        @Override
        @SuppressWarnings("NullAway")
        public void onCaptureStart(Canvas canvas, @Nullable Rect dirtyRect) {
            RecordHistogram.recordEnumeratedHistogram(
                    "Android.Toolbar.BitmapCapture",
                    ToolbarCaptureType.TOP,
                    ToolbarCaptureType.NUM_ENTRIES);

            // Erase the canvas because assets drawn are not fully opaque and therefore painting
            // twice would be bad.
            canvas.save();
            canvas.clipRect(0, 0, mToolbarContainer.getWidth(), mToolbarContainer.getHeight());
            canvas.drawColor(0, PorterDuff.Mode.CLEAR);
            canvas.restore();
            dirtyRect.set(0, 0, mToolbarContainer.getWidth(), mToolbarContainer.getHeight());

            mToolbar.setTextureCaptureMode(true);

            super.onCaptureStart(canvas, dirtyRect);
        }

        @Override
        public void onCaptureEnd() {
            mToolbar.setTextureCaptureMode(false);
            mMostRecentlyCapturedUrl = mToolbarDataProvider.getUrlBarData().displayText;
        }

        @Override
        public long createNativeResource() {
            mToolbar.getPositionRelativeToContainer(mToolbarContainer, mTempPosition);
            mToolbarRect.set(
                    mTempPosition[0],
                    mTempPosition[1],
                    mToolbarContainer.getWidth(),
                    mTempPosition[1] + mToolbar.getHeight());

            mToolbar.getLocationBarContentRect(mLocationBarRect);
            mLocationBarRect.offset(mTempPosition[0], mTempPosition[1]);

            int shadowHeight = mToolbarHairline.getHeight();
            return ResourceFactory.createToolbarContainerResource(
                    mToolbarRect, mLocationBarRect, shadowHeight);
        }

        public void onPageLoadStopped() {
            if (ChromeFeatureList.sBrowserControlsInViz.isEnabled()) {
                // With capture suppression, we don't capture after navigating. Instead, we schedule
                // a capture to happen when the controls become unlocked. With BCIV, there is no
                // surface sync, so it's more likely to scroll before the capture is complete. To
                // fix this, we capture after page load finishes. This is late enough in navigation
                // to not delay other important tasks on the main thread, and early enough so we
                // have a capture available before the controls are unlocked.
                mNeedCaptureAfterPageLoad = true;
                onResourceRequested();
                mNeedCaptureAfterPageLoad = false;
            }
        }

        private boolean shouldSampleStaleCaptureHistogram() {
            return sForceStaleCaptureHistogram || Math.random() < SAMPLE_STALE_CAPTURE_PROBABILITY;
        }

        public void onContentViewScrollingStateChanged(boolean scrolling) {
            if (scrolling
                    && mControlsToken == TokenHolder.INVALID_TOKEN
                    && !mConstraintsObserver.areControlsLocked()
                    && shouldSampleStaleCaptureHistogram()) {
                boolean isCaptureStale =
                        !mToolbarDataProvider
                                .getUrlBarData()
                                .displayText
                                .equals(mMostRecentlyCapturedUrl);
                RecordHistogram.recordBooleanHistogram(
                        "Android.Toolbar.StaleCapturedUrlOnScroll.Subsampled", isCaptureStale);
            }
        }

        public void destroy() {
            if (mConstraintsObserver != null) {
                mConstraintsObserver.destroy();
            }
            if (mCompositorInMotionSupplier != null) {
                mCompositorInMotionSupplier.removeObserver(mOnCompositorInMotionChange);
            }
        }

        private void onCompositorInMotionChange(Boolean compositorInMotion) {
            if (mToolbar == null
                    || mBrowserStateBrowserControlsVisibilityDelegate == null
                    || mControlContainerIsVisibleSupplier == null) {
                return;
            }

            if (ToolbarFeatures.shouldRecordSuppressionMetrics()) {
                RecordHistogram.recordEnumeratedHistogram(
                        "Android.TopToolbar.InMotionStage",
                        ToolbarInMotionStage.SUPPRESSION_ENABLED,
                        ToolbarInMotionStage.NUM_ENTRIES);
            }

            if (Boolean.FALSE.equals(compositorInMotion)) {
                if (mControlsToken == TokenHolder.INVALID_TOKEN) {
                    // Only needed when the ConstraintsChecker doesn't drive the capture.
                    // TODO(crbug.com/40244055): Make this post a task similar to
                    // ConstraintsChecker.
                    onResourceRequested();
                } else {
                    mBrowserStateBrowserControlsVisibilityDelegate.releasePersistentShowingToken(
                            mControlsToken);
                    mControlsToken = TokenHolder.INVALID_TOKEN;
                }
            } else if (Boolean.TRUE.equals(compositorInMotion)
                    && super.isDirty()
                    && (ChromeFeatureList.sToolbarStaleCaptureBugFix.isEnabled()
                            || mControlContainerIsVisibleSupplier.getAsBoolean())) {
                CaptureReadinessResult captureReadinessResult = mToolbar.isReadyForTextureCapture();
                CaptureReadinessResult.logCaptureReasonFromResult(captureReadinessResult);
                if (ToolbarFeatures.shouldRecordSuppressionMetrics()) {
                    RecordHistogram.recordEnumeratedHistogram(
                            "Android.TopToolbar.InMotionStage",
                            ToolbarInMotionStage.READINESS_CHECKED,
                            ToolbarInMotionStage.NUM_ENTRIES);
                }
                if (captureReadinessResult.blockReason
                        == TopToolbarBlockCaptureReason.SNAPSHOT_SAME) {
                    setDirtyRectEmpty();
                } else {
                    // Motion is starting, and we don't have a good capture. Lock the controls so
                    // that we keep using the Java view. After the touch event is over we'll unlock
                    // and try to capture.
                    mControlsToken =
                            mBrowserStateBrowserControlsVisibilityDelegate
                                    .showControlsPersistentAndClearOldToken(mControlsToken);
                    // Utilize posted task in ConstraintsChecker to drive new capture.
                    mConstraintsObserver.scheduleRequestResourceOnUnlock();
                    CaptureReadinessResult.logCaptureReasonFromResult(
                            CaptureReadinessResult.notReady(
                                    TopToolbarBlockCaptureReason.COMPOSITOR_IN_MOTION));
                }
            }
        }

        private @LayoutType int getCurrentLayoutType() {
            return mLayoutStateProvider == null
                    ? LayoutType.NONE
                    : mLayoutStateProvider.getActiveLayoutType();
        }
    }

    @Override
    public boolean onTouchEvent(MotionEvent event) {
        // Don't eat the event if we don't have a handler.
        if (mSwipeGestureListener == null) return false;

        // Don't react on touch events if the toolbar container is not fully visible.
        if (!isToolbarContainerFullyVisible()) return true;

        // If we have ACTION_DOWN in this context, that means either no child consumed the event or
        // this class is the top UI at the event position. Then, we don't need to feed the event to
        // mGestureDetector here because the event is already once fed in onInterceptTouchEvent().
        // Moreover, we have to return true so that this class can continue to intercept all the
        // subsequent events.
        if (event.getActionMasked() == MotionEvent.ACTION_DOWN && !isOnTabStrip(event)) {
            return true;
        }

        return mSwipeGestureListener.onTouchEvent(event);
    }

    @Override
    public boolean onInterceptTouchEvent(MotionEvent event) {
        if (!isToolbarContainerFullyVisible()) return true;
        if (isOnTabStrip(event)) return false;
        if (mSwipeGestureListener != null && mSwipeGestureListener.onTouchEvent(event)) return true;

        for (TouchEventObserver o : mTouchEventObservers) {
            if (o.onInterceptTouchEvent(event)) return true;
        }

        return false;
    }

    private boolean isOnTabStrip(MotionEvent e) {
        // If the tab strip is showing, allow the tab strip to consume its gestures.
        // Otherwise, permit bottom toolbar to handle swipe up gesture to open tab switcher.
        int tabStripHeight = mToolbar.getTabStripHeight();
        return tabStripHeight != 0 && e.getY() <= tabStripHeight;
    }

    /**
     * @return Whether or not the toolbar container is fully visible on screen.
     */
    private boolean isToolbarContainerFullyVisible() {
        return mToolbarContainer.getVisibility() == VISIBLE;
    }

    private void onToolbarCaptureUpdated(Resource toolbarCaptureResource) {
        Rect newSize = toolbarCaptureResource.getBitmapSize();
        if (mToolbarCaptureSize.equals(newSize)) return;

        mToolbarCaptureSize.set(toolbarCaptureResource.getBitmapSize());
        if (mToolbar != null) {
            mToolbar.onCaptureSizeUpdated();
        }
    }

    int getToolbarCaptureHeight() {
        return mToolbarCaptureSize.height();
    }

    private class SwipeGestureListenerImpl extends SwipeGestureListener {
        public SwipeGestureListenerImpl(Context context, SwipeHandler handler) {
            super(context, handler);
        }

        @Override
        public boolean shouldRecognizeSwipe(MotionEvent e1, MotionEvent e2) {
            if (isOnTabStrip(e1)) return false;
            if (mToolbar != null && mToolbar.shouldIgnoreSwipeGesture()) return false;
            if (KeyboardVisibilityDelegate.getInstance()
                    .isKeyboardShowing(ToolbarControlContainer.this)) {
                return false;
            }
            return true;
        }
    }

    void setToolbarForTesting(Toolbar testToolbar) {
        mToolbar = testToolbar;
    }

    void setToolbarHairlineForTesting(View hairline) {
        mToolbarHairline = hairline;
    }

    ToolbarViewResourceCoordinatorLayout getToolbarContainerForTesting() {
        return mToolbarContainer;
    }

    public void setXrSpaceModeObservableSupplierMaybe(
            @Nullable ObservableSupplier<Boolean> xrSpaceModeObservableSupplier) {
        if (mXrSpaceModeObservableSupplier == null && xrSpaceModeObservableSupplier != null) {
            mXrSpaceModeObservableSupplier = xrSpaceModeObservableSupplier;
            mXrSpaceModeObservableSupplier.addSyncObserver(mOnXrSpaceModeChanged);
        }
    }

    public void onXrSpaceModeChanged(Boolean fullSpaceMode) {
        setVisibility(Boolean.TRUE.equals(fullSpaceMode) ? View.INVISIBLE : View.VISIBLE);
    }

    static Runnable forceStaleCaptureHistogramForTesting() {
        sForceStaleCaptureHistogram = true;
        return () -> sForceStaleCaptureHistogram = false;
    }
}
