// Copyright 2016 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.ui.native_page;

import android.graphics.Rect;
import android.view.View;
import android.widget.FrameLayout.LayoutParams;

import org.chromium.base.Callback;
import org.chromium.base.supplier.DestroyableObservableSupplier;
import org.chromium.components.browser_ui.styles.ChromeColors;
import org.chromium.content_public.browser.LoadUrlParams;

/**
 * A basic implementation of a white {@link NativePage} that docks below the toolbar. This class
 * handles default behavior for background color, URL updates and margins.
 *
 * Sub-classes must call {@link #initWithView(View)} to finish setup.
 */
public abstract class BasicNativePage implements NativePage {
    private final NativePageHost mHost;
    private final int mBackgroundColor;
    private DestroyableObservableSupplier<Rect> mMarginSupplier;
    private Callback<Rect> mMarginObserver;
    private View mView;
    private String mUrl;
    private SmoothTransitionDelegate mSmoothTransitionDelegate;

    protected BasicNativePage(NativePageHost host) {
        mHost = host;
        mBackgroundColor = ChromeColors.getPrimaryBackgroundColor(host.getContext(), false);
    }

    /** Sets the View contained in this native page and finishes BasicNativePage initialization. */
    protected void initWithView(View view) {
        assert mView == null : "initWithView() should only be called once";
        mView = view;

        mMarginObserver = result -> updateMargins(result);
        mMarginSupplier = mHost.createDefaultMarginSupplier();
        mMarginSupplier.addObserver(mMarginObserver);

        // Update margins immediately if available rather than waiting for a posted notification.
        // Waiting for a posted notification could allow a layout pass to occur before the margins
        // are set.
        if (mMarginSupplier.get() != null) {
            updateMargins(mMarginSupplier.get());
        }
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
        if (mMarginSupplier != null) {
            mMarginSupplier.removeObserver(mMarginObserver);
            mMarginSupplier.destroy();
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

    public SmoothTransitionDelegate getSmoothTransitionDelegateForTesting() {
        return mSmoothTransitionDelegate;
    }
}
