// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.test.transit.hub;

import android.view.View;

import androidx.recyclerview.widget.RecyclerView;
import androidx.recyclerview.widget.RecyclerView.LayoutManager;

import org.chromium.base.supplier.Supplier;
import org.chromium.base.test.transit.Condition;
import org.chromium.base.test.transit.ConditionStatus;

/** Check that the card at this position in the tab switcher exists in the tab model. */
public class CardAtPositionCondition extends Condition {
    private final int mExpectedCardIndex;
    private final Supplier<RecyclerView> mRecyclerViewElementSupplier;
    private final Supplier<View> mCardViewElementSupplier;

    /**
     * Constructor.
     *
     * @param expectedCardIndex The expected index of the card in the tab switcher.
     * @param recyclerViewSupplier The supplier for the View representing the tab switcher recycler
     *     view.
     * @param cardViewSupplier The supplier for the View representing the card for the tab group in
     *     the tab switcher.
     */
    public CardAtPositionCondition(
            int expectedCardIndex,
            Supplier<RecyclerView> recyclerViewSupplier,
            Supplier<View> cardViewSupplier) {
        super(/* isRunOnUiThread= */ false);
        mExpectedCardIndex = expectedCardIndex;
        mRecyclerViewElementSupplier = dependOnSupplier(recyclerViewSupplier, "RecyclerView");
        mCardViewElementSupplier = dependOnSupplier(cardViewSupplier, "CardView");
    }

    @Override
    protected ConditionStatus checkWithSuppliers() {
        RecyclerView recyclerView = mRecyclerViewElementSupplier.get();

        LayoutManager layoutManager = recyclerView.getLayoutManager();
        if (layoutManager == null) {
            return notFulfilled("RecyclerView LayoutManager not available");
        }

        View cardAtPosition = layoutManager.findViewByPosition(mExpectedCardIndex);
        if (cardAtPosition == null) {
            return notFulfilled("No card at position %d", mExpectedCardIndex);
        }

        View cardView = mCardViewElementSupplier.get();
        if (cardAtPosition != cardView) {
            return notFulfilled(
                    "Expected card at position %d, but was not seen", mExpectedCardIndex);
        }

        return fulfilled();
    }

    @Override
    public String buildDescription() {
        return String.format("Card exists at index %s", mExpectedCardIndex);
    }
}
