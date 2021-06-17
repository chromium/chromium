// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.ui.android.webid;

import android.content.Context;
import android.view.LayoutInflater;
import android.view.View;
import android.view.ViewGroup;
import android.widget.LinearLayout;

import androidx.recyclerview.widget.LinearLayoutManager;
import androidx.recyclerview.widget.RecyclerView;

import org.chromium.chrome.browser.profiles.Profile;
import org.chromium.chrome.browser.ui.android.webid.data.Account;
import org.chromium.components.browser_ui.bottomsheet.BottomSheetController;
import org.chromium.components.favicon.LargeIconBridge;
import org.chromium.ui.modelutil.MVCListAdapter.ModelList;
import org.chromium.ui.modelutil.SimpleRecyclerViewAdapter;

import java.util.List;

/**
 * Creates the AccountSelection component. AccountSelection uses a bottom sheet to let the
 * user select an account.
 */
public class AccountSelectionCoordinator implements AccountSelectionComponent {
    private Context mContext;
    private BottomSheetController mBottomSheetController;
    private AccountSelectionBottomSheetContent mBottomSheetContent;
    private AccountSelectionMediator mMediator;
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
        mBottomSheetContent = new AccountSelectionBottomSheetContent(
                contentView, mSheetItemListView::computeVerticalScrollOffset);

        // TODO(majidvp): This is currently using the regular profile which is incorrect if the
        // API is being used in an incognito tabs. We should instead use the profile associated
        // with the RP's web contents. https://crbug.com/1199088
        mMediator = new AccountSelectionMediator(delegate, sheetItems, mBottomSheetController,
                mBottomSheetContent, new LargeIconBridge(Profile.getLastUsedRegularProfile()),
                context.getResources().getDimensionPixelSize(
                        R.dimen.account_selection_favicon_size));
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
        adapter.registerType(AccountSelectionProperties.ItemType.ACCOUNT,
                AccountSelectionCoordinator::buildAccountView,
                AccountSelectionViewBinder::bindAccountView);
        adapter.registerType(AccountSelectionProperties.ItemType.CONTINUE_BUTTON,
                AccountSelectionCoordinator::buildContinueButtonView,
                AccountSelectionViewBinder::bindContinueButtonView);
        sheetItemListView.setAdapter(adapter);

        return contentView;
    }

    static View buildHeaderView(ViewGroup parent) {
        return LayoutInflater.from(parent.getContext())
                .inflate(R.layout.account_selection_header_item, parent, false);
    }

    static View buildAccountView(ViewGroup parent) {
        return LayoutInflater.from(parent.getContext())
                .inflate(R.layout.account_selection_account_item, parent, false);
    }

    static View buildContinueButtonView(ViewGroup parent) {
        return LayoutInflater.from(parent.getContext())
                .inflate(R.layout.account_selection_continue_button, parent, false);
    }

    @Override
    public void showAccounts(String url, List<Account> accounts) {
        mMediator.showAccounts(url, accounts);
    }
}
