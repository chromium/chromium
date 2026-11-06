// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.test.transit.tab_strip;

import static org.chromium.build.NullUtil.assumeNonNull;

import org.chromium.base.test.transit.ConditionStatusWithResult;
import org.chromium.base.test.transit.ConditionWithResult;
import org.chromium.build.annotations.NullMarked;
import org.chromium.build.annotations.Nullable;
import org.chromium.chrome.browser.compositor.overlays.strip.StripLayoutGroupTitle;
import org.chromium.chrome.browser.compositor.overlays.strip.StripLayoutHelper;
import org.chromium.chrome.browser.compositor.overlays.strip.StripLayoutHelperManager;
import org.chromium.chrome.browser.compositor.overlays.strip.StripLayoutTab;
import org.chromium.chrome.browser.compositor.overlays.strip.StripLayoutView;
import org.chromium.chrome.browser.layouts.components.VirtualView;
import org.chromium.ui.dragdrop.DragDropGlobalState;

import java.util.ArrayList;
import java.util.List;
import java.util.function.Function;
import java.util.function.Supplier;

/**
 * Fulfilled when a {@link StripLayoutView} of the tab strip (a tab, a strip button or a tab's close
 * button) is clickable: a click on its center would reach it.
 *
 * <p>Like {@link org.chromium.base.test.transit.ViewElement}'s displayed Condition, this one
 * Condition is used for every kind of strip view. Not fulfilled if:
 *
 * <ul>
 *   <li>the strip of the other model (regular or incognito) is shown,
 *   <li>the strip isn't fully shown or isn't settled (see {@link #whyNotSettled}),
 *   <li>the view doesn't exist, is closing or isn't visible,
 *   <li>a click on its center would reach something else, e.g. because the center is off the strip,
 *       under a strip button or a pinned tab, past the new tab button, or the view is a hidden
 *       close button. This uses {@link StripLayoutHelperManager#getClickedViewForTesting}, the same
 *       hit test as a real click.
 * </ul>
 *
 * @param <ViewT> the type of the strip view.
 */
@NullMarked
public class StripViewCondition<ViewT extends StripLayoutView> extends ConditionWithResult<ViewT> {
    private final Supplier<@Nullable StripLayoutHelperManager> mManagerSupplier;
    private final boolean mIncognito;
    private final String mViewName;
    private final Function<StripLayoutHelperManager, @Nullable ViewT> mViewGetter;

    /**
     * @param managerSupplier Supplies the tab strip.
     * @param incognito Whether the view is on the incognito strip.
     * @param viewName The name of the view, for the description.
     * @param viewGetter Gets the view, or null if there is none. Run on the UI thread.
     */
    public StripViewCondition(
            Supplier<@Nullable StripLayoutHelperManager> managerSupplier,
            boolean incognito,
            String viewName,
            Function<StripLayoutHelperManager, @Nullable ViewT> viewGetter) {
        super(/* isRunOnUiThread= */ true);
        mManagerSupplier = dependOnSupplier(managerSupplier, "StripLayoutHelperManager");
        mIncognito = incognito;
        mViewName = viewName;
        mViewGetter = viewGetter;
    }

    @Override
    protected ConditionStatusWithResult<ViewT> resolveWithSuppliers() {
        StripLayoutHelperManager manager = assumeNonNull(mManagerSupplier.get());
        String notSettled = whyNotSettled(manager, mIncognito);
        if (notSettled != null) {
            return notFulfilled(notSettled).withoutResult();
        }
        @Nullable ViewT view = mViewGetter.apply(manager);
        if (view == null) {
            return notFulfilled("Not on the strip").withoutResult();
        }
        if (view.isDying()) {
            return notFulfilled("Closing").withoutResult();
        }
        if (!view.isVisible()) {
            return notFulfilled("Not visible, drawX=%.1f", view.getDrawX()).withoutResult();
        }
        float centerX = view.getDrawX() + view.getWidth() / 2;
        float centerY = view.getDrawY() + view.getHeight() / 2;
        if (centerX < 0 || centerX > manager.getWidth()) {
            return notFulfilled("Center off the strip, centerX=%.1f", centerX).withoutResult();
        }
        StripLayoutView clicked = manager.getClickedViewForTesting(centerX, centerY);
        if (clicked != view) {
            return notFulfilled(
                            "Center would hit %s, centerX=%.1f",
                            describe(manager.getStripLayoutHelper(mIncognito), clicked), centerX)
                    .withoutResult();
        }
        return fulfilled("drawX=%.1f, width=%.1f", view.getDrawX(), view.getWidth())
                .withResult(view);
    }

    @Override
    public String buildDescription() {
        return String.format(
                "%s clickable on the %s tab strip",
                mViewName, mIncognito ? "incognito" : "regular");
    }

    /**
     * Returns null if the strip of the given model (regular or incognito) is the one shown and it
     * is settled, or why not. Run on the UI thread.
     *
     * <p>Clicks on the strip are hit-tested against the current position of its tabs and buttons,
     * so they need the strip to be fully shown, not animating, scrolling, reordering a tab, or in a
     * tab drag and drop.
     */
    static @Nullable String whyNotSettled(StripLayoutHelperManager manager, boolean incognito) {
        // Hidden behind the Hub, scrolled away with the top controls, or during a height
        // transition: the strip has no virtual views then. StripLayoutHelperManager#simulateClick()
        // doesn't check this.
        List<VirtualView> virtualViews = new ArrayList<>();
        manager.getVirtualViews(virtualViews);
        if (virtualViews.isEmpty()) {
            return String.format(
                    "Strip not fully shown, visibilityState=%d",
                    manager.getStripVisibilityStateSupplier().get());
        }
        if (manager.isIncognito() != incognito) {
            return "Other model's strip shown";
        }
        // The strip drops clicks while a tab drag and drop is in progress.
        if (DragDropGlobalState.hasValue()) {
            return "Drag and drop in progress";
        }
        StripLayoutHelper strip = manager.getStripLayoutHelper(incognito);
        if (strip.getInReorderModeForTesting()) {
            return "Reordering";
        }
        return strip.isAnimatingForTesting() ? "Animating" : null;
    }

    private static String describe(StripLayoutHelper helper, @Nullable StripLayoutView view) {
        if (view == null) return "nothing or another trailing button";
        if (view == helper.getNewTabButton()) return "the new tab button";
        if (view == helper.getTabSearchButton()) return "the tab search button";
        if (view instanceof StripLayoutTab tab) {
            return (tab.getIsPinned() ? "pinned tab " : "tab ") + tab.getTabId();
        }
        if (view instanceof StripLayoutGroupTitle) return "a tab group title";
        return "another button";
    }
}
