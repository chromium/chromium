// Copyright 2016 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.ui.native_page;

import android.graphics.Rect;
import android.view.View;
import android.view.View.OnAttachStateChangeListener;
import android.widget.FrameLayout.LayoutParams;

import org.chromium.base.lifetime.Destroyable;
import org.chromium.base.supplier.ObservableSupplierImpl;
import org.chromium.build.annotations.NullMarked;
import org.chromium.build.annotations.Nullable;
import org.chromium.chrome.browser.flags.ChromeFeatureList;
import org.chromium.components.browser_ui.styles.ChromeColors;
import org.chromium.components.browser_ui.widget.gesture.BackPressHandler;
import org.chromium.components.browser_ui.widget.gesture.BackPressHandlerRegistry;
import org.chromium.content_public.browser.LoadUrlParams;

/**
 * A basic implementation of a white {@link NativePage} that docks below the toolbar. This class
 * handles default behavior for background color, URL updates and margins.
 *
 * <p>Sub-classes must call {@link #initWithView(View)} to finish setup.
 */
@NullMarked
public abstract class BasicNativePage implements NativePage, OnAttachStateChangeListener {
    private final NativePageHost mHost;
    private final int mBackgroundColor;
    private @Nullable BackPressHandler mBackPressHandler;
    private @Nullable BackPressHandlerRegistry mRegistry;
    private final ObservableSupplierImpl<Rect> mBrowserControlsMarginsSupplier =
            new ObservableSupplierImpl<>();
    private @Nullable Destroyable mMarginsAdapter;

    private @Nullable View mView;

    @SuppressWarnings("NullAway.Init")
    private String mUrl;

    private @Nullable SmoothTransitionDelegate mSmoothTransitionDelegate;

    protected BasicNativePage(NativePageHost host) {
        mHost = host;
        mBackgroundColor = ChromeColors.getPrimaryBackgroundColor(host.getContext(), false);
    }

    /** Sets the View contained in this native page and finishes BasicNativePage initialization. */
    protected void initWithView(View view) {
        assert mView == null : "initWithView() should only be called once";
        mView = view;

        mMarginsAdapter = mHost.createDefaultMarginAdapter(mBrowserControlsMarginsSupplier);
        // Update margins immediately if available rather than waiting for a posted notification.
        // Waiting for a posted notification could allow a layout pass to occur before the margins
        // are set.
        mBrowserControlsMarginsSupplier.addSyncObserverAndCallIfNonNull(this::updateMargins);
    }

    @Override
    public final View getView() {
        assert mView != null : "Need to call initWithView()";

        return mView;
    }

    @Override
    public SmoothTransitionDelegate enableSmoothTransition() {
        if (mSmoothTransitionDelegate == null) {
            mSmoothTransitionDelegate = new BasicSmoothTransitionDelegate(getView());
        }
        return mSmoothTransitionDelegate;
    }

    @Override
    public String getUrl() {
        return mUrl;
    }

    @Override
    public int getBackgroundColor() {
        return mBackgroundColor;
    }

    @Override
    public boolean needsToolbarShadow() {
        return true;
    }

    @Override
    public void updateForUrl(String url) {
        mUrl = url;
    }

    @Override
    public int getHeightOverlappedWithTopControls() {
        return 0;
    }

    @Override
    public void destroy() {
        if (mMarginsAdapter != null) {
            mMarginsAdapter.destroy();
        }

        if (mBackPressHandler != null && getView() != null) {
            getView().removeOnAttachStateChangeListener(this);
        }
    }

    @Override
    public void onViewAttachedToWindow(View view) {
        if (mBackPressHandler != null && mRegistry != null) {
            mRegistry.addHandler(mBackPressHandler, BackPressHandler.Type.NATIVE_PAGE);
        }
    }

    @Override
    public void onViewDetachedFromWindow(View view) {
        if (mBackPressHandler != null && mRegistry != null) {
            mRegistry.removeHandler(mBackPressHandler);
        }
    }

    /**
     * Tells the native page framework that the url should be changed.
     * @param url The URL of the page.
     * @param replaceLastUrl Whether the last navigation entry should be replaced with the new URL.
     */
    public void onStateChange(String url, boolean replaceLastUrl) {
        if (url.equals(mUrl)) return;
        LoadUrlParams params = new LoadUrlParams(url);
        params.setShouldReplaceCurrentEntry(replaceLastUrl);
        mHost.loadUrl(params, /* incognito= */ false);
    }

    /** Updates the top margin depending on whether the browser controls are shown or hidden. */
    private void updateMargins(Rect margins) {
        LayoutParams layoutParams =
                new LayoutParams(LayoutParams.MATCH_PARENT, LayoutParams.MATCH_PARENT);
        layoutParams.setMargins(margins.left, margins.top, margins.left, margins.bottom);
        getView().setLayoutParams(layoutParams);
    }

    public @Nullable SmoothTransitionDelegate getSmoothTransitionDelegateForTesting() {
        return mSmoothTransitionDelegate;
    }

    /**
     * Opts this page into centralized back press handling. The provided handler will be
     * automatically registered and unregistered with the registry as the page's view is attached
     * and detached.
     *
     * @param handler The BackPressHandler to be managed (e.g., a Coordinator).
     * @param registry The system that manages back press handlers (e.g., BackPressManager).
     */
    protected void setBackPressHandler(
            BackPressHandler handler, BackPressHandlerRegistry registry) {
        if (!ChromeFeatureList.isEnabled(
                ChromeFeatureList.ENABLE_ESCAPE_HANDLING_FOR_SECONDARY_ACTIVITIES)) {
            return;
        }
        assert mView != null : "setBackPressHandler must be called after initWithView()";
        mBackPressHandler = handler;
        mRegistry = registry;
        getView().addOnAttachStateChangeListener(this);
    }
}
