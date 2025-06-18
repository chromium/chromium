// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.components.payments.secure_payment_confirmation;

import android.content.Context;
import android.text.method.LinkMovementMethod;
import android.view.LayoutInflater;
import android.view.View;
import android.view.ViewGroup;
import android.widget.Button;
import android.widget.ImageView;
import android.widget.LinearLayout;
import android.widget.RelativeLayout;
import android.widget.ScrollView;
import android.widget.TextView;

import androidx.recyclerview.widget.LinearLayoutManager;
import androidx.recyclerview.widget.RecyclerView;

import org.chromium.build.annotations.NullMarked;
import org.chromium.components.payments.R;
import org.chromium.components.payments.ui.ItemDividerBase;
import org.chromium.ui.modelutil.MVCListAdapter.ModelList;
import org.chromium.ui.modelutil.SimpleRecyclerViewAdapter;
import org.chromium.ui.widget.TextViewWithClickableSpans;

/**
 * The view of the SecurePaymentConfirmation UI. This view does not have a peeked or half-open
 * state. It has a fixed height, which is the height of the visible content area. It shows the
 * payment details and provides the option to continue with the payment or to cancel.
 */
@NullMarked
/* package */ class SecurePaymentConfirmationView {
    /**
     * Creates an item View to be used with the RecyclerView. These views will be contained within
     * the {@link #mItemList}.
     */
    public static View createItemView(ViewGroup parent) {
        return LayoutInflater.from(parent.getContext())
                .inflate(R.layout.payments_item, parent, false);
    }

    /* package */ final RelativeLayout mContentView;
    /* package */ final ScrollView mScrollView;
    /* package */ final ImageView mHeaderIllustration;
    /* package */ final LinearLayout mHeaderLogosRow;
    /* package */ final View mHeaderLogosDivider;
    /* package */ final ImageView mHeaderLogoPrimary;
    /* package */ final ImageView mHeaderLogoSecondary;
    /* package */ final TextView mTitle;
    /* package */ final RecyclerView mItemList;
    /* package */ final TextViewWithClickableSpans mOptOutText;
    /* package */ final TextViewWithClickableSpans mFootnote;
    /* package */ final Button mContinueButton;

    /* package */ SecurePaymentConfirmationView(Context context) {
        mContentView =
                (RelativeLayout)
                        LayoutInflater.from(context)
                                .inflate(R.layout.secure_payment_confirmation, null);
        mScrollView = mContentView.findViewById(R.id.scroll_view);
        mHeaderIllustration =
                mContentView.findViewById(R.id.secure_payment_confirmation_header_illustration);
        mHeaderLogosRow = mContentView.findViewById(R.id.header_logos_row);
        mHeaderLogosDivider = mContentView.findViewById(R.id.header_logos_divider);
        mHeaderLogoPrimary = mContentView.findViewById(R.id.header_logo_primary);
        mHeaderLogoSecondary = mContentView.findViewById(R.id.header_logo_secondary);
        mTitle = mContentView.findViewById(R.id.secure_payment_confirmation_title);
        mItemList = mContentView.findViewById(R.id.item_list);
        mOptOutText =
                mContentView.findViewById(R.id.secure_payment_confirmation_nocredmatch_opt_out);
        mFootnote = mContentView.findViewById(R.id.secure_payment_confirmation_footnote);
        mContinueButton = mContentView.findViewById(R.id.continue_button);

        mItemList.setLayoutManager(
                new LinearLayoutManager(
                        context, LinearLayoutManager.VERTICAL, /* reverseLayout= */ false));
        mItemList.addItemDecoration(new ItemDividerBase(context));
        mItemList.setAdapter(new SimpleRecyclerViewAdapter(new ModelList()));
        mOptOutText.setMovementMethod(LinkMovementMethod.getInstance());
        mFootnote.setMovementMethod(LinkMovementMethod.getInstance());
    }
}
