// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.components.browser_ui.widget;

import android.view.View;

import androidx.recyclerview.widget.RecyclerView;

import org.hamcrest.Matchers;

import org.chromium.base.test.util.CallbackHelper;
import org.chromium.base.test.util.Criteria;
import org.chromium.base.test.util.CriteriaHelper;
import org.chromium.content_public.browser.test.util.TestThreadUtils;

import java.util.concurrent.TimeoutException;

/**
 * Utilities for {@link RecyclerView}, to handle waiting for animation changes and other potential
 * flakiness sources.
 */
public final class RecyclerViewTestUtils {
    private RecyclerViewTestUtils() {}

    public static RecyclerView.ViewHolder waitForView(
            final RecyclerView recyclerView, final int position) {
        CriteriaHelper.pollUiThread(() -> {
            RecyclerView.ViewHolder viewHolder =
                    recyclerView.findViewHolderForAdapterPosition(position);

            Criteria.checkThat("Cannot find view holder for position " + position + ".", viewHolder,
                    Matchers.notNullValue());
            Criteria.checkThat("The view is not attached for position " + position + ".",
                    viewHolder.itemView.getParent(), Matchers.notNullValue());
            Criteria.checkThat("The view is not visible for position " + position + ".",
                    viewHolder.itemView.isShown(), Matchers.is(true));
        });

        waitForStableRecyclerView(recyclerView);

        return recyclerView.findViewHolderForAdapterPosition(position);
    }

    public static void waitForViewToDetach(final RecyclerView recyclerView, final View view)
            throws TimeoutException {
        final CallbackHelper callback = new CallbackHelper();

        recyclerView.addOnChildAttachStateChangeListener(
                new RecyclerView.OnChildAttachStateChangeListener() {
                    @Override
                    public void onChildViewAttachedToWindow(View view) {}

                    @Override
                    public void onChildViewDetachedFromWindow(View detachedView) {
                        if (detachedView == view) {
                            recyclerView.removeOnChildAttachStateChangeListener(this);
                            callback.notifyCalled();
                        }
                    }
                });
        callback.waitForCallback("The view did not detach.", 0);

        waitForStableRecyclerView(recyclerView);
    }

    public static void waitForStableRecyclerView(final RecyclerView recyclerView) {
        CriteriaHelper.pollUiThread(() -> {
            Criteria.checkThat("The recycler view is computing layout.",
                    recyclerView.isComputingLayout(), Matchers.is(false));
            Criteria.checkThat("The recycler view layout is frozen.", recyclerView.isLayoutFrozen(),
                    Matchers.is(false));
            Criteria.checkThat("The recycler view is animating.", recyclerView.isAnimating(),
                    Matchers.is(false));
            Criteria.checkThat(
                    "The recycler view is dirty.", recyclerView.isDirty(), Matchers.is(false));
            Criteria.checkThat("The recycler view has layout requested.",
                    recyclerView.isLayoutRequested(), Matchers.is(false));
        });
    }

    /**
     * Scrolls the {@link View} at the given adapter position into view and returns
     * its {@link RecyclerView.ViewHolder}.
     * @param recyclerView the {@link RecyclerView} to scroll.
     * @param position the adapter position for which to return the {@link RecyclerView.ViewHolder}.
     * @return the ViewHolder for the given {@code position}.
     */
    public static RecyclerView.ViewHolder scrollToView(RecyclerView recyclerView, int position) {
        TestThreadUtils.runOnUiThreadBlocking(() -> recyclerView.scrollToPosition(position));
        return waitForView(recyclerView, position);
    }

    /**
     * Scrolls the {@link RecyclerView} to the bottom.
     */
    public static void scrollToBottom(RecyclerView recyclerView) {
        TestThreadUtils.runOnUiThreadBlocking(() -> {
            // Scroll to bottom.
            recyclerView.scrollToPosition(recyclerView.getAdapter().getItemCount() - 1);
        });

        // Wait until we can scroll no further.
        // A positive parameter checks scrolling down, a negative one scrolling up.
        CriteriaHelper.pollUiThread(() -> !recyclerView.canScrollVertically(1));
    }
}
