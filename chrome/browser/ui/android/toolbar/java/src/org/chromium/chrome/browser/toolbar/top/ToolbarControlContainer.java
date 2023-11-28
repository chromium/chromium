// Copyright 2015 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.toolbar.top;

import android.content.Context;
import android.graphics.Canvas;
import android.graphics.PorterDuff;
import android.graphics.Rect;
import android.graphics.Region;
import android.graphics.drawable.ColorDrawable;
import android.graphics.drawable.Drawable;
import android.graphics.drawable.LayerDrawable;
import android.os.Build;
import android.os.Looper;
import android.util.AttributeSet;
import android.view.Gravity;
import android.view.MotionEvent;
import android.view.View;
import android.view.ViewStub;

import androidx.annotation.IntDef;
import androidx.annotation.Nullable;
import androidx.annotation.VisibleForTesting;
import androidx.core.content.res.ResourcesCompat;

import org.chromium.base.Callback;
import org.chromium.base.TraceEvent;
import org.chromium.base.metrics.RecordHistogram;
import org.chromium.base.supplier.ObservableSupplier;
import org.chromium.base.supplier.OneshotSupplier;
import org.chromium.base.supplier.Supplier;
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
import org.chromium.chrome.browser.toolbar.ToolbarFeatures;
import org.chromium.chrome.browser.toolbar.ToolbarProgressBar;
import org.chromium.chrome.browser.toolbar.top.CaptureReadinessResult.TopToolbarBlockCaptureReason;
import org.chromium.components.browser_ui.widget.ClipDrawableProgressBar.DrawingInfo;
import org.chromium.components.browser_ui.widget.ViewResourceFrameLayout;
import org.chromium.components.browser_ui.widget.gesture.SwipeGestureListener;
import org.chromium.components.browser_ui.widget.gesture.SwipeGestureListener.SwipeHandler;
import org.chromium.ui.KeyboardVisibilityDelegate;
import org.chromium.ui.base.DeviceFormFactor;
import org.chromium.ui.base.ViewUtils;
import org.chromium.ui.resources.dynamics.ViewResourceAdapter;
import org.chromium.ui.util.TokenHolder;
import org.chromium.ui.widget.OptimizedFrameLayout;

import java.lang.annotation.Retention;
import java.lang.annotation.RetentionPolicy;
import java.util.function.BooleanSupplier;

/** Layout for the browser controls (omnibox, menu, tab strip, etc..). */
public class ToolbarControlContainer extends OptimizedFrameLayout implements ControlContainer {
    private boolean mIncognito;

    private Toolbar mToolbar;
    private ToolbarViewResourceFrameLayout mToolbarContainer;

    private SwipeGestureListener mSwipeGestureListener;
    private OnDragListener mToolbarContainerDragListener;

    /**
     * Constructs a new control container.
     * <p>
     * This constructor is used when inflating from XML.
     *
     * @param context The context used to build this view.
     * @param attrs The attributes used to determine how to construct this view.
     */
    public ToolbarControlContainer(Context context, AttributeSet attrs) {
        super(context, attrs);
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
    public void setSwipeHandler(SwipeHandler handler) {
        mSwipeGestureListener = new SwipeGestureListenerImpl(getContext(), handler);
    }

    @Override
    public void initWithToolbar(int toolbarLayoutId) {
        try (TraceEvent te = TraceEvent.scoped("ToolbarControlContainer.initWithToolbar")) {
            mToolbarContainer =
                    (ToolbarViewResourceFrameLayout) findViewById(R.id.toolbar_container);
            ViewStub toolbarStub = findViewById(R.id.toolbar_stub);
            toolbarStub.setLayoutResource(toolbarLayoutId);
            toolbarStub.inflate();
        }
    }

    @Override
    public void onTabOrModelChanged(boolean incognito) {
        if (!DeviceFormFactor.isNonMultiDisplayContextOnTablet(getContext())
                || getBackground() == null) {
            return;
        }

        if (mIncognito != incognito) {
            setBackground(getTempTabStripDrawable(incognito));
            mIncognito = incognito;
        }
    }

    @Override
    public void destroy() {
        ((ToolbarViewResourceAdapter) getToolbarResourceAdapter()).destroy();
        if (mToolbarContainerDragListener != null) {
            mToolbarContainer.setOnDragListener(null);
            mToolbarContainerDragListener = null;
        }
    }

    private Drawable getTempTabStripDrawable(boolean incognito) {
        Drawable bgdColor =
                new ColorDrawable(
                        TabUiThemeUtil.getTabStripBackgroundColor(getContext(), incognito));
        Drawable bdgTabImage =
                ResourcesCompat.getDrawable(
                        getContext().getResources(),
                        TabUiThemeUtil.getTabResource(),
                        getContext().getTheme());
        bdgTabImage.setTint(
                TabUiThemeUtil.getTabStripContainerColor(
                        getContext(), incognito, true, false, false, false));
        LayerDrawable backgroundDrawable =
                new LayerDrawable(new Drawable[] {bgdColor, bdgTabImage});
        // Set image size to match tab size.
        backgroundDrawable.setPadding(0, 0, 0, 0);
        backgroundDrawable.setLayerSize(
                1,
                ViewUtils.dpToPx(getContext(), TabUiThemeUtil.getMaxTabStripTabWidthDp()),
                mToolbar.getTabStripHeight());
        // Tab should show up at start of layer based on layout.
        backgroundDrawable.setLayerGravity(1, Gravity.START);

        return backgroundDrawable;
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
    public void setPostInitializationDependencies(
            Toolbar toolbar,
            boolean isIncognito,
            ObservableSupplier<Integer> constraintsSupplier,
            Supplier<Tab> tabSupplier,
            ObservableSupplier<Boolean> compositorInMotionSupplier,
            BrowserStateBrowserControlsVisibilityDelegate
                    browserStateBrowserControlsVisibilityDelegate,
            OneshotSupplier<LayoutStateProvider> layoutStateProviderSupplier,
            FullscreenManager fullscreenManager) {
        mToolbar = toolbar;
        mIncognito = isIncognito;

        BooleanSupplier isVisible = () -> this.getVisibility() == View.VISIBLE;
        mToolbarContainer.setPostInitializationDependencies(
                mToolbar,
                constraintsSupplier,
                tabSupplier,
                compositorInMotionSupplier,
                browserStateBrowserControlsVisibilityDelegate,
                isVisible,
                layoutStateProviderSupplier,
                fullscreenManager);

        View toolbarView = findViewById(R.id.toolbar);
        assert toolbarView != null;

        if (toolbarView instanceof ToolbarTablet) {
            // On tablet, draw a fake tab strip and toolbar until the compositor is
            // ready to draw the real tab strip. (On phone, the toolbar is made entirely
            // of Android views, which are already initialized.)
            setBackground(getTempTabStripDrawable(isIncognito));
        }
    }

    @Override
    // TODO(crbug.com/1231201): work out why this is causing a lint error
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

    /** Invalidate the entire capturing bitmap region. */
    public void invalidateBitmap() {
        ((ToolbarViewResourceAdapter) getToolbarResourceAdapter()).forceInvalidate();
    }

    /**
     * Update whether the control container is ready to have the bitmap representation of
     * itself be captured.
     */
    public void setReadyForBitmapCapture(boolean ready) {
        mToolbarContainer.mReadyForBitmapCapture = ready;
    }

    /**
     * Sets drag listener for toolbar container.
     *
     * @param toolbarContainerDragListener Listener to set.
     */
    public void setToolbarContainerDragListener(OnDragListener toolbarContainerDragListener) {
        mToolbarContainerDragListener = toolbarContainerDragListener;
        mToolbarContainer.setOnDragListener(mToolbarContainerDragListener);
    }

    /** The layout that handles generating the toolbar view resource. */
    // Only publicly visible due to lint warnings.
    public static class ToolbarViewResourceFrameLayout extends ViewResourceFrameLayout {
        private boolean mReadyForBitmapCapture;

        public ToolbarViewResourceFrameLayout(Context context, AttributeSet attrs) {
            super(context, attrs);
        }

        @Override
        protected ViewResourceAdapter createResourceAdapter() {
            boolean useHardwareBitmapDraw = false;
            if (Build.VERSION.SDK_INT >= Build.VERSION_CODES.Q) {
                useHardwareBitmapDraw = ChromeFeatureList.sToolbarUseHardwareBitmapDraw.isEnabled();
            }
            return new ToolbarViewResourceAdapter(this, useHardwareBitmapDraw);
        }

        /**
         * @see ToolbarViewResourceAdapter#setPostInitializationDependencies.
         */
        public void setPostInitializationDependencies(
                Toolbar toolbar,
                ObservableSupplier<Integer> constraintsSupplier,
                Supplier<Tab> tabSupplier,
                ObservableSupplier<Boolean> compositorInMotionSupplier,
                BrowserStateBrowserControlsVisibilityDelegate
                        browserStateBrowserControlsVisibilityDelegate,
                BooleanSupplier isVisible,
                OneshotSupplier<LayoutStateProvider> layoutStateProviderSupplier,
                FullscreenManager fullscreenManager) {
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
                    fullscreenManager);
        }

        @Override
        protected boolean isReadyForCapture() {
            return mReadyForBitmapCapture && getVisibility() == VISIBLE;
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
        private final Callback<Boolean> mOnCompositorInMotionChange =
                this::onCompositorInMotionChange;

        @Nullable private Toolbar mToolbar;
        @Nullable private ConstraintsChecker mConstraintsObserver;
        @Nullable private Supplier<Tab> mTabSupplier;
        @Nullable private ObservableSupplier<Boolean> mCompositorInMotionSupplier;

        @Nullable
        private BrowserStateBrowserControlsVisibilityDelegate
                mBrowserStateBrowserControlsVisibilityDelegate;

        @Nullable private BooleanSupplier mControlContainerIsVisibleSupplier;
        @Nullable private LayoutStateProvider mLayoutStateProvider;
        @Nullable private FullscreenManager mFullscreenManager;

        private int mControlsToken = TokenHolder.INVALID_TOKEN;

        /** Builds the resource adapter for the toolbar. */
        public ToolbarViewResourceAdapter(View toolbarContainer, boolean useHardwareBitmapDraw) {
            super(toolbarContainer, useHardwareBitmapDraw);
            mToolbarContainer = toolbarContainer;
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
        public void setPostInitializationDependencies(
                Toolbar toolbar,
                ObservableSupplier<Integer> constraintsSupplier,
                Supplier<Tab> tabSupplier,
                ObservableSupplier<Boolean> compositorInMotionSupplier,
                BrowserStateBrowserControlsVisibilityDelegate
                        browserStateBrowserControlsVisibilityDelegate,
                BooleanSupplier controlContainerIsVisibleSupplier,
                OneshotSupplier<LayoutStateProvider> layoutStateProviderSupplier,
                FullscreenManager fullscreenManager) {
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
        }

        /**
         * Force this resource to be recaptured in full, ignoring the checks
         * {@link #invalidate(Rect)} does.
         */
        public void forceInvalidate() {
            super.invalidate(null);
        }

        @Override
        public boolean isDirty() {
            if (!super.isDirty()) {
                CaptureReadinessResult.logCaptureReasonFromResult(
                        CaptureReadinessResult.notReady(
                                TopToolbarBlockCaptureReason.VIEW_NOT_DIRTY));
                return false;
            }

            if (ToolbarFeatures.shouldSuppressCaptures()) {
                if (ToolbarFeatures.shouldBlockCapturesForFullscreen()
                        && mFullscreenManager.getPersistentFullscreenMode()) {
                    // The toolbar is never shown during fullscreen, so no point in capturing. The
                    // dimensions are likely wrong and will only be restored after fullscreen is
                    // exited.
                    CaptureReadinessResult.logCaptureReasonFromResult(
                            CaptureReadinessResult.notReady(
                                    TopToolbarBlockCaptureReason.FULLSCREEN));
                    return false;
                }

                final @LayoutType int layoutType = getCurrentLayoutType();
                if (layoutType != LayoutType.TOOLBAR_SWIPE) {
                    if (mConstraintsObserver != null && mTabSupplier != null) {
                        Tab tab = mTabSupplier.get();

                        // TODO(https://crbug.com/1355516): Understand and fix this for native
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
            }

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
        public void onCaptureStart(Canvas canvas, Rect dirtyRect) {
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
            // Forcing a texture capture should only be done for one draw. Turn off forced
            // texture capture.
            mToolbar.setForceTextureCapture(false);
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

            int shadowHeight =
                    mToolbarContainer.getHeight()
                            - mToolbar.getHeight()
                            - mToolbar.getTabStripHeight();
            return ResourceFactory.createToolbarContainerResource(
                    mToolbarRect, mLocationBarRect, shadowHeight);
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
            if (!ToolbarFeatures.shouldSuppressCaptures()
                    || mToolbar == null
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

            if (!Boolean.TRUE.equals(compositorInMotion)) {
                if (mControlsToken == TokenHolder.INVALID_TOKEN) {
                    // Only needed when the ConstraintsChecker doesn't drive the capture.
                    // TODO(https://crbug.com/1378721): Make this post a task similar to
                    // ConstraintsChecker.
                    onResourceRequested();
                } else {
                    mBrowserStateBrowserControlsVisibilityDelegate.releasePersistentShowingToken(
                            mControlsToken);
                    mControlsToken = TokenHolder.INVALID_TOKEN;
                }
            } else if (super.isDirty() && mControlContainerIsVisibleSupplier.getAsBoolean()) {
                CaptureReadinessResult captureReadinessResult = mToolbar.isReadyForTextureCapture();
                if (ToolbarFeatures.shouldRecordSuppressionMetrics()
                        && compositorInMotion != null) {
                    RecordHistogram.recordEnumeratedHistogram(
                            "Android.TopToolbar.InMotionStage",
                            ToolbarInMotionStage.READINESS_CHECKED,
                            ToolbarInMotionStage.NUM_ENTRIES);
                }
                if (captureReadinessResult.blockReason
                        == TopToolbarBlockCaptureReason.SNAPSHOT_SAME) {
                    setDirtyRectEmpty();
                } else if (captureReadinessResult.isReady) {
                    // Motion is starting, and we don't have a good capture. Lock the controls so
                    // that a new capture doesn't happen and the old capture is not shown. This can
                    // be fixed once the motion is over.
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
        if (mSwipeGestureListener == null || isOnTabStrip(event)) return false;

        return mSwipeGestureListener.onTouchEvent(event);
    }

    private boolean isOnTabStrip(MotionEvent e) {
        return e.getY() <= mToolbar.getTabStripHeight();
    }

    /**
     * @return Whether or not the toolbar container is fully visible on screen.
     */
    private boolean isToolbarContainerFullyVisible() {
        return Float.compare(0f, getTranslationY()) == 0
                && mToolbarContainer.getVisibility() == VISIBLE;
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
                    .isKeyboardShowing(getContext(), ToolbarControlContainer.this)) {
                return false;
            }
            return true;
        }
    }
}
