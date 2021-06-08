// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.ui.android.webid;

import android.content.Context;
import android.content.res.Resources;
import android.view.LayoutInflater;
import android.view.View;
import android.view.ViewGroup;
import android.widget.LinearLayout;

import androidx.annotation.Px;
import androidx.recyclerview.widget.LinearLayoutManager;
import androidx.recyclerview.widget.RecyclerView;

import org.chromium.chrome.browser.ui.android.webid.data.Account;
import org.chromium.components.browser_ui.bottomsheet.BottomSheetController;
import org.chromium.ui.modelutil.MVCListAdapter.ModelList;
import org.chromium.ui.modelutil.SimpleRecyclerViewAdapter;

import java.util.List;

/**
 * Creates the AccountSelection component. AccountSelection uses a bottom sheet to let the
 * user select an account.
 */
public class AccountSelectionCoordinator implements AccountSelectionComponent {
    private AccountSelectionMediator mMediator;

    private Context mContext;
    private BottomSheetController mBottomSheetController;
    private AccountSelectionBottomSheetContent mBottomSheetContent;
    private RecyclerView mSheetItemListView;

    @Override
    public void initialize(Context context, BottomSheetController sheetController,
            AccountSelectionComponent.Delegate delegate) {
        mBottomSheetController = sheetController;
        mContext = context;

        // Construct view and its related adaptor to be displayed in the bottom sheet.
        ModelList sheetItems = new ModelList();
        View contentView = setupContentView(context, sheetItems);
        mSheetItemListView = contentView.findViewById(R.id.sheet_item_list);

        // Setup the bottom sheet content view.
        mBottomSheetContent = new AccountSelectionBottomSheetContent(contentView,
                mSheetItemListView::computeVerticalScrollOffset, this::computeHalfHeightRatio);

        mMediator = new AccountSelectionMediator(
                delegate, sheetItems, mBottomSheetController, mBottomSheetContent);
    }

    static View setupContentView(Context context, ModelList sheetItems) {
        View contentView = (LinearLayout) LayoutInflater.from(context).inflate(
                R.layout.account_selection_sheet, null);
        RecyclerView sheetItemListView = contentView.findViewById(R.id.sheet_item_list);
        sheetItemListView.setLayoutManager(new LinearLayoutManager(
                sheetItemListView.getContext(), LinearLayoutManager.VERTICAL, false));
        sheetItemListView.setItemAnimator(null);

        // Setup the recycler view to be updated as we update the sheet items.
        SimpleRecyclerViewAdapter adapter = new SimpleRecyclerViewAdapter(sheetItems);
        adapter.registerType(AccountSelectionProperties.ItemType.HEADER,
                AccountSelectionCoordinator::buildHeaderView,
                AccountSelectionViewBinder::bindHeaderView);
        sheetItemListView.setAdapter(adapter);

        return contentView;
    }

    static View buildHeaderView(ViewGroup parent) {
        return LayoutInflater.from(parent.getContext())
                .inflate(R.layout.account_selection_header_item, parent, false);
    }

    @Override
    public void showAccounts(String url, List<Account> accounts) {
        mMediator.showAccounts(url, accounts);
    }

    public float computeHalfHeightRatio() {
        return Math.min(getDesiredSheetHeight(), mBottomSheetController.getContainerHeight())
                / (float) mBottomSheetController.getContainerHeight();
    }

    private @Px int getDesiredSheetHeight() {
        Resources resources = mContext.getResources();
        @Px
        int totalHeight = resources.getDimensionPixelSize(
                R.dimen.account_selection_sheet_height_single_account);

        return totalHeight;
    }
}
