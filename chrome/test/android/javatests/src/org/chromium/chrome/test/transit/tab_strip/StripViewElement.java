// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.test.transit.tab_strip;

import static org.chromium.build.NullUtil.assumeNonNull;

import org.chromium.base.test.transit.Condition;
import org.chromium.base.test.transit.ConditionStatus;
import org.chromium.base.test.transit.ConditionWithResult;
import org.chromium.base.test.transit.Element;
import org.chromium.base.test.transit.TripBuilder;
import org.chromium.build.annotations.NullMarked;
import org.chromium.build.annotations.Nullable;
import org.chromium.chrome.browser.compositor.overlays.strip.StripLayoutHelperManager;
import org.chromium.chrome.browser.compositor.overlays.strip.StripLayoutView;

import java.util.function.Function;
import java.util.function.Supplier;

/**
 * A {@link StripLayoutView} of the tab strip (a tab, a strip button or a tab's close button), the
 * tab strip's equivalent of a {@link org.chromium.base.test.transit.ViewElement}. The strip is
 * drawn by the compositor and has no Android Views.
 *
 * <p>Its ENTER Condition is a {@link StripViewCondition}: the view is clickable. An optional one,
 * like {@link org.chromium.base.test.transit.OptionalViewElement}, has no ENTER Condition and is
 * only checked by {@link #checkPresent()} or right before {@link #clickTo()}.
 *
 * <p>It has no EXIT Condition by default, since strip views usually stay on the strip when their
 * owner is exited. Call {@link #expectRemoved()} before a Transition that closes a tab.
 *
 * @param <ViewT> the type of the strip view.
 */
@NullMarked
public class StripViewElement<ViewT extends StripLayoutView> extends Element<ViewT> {
    private final Supplier<@Nullable StripLayoutHelperManager> mManagerSupplier;
    private final boolean mIncognito;
    private final String mViewName;
    private final Function<StripLayoutHelperManager, @Nullable ViewT> mViewGetter;
    private final boolean mOptional;

    /**
     * @param managerSupplier Supplies the tab strip.
     * @param incognito Whether the view is on the incognito strip.
     * @param viewName The name of the view, for descriptions and the Element id.
     * @param viewGetter Gets the view, or null if there is none. Run on the UI thread.
     * @param optional Whether the view might not be clickable when the owner is entered, e.g. the
     *     close button of a narrow tab.
     */
    public StripViewElement(
            Supplier<@Nullable StripLayoutHelperManager> managerSupplier,
            boolean incognito,
            String viewName,
            Function<StripLayoutHelperManager, @Nullable ViewT> viewGetter,
            boolean optional) {
        super(
                (optional ? "OptionalStripView/" : "StripView/")
                        + (incognito ? "incognito/" : "regular/")
                        + viewName);
        mManagerSupplier = managerSupplier;
        mIncognito = incognito;
        mViewName = viewName;
        mViewGetter = viewGetter;
        mOptional = optional;
    }

    @Override
    public @Nullable ConditionWithResult<ViewT> createEnterCondition() {
        return mOptional ? null : present();
    }

    @Override
    public @Nullable Condition createExitCondition() {
        // expectRemoved() might set this afterwards.
        return null;
    }

    /** Creates a Condition fulfilled when the view is clickable. */
    public ConditionWithResult<ViewT> present() {
        return new StripViewCondition<>(mManagerSupplier, mIncognito, mViewName, mViewGetter);
    }

    /** Waits for an optional view to be clickable and returns it. */
    public ViewT checkPresent() {
        assert mOptional : mViewName + " is not optional, use value()";
        ConditionWithResult<ViewT> present = present();
        mOwner.noopTo().waitFor(present);
        return present.getFromPast();
    }

    /** Starts a Trip clicking the center of the view. An optional view is checked first. */
    public TripBuilder clickTo() {
        if (mOptional) {
            checkPresent();
        }
        return mOwner.runOnUiThreadTo(
                () -> {
                    StripLayoutHelperManager manager = assumeNonNull(mManagerSupplier.get());
                    // Read the position right before the click.
                    StripLayoutView view = mViewGetter.apply(manager);
                    assert view != null : mViewName + " is gone";
                    manager.simulateClick(
                            view.getDrawX() + view.getWidth() / 2,
                            view.getDrawY() + view.getHeight() / 2,
                            /* buttons= */ 0,
                            /* modifiers= */ 0);
                });
    }

    /**
     * Expect the view to be removed from the strip and the strip to be settled when exiting the
     * owner, e.g. after the close animation of a tab and the resize of the other tabs.
     */
    public void expectRemoved() {
        assert mExitCondition == null : "Already set an exit condition";
        replaceExitCondition(new RemovedCondition());
    }

    private class RemovedCondition extends Condition {
        private final Supplier<@Nullable StripLayoutHelperManager> mExitManagerSupplier;

        private RemovedCondition() {
            super(/* isRunOnUiThread= */ true);
            mExitManagerSupplier = dependOnSupplier(mManagerSupplier, "StripLayoutHelperManager");
        }

        @Override
        protected ConditionStatus checkWithSuppliers() {
            StripLayoutHelperManager manager = assumeNonNull(mExitManagerSupplier.get());
            StripLayoutView view = mViewGetter.apply(manager);
            if (view != null) {
                return awaiting("Still on the strip, isDying=%b", view.isDying());
            }
            String notSettled = StripViewCondition.whyNotSettled(manager, mIncognito);
            return notSettled == null ? fulfilled() : awaiting(notSettled);
        }

        @Override
        public String buildDescription() {
            return String.format(
                    "%s removed from the %s tab strip and strip settled",
                    mViewName, mIncognito ? "incognito" : "regular");
        }
    }
}
