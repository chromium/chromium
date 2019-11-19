// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.test.util.browser;

import android.support.v7.widget.RecyclerView;
import android.view.View;

import org.chromium.base.test.util.CallbackHelper;
import org.chromium.content_public.browser.test.util.Criteria;
import org.chromium.content_public.browser.test.util.CriteriaHelper;
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
        CriteriaHelper.pollUiThread(new Criteria() {
            @Override
            public boolean isSatisfied() {
                RecyclerView.ViewHolder viewHolder =
                        recyclerView.findViewHolderForAdapterPosition(position);

                if (viewHolder == null) {
                    updateFailureReason("Cannot find view holder for position " + position + ".");
                    return false;
                }

                if (viewHolder.itemView.getParent() == null) {
                    updateFailureReason("The view is not attached for position " + position + ".");
                    return false;
                }

                if (!viewHolder.itemView.isShown()) {
                    updateFailureReason("The view is not visible for position " + position + ".");
                    return false;
                }

                return true;
            }
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
        CriteriaHelper.pollUiThread(new Criteria() {
            @Override
            public boolean isSatisfied() {
                if (recyclerView.isComputingLayout()) {
                    updateFailureReason("The recycler view is computing layout.");
                    return false;
                }

                if (recyclerView.isLayoutFrozen()) {
                    updateFailureReason("The recycler view layout is frozen.");
                    return false;
                }

                if (recyclerView.isAnimating()) {
                    updateFailureReason("The recycler view is animating.");
                    return false;
                }

                if (recyclerView.isDirty()) {
                    updateFailureReason("The recycler view is dirty.");
                    return false;
                }

                if (recyclerView.isLayoutRequested()) {
                    updateFailureReason("The recycler view has layout requested.");
                    return false;
                }

                return true;
            }
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

        CriteriaHelper.pollUiThread(new Criteria(){
            @Override
            public boolean isSatisfied() {
                // Wait until we can scroll no further.
                // A positive parameter checks scrolling down, a negative one scrolling up.
                return !recyclerView.canScrollVertically(1);
            }
        });
    }
}
