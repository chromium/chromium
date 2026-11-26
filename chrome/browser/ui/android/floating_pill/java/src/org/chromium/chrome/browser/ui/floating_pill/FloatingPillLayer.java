// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.ui.floating_pill;

import android.view.View;
import android.view.ViewGroup;
import android.view.ViewStub;
import android.widget.LinearLayout;

import androidx.annotation.IntDef;

import org.chromium.base.Callback;
import org.chromium.base.lifetime.Destroyable;
import org.chromium.base.supplier.NonNullObservableSupplier;
import org.chromium.base.supplier.OneshotSupplier;
import org.chromium.build.annotations.NullMarked;
import org.chromium.build.annotations.Nullable;
import org.chromium.chrome.browser.browser_controls.BrowserControlsStateProvider;
import org.chromium.chrome.browser.browser_controls.BrowserControlsStateProvider.ControlsPosition;
import org.chromium.chrome.browser.browser_controls.TopControlLayer;
import org.chromium.chrome.browser.browser_controls.TopControlsStacker;
import org.chromium.chrome.browser.browser_controls.TopControlsStacker.TopControlType;
import org.chromium.chrome.browser.browser_controls.TopControlsStacker.TopControlVisibility;
import org.chromium.chrome.browser.flags.ChromeFeatureList;
import org.chromium.chrome.browser.ui.side_ui.SideUiStateProvider;
import org.chromium.chrome.browser.ui.side_ui.ViewMarginAdjusterForSideUi;
import org.chromium.components.browser_ui.widget.floating_pill.FloatingPillContainer;

import java.lang.annotation.ElementType;
import java.lang.annotation.Retention;
import java.lang.annotation.RetentionPolicy;
import java.lang.annotation.Target;
import java.util.SortedMap;
import java.util.TreeMap;

/**
 * The top controls layer for a window's floating pills, like auto browse's "Take over task" button.
 * It owns the window's single {@link FloatingPillContainer}, lays the added pills out side by side
 * in {@link PillType} order, and keeps the container on the bottom edge of the top controls, or a
 * margin below them when the toolbar is at the bottom, as the controls resize or move. See {@link
 * FloatingPillContainer#updateTopMargin}. The pills are centered on the web contents, next to any
 * side UI, and hidden while the omnibox is focused. The layer doesn't add to the controls' height,
 * and it ignores their scroll offset and min height. It only joins the {@link TopControlsStacker}
 * with the FloatingPillLayer feature; without it, it follows the controls' height on its own.
 */
@NullMarked
public class FloatingPillLayer implements TopControlLayer, Destroyable {
    /**
     * The kinds of pill. The values set the order the pills are laid out in, from the start, and
     * aren't persisted, so they can be renumbered freely. Callers must add at most one of each.
     */
    @IntDef({PillType.ACTOR_TAKE_OVER, PillType.TTC_VOICE_PLATE})
    @Retention(RetentionPolicy.SOURCE)
    @Target(ElementType.TYPE_USE)
    public @interface PillType {
        /** Auto browse's "Take over task" button. */
        int ACTOR_TAKE_OVER = 0;

        /** The TTC voice plate. */
        int TTC_VOICE_PLATE = 1;
    }

    private final ViewStub mContainerStub;
    private final TopControlsStacker mTopControlsStacker;
    private final BrowserControlsStateProvider mBrowserControlsStateProvider;
    private final NonNullObservableSupplier<Boolean> mOmniboxFocusStateSupplier;
    private final boolean mJoinsStacker;
    // The pills by type, at most one of each, in the order they're shown.
    private final SortedMap<Integer, View> mPills = new TreeMap<>();
    private final BrowserControlsStateProvider.Observer mBrowserControlsObserver =
            new BrowserControlsStateProvider.Observer() {
                @Override
                public void onTopControlsHeightChanged(
                        int topControlsHeight, int topControlsMinHeight) {
                    // In the stacker, this comes through onTopControlLayerHeightChanged().
                    if (!mJoinsStacker) updateTopMargin(topControlsHeight);
                }

                @Override
                public void onControlsPositionChanged(@ControlsPosition int controlsPosition) {
                    updateTopMargin(mBrowserControlsStateProvider.getTopControlsHeight());
                }
            };
    private final Callback<Boolean> mOmniboxFocusObserver = ignored -> updateVisibility();

    private @Nullable FloatingPillContainer mContainer;
    private @Nullable SideUiStateProvider mSideUiStateProvider;
    private @Nullable ViewMarginAdjusterForSideUi mSideUiMarginAdjuster;
    private boolean mDestroyed;

    /**
     * @param containerStub Where the window's pill container goes. Inflated with the first pill.
     * @param topControlsStacker The window's stacker, which this joins while it has pills.
     * @param browserControlsStateProvider Provides the height and position of the controls.
     * @param omniboxFocusStateSupplier Whether the omnibox is focused. The pills are hidden while
     *     it is, since they'd sit where the omnibox suggestions are drawn.
     * @param sideUiStateProviderSupplier Provides the space taken by side UI, once the window has
     *     it, so the pills stay centered on the web contents.
     */
    public FloatingPillLayer(
            ViewStub containerStub,
            TopControlsStacker topControlsStacker,
            BrowserControlsStateProvider browserControlsStateProvider,
            NonNullObservableSupplier<Boolean> omniboxFocusStateSupplier,
            OneshotSupplier<SideUiStateProvider> sideUiStateProviderSupplier) {
        mContainerStub = containerStub;
        mTopControlsStacker = topControlsStacker;
        mBrowserControlsStateProvider = browserControlsStateProvider;
        mOmniboxFocusStateSupplier = omniboxFocusStateSupplier;
        mJoinsStacker = ChromeFeatureList.sFloatingPillLayer.isEnabled();
        sideUiStateProviderSupplier.runSyncOrOnAvailable(this::onSideUiStateProviderAvailable);
    }

    /**
     * Shows {@code pill} in the window's pill container, placed against the browser controls, until
     * it's removed. Add it only while it's shown, and remove it when it's hidden: the layer reports
     * itself as visible to the stacker while it has any pill, unless the omnibox is focused.
     *
     * @param pill A pill-shaped view, e.g. a {@code FloatingPillView}. Must not have a parent.
     * @param type The kind of pill, which sets its position among the others.
     */
    public void addPill(View pill, @PillType int type) {
        assert !mDestroyed : "The layer was destroyed.";
        assert pill.getParent() == null : "The pill already has a parent.";
        assert !mPills.containsKey(type) : "A pill of this type was already added.";
        int index = mPills.headMap(type).size();

        FloatingPillContainer container = getOrInflateContainer();
        if (mPills.isEmpty()) startObserving();
        mPills.put(type, pill);
        container.addView(
                pill,
                index,
                new LinearLayout.LayoutParams(
                        ViewGroup.LayoutParams.WRAP_CONTENT, ViewGroup.LayoutParams.WRAP_CONTENT));
        updateVisibility();
        updateTopMargin(mBrowserControlsStateProvider.getTopControlsHeight());
    }

    /** Removes {@code pill} from the window's pill container. No-op if it wasn't added. */
    public void removePill(View pill) {
        if (!mPills.values().remove(pill)) return;
        assert mContainer != null;
        mContainer.removeView(pill);
        if (mPills.isEmpty()) stopObserving();
        updateVisibility();
    }

    @Override
    public void destroy() {
        mDestroyed = true;
        if (mPills.isEmpty()) return;
        mPills.clear();
        if (mContainer != null) mContainer.removeAllViews();
        stopObserving();
    }

    // TopControlLayer:

    @Override
    public @TopControlType int getTopControlType() {
        return TopControlType.FLOATING_PILL;
    }

    @Override
    public int getTopControlHeight() {
        return 0;
    }

    @Override
    public @TopControlVisibility int getTopControlVisibility() {
        // Matches the container. The stacker reads it when it needs it, so changes aren't reported:
        // with no height of its own, the layer doesn't change the controls' height.
        return isVisible() ? TopControlVisibility.VISIBLE : TopControlVisibility.HIDDEN;
    }

    @Override
    public boolean contributesToTotalHeight() {
        // The pills draw over the web contents, so they don't add to the controls' height.
        return false;
    }

    @Override
    public void onTopControlLayerHeightChanged(int topControlsHeight, int topControlsMinHeight) {
        updateTopMargin(topControlsHeight);
    }

    @Nullable FloatingPillContainer getContainerForTesting() {
        return mContainer;
    }

    private void onSideUiStateProviderAvailable(SideUiStateProvider sideUiStateProvider) {
        if (mDestroyed) return;
        mSideUiStateProvider = sideUiStateProvider;
        if (mContainer == null) return;
        mSideUiMarginAdjuster = new ViewMarginAdjusterForSideUi(mContainer);
        if (!mPills.isEmpty()) startObservingSideUi();
    }

    private FloatingPillContainer getOrInflateContainer() {
        if (mContainer != null) return mContainer;
        FloatingPillContainer container = (FloatingPillContainer) mContainerStub.inflate();
        if (mSideUiStateProvider != null) {
            mSideUiMarginAdjuster = new ViewMarginAdjusterForSideUi(container);
        }
        mContainer = container;
        return container;
    }

    private void startObserving() {
        if (mJoinsStacker) mTopControlsStacker.addControl(this);
        mBrowserControlsStateProvider.addObserver(mBrowserControlsObserver);
        mOmniboxFocusStateSupplier.addSyncObserver(mOmniboxFocusObserver);
        startObservingSideUi();
    }

    private void startObservingSideUi() {
        if (mSideUiStateProvider == null || mSideUiMarginAdjuster == null) return;
        mSideUiStateProvider.addObserver(mSideUiMarginAdjuster);
        mSideUiMarginAdjuster.updateMarginsForSideUi(mSideUiStateProvider.getCurrentSideUiSpecs());
    }

    private void stopObserving() {
        if (mJoinsStacker) mTopControlsStacker.removeControl(this);
        mBrowserControlsStateProvider.removeObserver(mBrowserControlsObserver);
        mOmniboxFocusStateSupplier.removeObserver(mOmniboxFocusObserver);
        if (mSideUiStateProvider != null && mSideUiMarginAdjuster != null) {
            mSideUiStateProvider.removeObserver(mSideUiMarginAdjuster);
        }
    }

    private boolean isVisible() {
        return !mPills.isEmpty() && !mOmniboxFocusStateSupplier.get();
    }

    private void updateVisibility() {
        if (mContainer == null) return;
        mContainer.setVisibility(isVisible() ? View.VISIBLE : View.GONE);
    }

    private void updateTopMargin(int topControlsHeight) {
        if (mContainer == null) return;
        mContainer.updateTopMargin(
                topControlsHeight,
                mBrowserControlsStateProvider.getControlsPosition() == ControlsPosition.BOTTOM);
    }
}
