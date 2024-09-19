// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.components.embedder_support.view;

import android.content.Context;
import android.content.res.Configuration;
import android.graphics.Rect;
import android.os.Handler;
import android.util.SparseArray;
import android.view.DragEvent;
import android.view.KeyEvent;
import android.view.MotionEvent;
import android.view.PointerIcon;
import android.view.View;
import android.view.View.OnDragListener;
import android.view.View.OnSystemUiVisibilityChangeListener;
import android.view.ViewGroup.OnHierarchyChangeListener;
import android.view.ViewStructure;
import android.view.accessibility.AccessibilityNodeProvider;
import android.view.autofill.AutofillValue;
import android.view.inputmethod.EditorInfo;
import android.view.inputmethod.InputConnection;
import android.widget.FrameLayout;

import androidx.annotation.Nullable;

import org.chromium.base.ObserverList;
import org.chromium.base.TraceEvent;
import org.chromium.components.autofill.AndroidAutofillFeatures;
import org.chromium.components.embedder_support.util.TouchEventFilter;
import org.chromium.content_public.browser.ImeAdapter;
import org.chromium.content_public.browser.RenderCoordinates;
import org.chromium.content_public.browser.SmartClipProvider;
import org.chromium.content_public.browser.ViewEventSink;
import org.chromium.content_public.browser.WebContents;
import org.chromium.content_public.browser.WebContentsAccessibility;
import org.chromium.ui.accessibility.AccessibilityState;
import org.chromium.ui.base.EventForwarder;
import org.chromium.ui.base.EventOffsetHandler;
import org.chromium.ui.base.ViewAndroidDelegate;
import org.chromium.ui.dragdrop.DragEventDispatchHelper.DragEventDispatchDestination;

import java.util.function.Supplier;

/**
 * The containing view for {@link WebContents} that exists in the Android UI hierarchy and exposes
 * the various {@link View} functionality to it.
 *
 * While ContentView is a ViewGroup, the only place that should add children is ViewAndroidDelegate,
 * and only for cases that WebContentsAccessibility handles (such as anchoring popups). This is
 * because the accessibility support provided by WebContentsAccessibility ignores all child views.
 * In other words, any children added to this are *not* accessible.
 */
public class ContentView extends FrameLayout
        implements ViewEventSink.InternalAccessDelegate,
                SmartClipProvider,
                OnHierarchyChangeListener,
                OnSystemUiVisibilityChangeListener,
                OnDragListener,
                DragEventDispatchDestination {
    // Default value to signal that the ContentView's size need not be overridden.
    public static final int DEFAULT_MEASURE_SPEC =
            MeasureSpec.makeMeasureSpec(0, MeasureSpec.UNSPECIFIED);

    @Nullable private WebContents mWebContents;
    private boolean mIsObscuredForAccessibility;
    private final ObserverList<OnHierarchyChangeListener> mHierarchyChangeListeners =
            new ObserverList<>();
    private final ObserverList<OnSystemUiVisibilityChangeListener> mSystemUiChangeListeners =
            new ObserverList<>();
    private final ObserverList<OnDragListener> mOnDragListeners = new ObserverList<>();
    private ViewEventSink mViewEventSink;
    @Nullable private Supplier<PointerIcon> mStylusWritingIconSupplier;

    /**
     * The desired size of this view in {@link MeasureSpec}. Set by the host
     * when it should be different from that of the parent.
     */
    private int mDesiredWidthMeasureSpec = DEFAULT_MEASURE_SPEC;

    private int mDesiredHeightMeasureSpec = DEFAULT_MEASURE_SPEC;

    private EventOffsetHandler mDragDropEventOffsetHandler;
    private boolean mDeferKeepScreenOnChanges;
    private Boolean mPendingKeepScreenOnValue;

    /**
     * Constructs a new ContentView for the appropriate Android version.
     *
     * @param context The Context the view is running in, through which it can access the current
     *     theme, resources, etc.
     * @param webContents The WebContents managing this content view.
     * @return an instance of a ContentView.
     */
    public static ContentView createContentView(
            Context context, @Nullable WebContents webContents) {
        return new ContentView(context, webContents);
    }

    /**
     * Creates an instance of a ContentView.
     *
     * @param context The Context the view is running in, through which it can access the current
     *     theme, resources, etc.
     * @param webContents A pointer to the WebContents managing this content view.
     */
    protected ContentView(Context context, WebContents webContents) {
        super(context, null, android.R.attr.webViewStyle);

        if (getScrollBarStyle() == View.SCROLLBARS_INSIDE_OVERLAY) {
            setHorizontalScrollBarEnabled(false);
            setVerticalScrollBarEnabled(false);
        }

        mWebContents = webContents;

        setFocusable(true);
        setFocusableInTouchMode(true);
        setDefaultFocusHighlightEnabled(false);

        setOnHierarchyChangeListener(this);
        setOnSystemUiVisibilityChangeListener(this);
        setOnDragListener(this);
    }

    protected WebContentsAccessibility getWebContentsAccessibility() {
        return webContentsAttached()
                ? WebContentsAccessibility.fromWebContents(mWebContents)
                : null;
    }

    public WebContents getWebContents() {
        return mWebContents;
    }

    public void setWebContents(WebContents webContents) {
        boolean wasFocused = isFocused();
        boolean wasWindowFocused = hasWindowFocus();
        boolean wasAttached = isAttachedToWindow();
        boolean wasObscured = mIsObscuredForAccessibility;
        if (wasFocused) onFocusChanged(false, View.FOCUS_FORWARD, null);
        if (wasWindowFocused) onWindowFocusChanged(false);
        if (wasAttached) onDetachedFromWindow();
        if (wasObscured) setIsObscuredForAccessibility(false);
        mWebContents = webContents;
        mViewEventSink = null;
        if (wasFocused) onFocusChanged(true, View.FOCUS_FORWARD, null);
        if (wasWindowFocused) onWindowFocusChanged(true);
        if (wasAttached) onAttachedToWindow();
        if (wasObscured) setIsObscuredForAccessibility(true);
    }

    /** Control whether WebContentsAccessibility will respond to accessibility requests. */
    public void setIsObscuredForAccessibility(boolean isObscured) {
        if (mIsObscuredForAccessibility == isObscured) return;
        mIsObscuredForAccessibility = isObscured;
        WebContentsAccessibility wcax = getWebContentsAccessibility();
        if (wcax == null) return;
        wcax.setObscuredByAnotherView(mIsObscuredForAccessibility);
    }

    /**
     * Instructs the ContentView to defer (or stop deferring) the application of changes to its
     * KeepScreenOn flag. If deferring is being turned off, super.setKeepScreenOn will be called
     * with the latest value passed to setKeepScreenOn.
     */
    public void setDeferKeepScreenOnChanges(boolean deferKeepScreenOnChanges) {
        mDeferKeepScreenOnChanges = deferKeepScreenOnChanges;
        if (!mDeferKeepScreenOnChanges && mPendingKeepScreenOnValue != null) {
            super.setKeepScreenOn(mPendingKeepScreenOnValue);
            mPendingKeepScreenOnValue = null;
        }
    }

    /**
     * Set {@link EventOffsetHandler} used to handle drag event offsets. Offsets are
     * provided if the content view is has a different coordinate base than the physical screen
     * (e.g. top browser control).
     * @param handler Handler used to adjust drag event offsets.
     */
    public void setEventOffsetHandlerForDragDrop(EventOffsetHandler handler) {
        assert mDragDropEventOffsetHandler == null || handler == null
                : "Non-null DragDropEventOffsetHandler was overwritten with another.";
        mDragDropEventOffsetHandler = handler;
    }

    public void setStylusWritingIconSupplier(Supplier<PointerIcon> iconSupplier) {
        mStylusWritingIconSupplier = iconSupplier;
    }

    @Override
    public void setKeepScreenOn(boolean keepScreenOn) {
        if (mDeferKeepScreenOnChanges) {
            mPendingKeepScreenOnValue = keepScreenOn;
        } else {
            super.setKeepScreenOn(keepScreenOn);
        }
    }

    /**
     * Set the desired size of the view. The values are in {@link MeasureSpec}.
     * @param width The width of the content view.
     * @param height The height of the content view.
     */
    public void setDesiredMeasureSpec(int width, int height) {
        mDesiredWidthMeasureSpec = width;
        mDesiredHeightMeasureSpec = height;
    }

    @Override
    public void setOnHierarchyChangeListener(OnHierarchyChangeListener listener) {
        assert listener == this : "Use add/removeOnHierarchyChangeListener instead.";
        super.setOnHierarchyChangeListener(listener);
    }

    /**
     * Registers the given listener to receive state changes for the content view hierarchy.
     * @param listener Listener to receive view hierarchy state changes.
     */
    public void addOnHierarchyChangeListener(OnHierarchyChangeListener listener) {
        mHierarchyChangeListeners.addObserver(listener);
    }

    /**
     * Unregisters the given listener from receiving state changes for the content view hierarchy.
     * @param listener Listener that doesn't want to receive view hierarchy state changes.
     */
    public void removeOnHierarchyChangeListener(OnHierarchyChangeListener listener) {
        mHierarchyChangeListeners.removeObserver(listener);
    }

    @Override
    public void setOnSystemUiVisibilityChangeListener(OnSystemUiVisibilityChangeListener listener) {
        assert listener == this : "Use add/removeOnSystemUiVisibilityChangeListener instead.";
        super.setOnSystemUiVisibilityChangeListener(listener);
    }

    /**
     * Registers the given listener to receive system UI visibility state changes.
     * @param listener Listener to receive system UI visibility state changes.
     */
    public void addOnSystemUiVisibilityChangeListener(OnSystemUiVisibilityChangeListener listener) {
        mSystemUiChangeListeners.addObserver(listener);
    }

    /**
     * Unregisters the given listener from receiving system UI visibility state changes.
     * @param listener Listener that doesn't want to receive state changes.
     */
    public void removeOnSystemUiVisibilityChangeListener(
            OnSystemUiVisibilityChangeListener listener) {
        mSystemUiChangeListeners.removeObserver(listener);
    }

    /**
     * Registers the given listener to receive DragEvent updates on this view.
     * @param listener Listener to receive DragEvent updates.
     */
    public void addOnDragListener(OnDragListener listener) {
        mOnDragListeners.addObserver(listener);
    }

    /**
     * Unregisters the given listener to receive DragEvent updates on this view.
     * @param listener Listener that doesn't want to receive DragEvent updates anymore.
     */
    public void removeOnDragListener(OnDragListener listener) {
        mOnDragListeners.removeObserver(listener);
    }

    @Override
    public void setOnDragListener(OnDragListener listener) {
        assert listener == this : "Use add/removeOnDragListener instead.";
        super.setOnDragListener(listener);
    }

    // View.OnHierarchyChangeListener implementation

    @Override
    public void onChildViewRemoved(View parent, View child) {
        for (OnHierarchyChangeListener listener : mHierarchyChangeListeners) {
            listener.onChildViewRemoved(parent, child);
        }
    }

    @Override
    public void onChildViewAdded(View parent, View child) {
        for (OnHierarchyChangeListener listener : mHierarchyChangeListeners) {
            listener.onChildViewAdded(parent, child);
        }
    }

    // View.OnHierarchyChangeListener implementation

    @Override
    public void onSystemUiVisibilityChange(int visibility) {
        for (OnSystemUiVisibilityChangeListener listener : mSystemUiChangeListeners) {
            listener.onSystemUiVisibilityChange(visibility);
        }
    }

    // View.OnDragListener implementation

    @Override
    public boolean onDrag(View view, DragEvent event) {
        for (OnDragListener listener : mOnDragListeners) {
            listener.onDrag(view, event);
        }
        // Do not consume the drag event to allow #onDragEvent to be called.
        return false;
    }

    @Override
    protected void onMeasure(int widthMeasureSpec, int heightMeasureSpec) {
        if (mDesiredWidthMeasureSpec != DEFAULT_MEASURE_SPEC) {
            widthMeasureSpec = mDesiredWidthMeasureSpec;
        }
        if (mDesiredHeightMeasureSpec != DEFAULT_MEASURE_SPEC) {
            heightMeasureSpec = mDesiredHeightMeasureSpec;
        }
        super.onMeasure(widthMeasureSpec, heightMeasureSpec);
    }

    @Override
    public AccessibilityNodeProvider getAccessibilityNodeProvider() {
        WebContentsAccessibility wcax = getWebContentsAccessibility();
        return (wcax != null) ? wcax.getAccessibilityNodeProvider() : null;
    }

    // Needed by ViewEventSink.InternalAccessDelegate
    @Override
    public void onScrollChanged(int l, int t, int oldl, int oldt) {
        super.onScrollChanged(l, t, oldl, oldt);
    }

    @Override
    public InputConnection onCreateInputConnection(EditorInfo outAttrs) {
        // Calls may come while/after WebContents is destroyed. See https://crbug.com/821750#c8.
        if (!hasValidWebContents()) return null;
        return ImeAdapter.fromWebContents(mWebContents).onCreateInputConnection(outAttrs);
    }

    @Override
    public boolean onCheckIsTextEditor() {
        if (!hasValidWebContents()) return false;
        return ImeAdapter.fromWebContents(mWebContents).onCheckIsTextEditor();
    }

    @Override
    protected void onFocusChanged(boolean gainFocus, int direction, Rect previouslyFocusedRect) {
        try {
            TraceEvent.begin("ContentView.onFocusChanged");
            super.onFocusChanged(gainFocus, direction, previouslyFocusedRect);
            if (hasValidWebContents()) {
                getViewEventSink().setHideKeyboardOnBlur(true);
                getViewEventSink().onViewFocusChanged(gainFocus);
            }
        } finally {
            TraceEvent.end("ContentView.onFocusChanged");
        }
    }

    @Override
    public void onWindowFocusChanged(boolean hasWindowFocus) {
        super.onWindowFocusChanged(hasWindowFocus);
        if (hasValidWebContents()) {
            getViewEventSink().onWindowFocusChanged(hasWindowFocus);
        }
    }

    @Override
    public boolean onKeyUp(int keyCode, KeyEvent event) {
        EventForwarder forwarder = getEventForwarder();
        return forwarder != null ? forwarder.onKeyUp(keyCode, event) : false;
    }

    @Override
    public boolean dispatchKeyEvent(KeyEvent event) {
        if (!isFocused()) return super.dispatchKeyEvent(event);
        EventForwarder forwarder = getEventForwarder();
        return forwarder != null ? forwarder.dispatchKeyEvent(event) : false;
    }

    @Override
    public boolean onDragEvent(DragEvent event) {
        EventForwarder forwarder = getEventForwarder();
        return forwarder != null ? forwarder.onDragEvent(event, this) : false;
    }

    @Override
    public boolean onTouchEvent(MotionEvent event) {
        if (TouchEventFilter.hasInvalidToolType(event)) return false;
        EventForwarder forwarder = getEventForwarder();
        boolean ret = forwarder != null ? forwarder.onTouchEvent(event) : false;
        return ret;
    }

    /**
     * Mouse move events are sent on hover enter, hover move and hover exit.
     * They are sent on hover exit because sometimes it acts as both a hover
     * move and hover exit.
     */
    @Override
    public boolean onHoverEvent(MotionEvent event) {
        EventForwarder forwarder = getEventForwarder();
        boolean consumed = forwarder != null ? forwarder.onHoverEvent(event) : false;
        if (!AccessibilityState.isTouchExplorationEnabled()) super.onHoverEvent(event);
        return consumed;
    }

    @Override
    public boolean onGenericMotionEvent(MotionEvent event) {
        EventForwarder forwarder = getEventForwarder();
        return forwarder != null ? forwarder.onGenericMotionEvent(event) : false;
    }

    @Override
    public PointerIcon onResolvePointerIcon(MotionEvent event, int pointerIndex) {
        PointerIcon icon = null;
        if (mStylusWritingIconSupplier != null) {
            icon = mStylusWritingIconSupplier.get();
        }
        if (icon != null) {
            return icon;
        }
        return super.onResolvePointerIcon(event, pointerIndex);
    }

    @Nullable
    private EventForwarder getEventForwarder() {
        return webContentsAttached() ? mWebContents.getEventForwarder() : null;
    }

    private ViewEventSink getViewEventSink() {
        if (mViewEventSink == null && hasValidWebContents()) {
            mViewEventSink = ViewEventSink.from(mWebContents);
        }
        return mViewEventSink;
    }

    @Override
    public boolean performLongClick() {
        return false;
    }

    @Override
    protected void onConfigurationChanged(Configuration newConfig) {
        if (hasValidWebContents()) {
            getViewEventSink().onConfigurationChanged(newConfig);
        }
        super.onConfigurationChanged(newConfig);
    }

    /**
     * Currently the ContentView scrolling happens in the native side. In
     * the Java view system, it is always pinned at (0, 0). scrollBy() and scrollTo()
     * are overridden, so that View's mScrollX and mScrollY will be unchanged at
     * (0, 0). This is critical for drawing ContentView correctly.
     */
    @Override
    public void scrollBy(int x, int y) {
        EventForwarder forwarder = getEventForwarder();
        if (forwarder != null) forwarder.scrollBy(x, y);
    }

    @Override
    public void scrollTo(int x, int y) {
        EventForwarder forwarder = getEventForwarder();
        if (forwarder != null) forwarder.scrollTo(x, y);
    }

    @Override
    protected int computeHorizontalScrollExtent() {
        RenderCoordinates rc = getRenderCoordinates();
        return rc != null ? rc.getLastFrameViewportWidthPixInt() : 0;
    }

    @Override
    protected int computeHorizontalScrollOffset() {
        RenderCoordinates rc = getRenderCoordinates();
        return rc != null ? rc.getScrollXPixInt() : 0;
    }

    @Override
    protected int computeHorizontalScrollRange() {
        RenderCoordinates rc = getRenderCoordinates();
        return rc != null ? rc.getContentWidthPixInt() : 0;
    }

    @Override
    protected int computeVerticalScrollExtent() {
        RenderCoordinates rc = getRenderCoordinates();
        return rc != null ? rc.getLastFrameViewportHeightPixInt() : 0;
    }

    @Override
    protected int computeVerticalScrollOffset() {
        RenderCoordinates rc = getRenderCoordinates();
        return rc != null ? rc.getScrollYPixInt() : 0;
    }

    @Override
    protected int computeVerticalScrollRange() {
        RenderCoordinates rc = getRenderCoordinates();
        return rc != null ? rc.getContentHeightPixInt() : 0;
    }

    private RenderCoordinates getRenderCoordinates() {
        return hasValidWebContents() ? RenderCoordinates.fromWebContents(mWebContents) : null;
    }

    // End FrameLayout overrides.

    @Override
    public boolean awakenScrollBars(int startDelay, boolean invalidate) {
        // For the default implementation of ContentView which draws the scrollBars on the native
        // side, calling this function may get us into a bad state where we keep drawing the
        // scrollBars, so disable it by always returning false.
        if (getScrollBarStyle() == View.SCROLLBARS_INSIDE_OVERLAY) return false;
        return super.awakenScrollBars(startDelay, invalidate);
    }

    @Override
    protected void onAttachedToWindow() {
        super.onAttachedToWindow();
        if (hasValidWebContents()) {
            getViewEventSink().onAttachedToWindow();
        }
    }

    @Override
    protected void onDetachedFromWindow() {
        super.onDetachedFromWindow();
        if (hasValidWebContents()) {
            getViewEventSink().onDetachedFromWindow();
        }
    }

    // Implements SmartClipProvider
    @Override
    public void extractSmartClipData(int x, int y, int width, int height) {
        if (hasValidWebContents()) {
            mWebContents.requestSmartClipExtract(x, y, width, height);
        }
    }

    // Implements SmartClipProvider
    @Override
    public void setSmartClipResultHandler(final Handler resultHandler) {
        if (hasValidWebContents()) {
            mWebContents.setSmartClipResultHandler(resultHandler);
        }
    }

    @Override
    public void onProvideVirtualStructure(final ViewStructure structure) {
        WebContentsAccessibility wcax = getWebContentsAccessibility();
        if (wcax != null) wcax.onProvideVirtualStructure(structure, false);
    }

    @Override
    public void autofill(final SparseArray<AutofillValue> values) {
        ViewAndroidDelegate viewDelegate = mWebContents.getViewAndroidDelegate();
        if (viewDelegate == null || !viewDelegate.providesAutofillStructure()) {
            if (allowAutofillViaAccessibilityAPI()) {
                super.autofill(values);
            }
            return;
        }
        viewDelegate.autofill(values);
    }

    @Override
    public void onProvideAutofillVirtualStructure(ViewStructure structure, int flags) {
        ViewAndroidDelegate viewDelegate = mWebContents.getViewAndroidDelegate();
        if (viewDelegate == null || !viewDelegate.providesAutofillStructure()) {
            if (allowAutofillViaAccessibilityAPI()) {
                super.onProvideAutofillVirtualStructure(structure, flags);
            }
            return;
        }
        viewDelegate.onProvideAutofillVirtualStructure(structure, flags);
    }

    private boolean allowAutofillViaAccessibilityAPI() {
        return !AndroidAutofillFeatures.ANDROID_AUTOFILL_DEPRECATE_ACCESSIBILITY_API.isEnabled();
    }

    ///////////////////////////////////////////////////////////////////////////////////////////////
    //              Start Implementation of ViewEventSink.InternalAccessDelegate                 //
    ///////////////////////////////////////////////////////////////////////////////////////////////

    @Override
    public boolean super_onKeyUp(int keyCode, KeyEvent event) {
        return super.onKeyUp(keyCode, event);
    }

    @Override
    public boolean super_dispatchKeyEvent(KeyEvent event) {
        return super.dispatchKeyEvent(event);
    }

    @Override
    public boolean super_onGenericMotionEvent(MotionEvent event) {
        return super.onGenericMotionEvent(event);
    }

    ///////////////////////////////////////////////////////////////////////////////////////////////
    //                End Implementation of ViewEventSink.InternalAccessDelegate                 //
    ///////////////////////////////////////////////////////////////////////////////////////////////

    private boolean hasValidWebContents() {
        return mWebContents != null && !mWebContents.isDestroyed();
    }

    private boolean webContentsAttached() {
        return hasValidWebContents() && mWebContents.getTopLevelNativeWindow() != null;
    }

    ///////////////////////////////////////////////////////////////////////////////////////////////
    //              Start Implementation of DragEventDispatchDestination                         //
    ///////////////////////////////////////////////////////////////////////////////////////////////
    @Override
    public View view() {
        return this;
    }

    @Override
    public boolean onDragEventWithOffset(DragEvent event, int dx, int dy) {
        if (mDragDropEventOffsetHandler == null) return super.dispatchDragEvent(event);

        mDragDropEventOffsetHandler.onPreDispatchDragEvent(event.getAction(), dx, dy);
        boolean ret = super.dispatchDragEvent(event);
        mDragDropEventOffsetHandler.onPostDispatchDragEvent(event.getAction());
        return ret;
    }
}
