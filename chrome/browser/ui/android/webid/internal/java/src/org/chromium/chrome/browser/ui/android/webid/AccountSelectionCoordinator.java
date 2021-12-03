// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.ui.android.webid;

import android.content.Context;
import android.view.LayoutInflater;
import android.view.View;
import android.view.ViewGroup;
import android.widget.LinearLayout;

import androidx.annotation.Px;
import androidx.recyclerview.widget.LinearLayoutManager;
import androidx.recyclerview.widget.RecyclerView;

import org.chromium.chrome.browser.profiles.Profile;
import org.chromium.chrome.browser.ui.android.webid.data.Account;
import org.chromium.chrome.browser.ui.android.webid.data.ClientIdMetadata;
import org.chromium.chrome.browser.ui.android.webid.data.IdentityProviderMetadata;
import org.chromium.components.browser_ui.bottomsheet.BottomSheetController;
import org.chromium.components.browser_ui.util.ConversionUtils;
import org.chromium.components.browser_ui.util.GlobalDiscardableReferencePool;
import org.chromium.components.favicon.LargeIconBridge;
import org.chromium.components.image_fetcher.ImageFetcher;
import org.chromium.components.image_fetcher.ImageFetcherConfig;
import org.chromium.components.image_fetcher.ImageFetcherFactory;
import org.chromium.ui.modelutil.MVCListAdapter.ModelList;
import org.chromium.ui.modelutil.SimpleRecyclerViewAdapter;
import org.chromium.url.GURL;

import java.util.List;

/**
 * Creates the AccountSelection component. AccountSelection uses a bottom sheet to let the
 * user select an account.
 */
public class AccountSelectionCoordinator implements AccountSelectionComponent {
    private static final int MAX_IMAGE_CACHE_SIZE = 500 * ConversionUtils.BYTES_PER_KILOBYTE;

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
        Profile profile = Profile.getLastUsedRegularProfile();
        ImageFetcher imageFetcher = ImageFetcherFactory.createImageFetcher(
                ImageFetcherConfig.IN_MEMORY_ONLY, profile.getProfileKey(),
                GlobalDiscardableReferencePool.getReferencePool(), MAX_IMAGE_CACHE_SIZE);

        @Px
        int avatar_size =
                context.getResources().getDimensionPixelSize(R.dimen.list_item_start_icon_width);
        @Px
        int idp_icon_size = Math.round(avatar_size * 0.4f);
        mMediator = new AccountSelectionMediator(delegate, sheetItems, mBottomSheetController,
                mBottomSheetContent, imageFetcher, avatar_size, new LargeIconBridge(profile),
                idp_icon_size);
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
        adapter.registerType(AccountSelectionProperties.ItemType.AUTO_SIGN_IN_CANCEL_BUTTON,
                AccountSelectionCoordinator::buildAutoSignInCancelButtonView,
                AccountSelectionViewBinder::bindAutoSignInCancelButtonView);
        adapter.registerType(AccountSelectionProperties.ItemType.DATA_SHARING_CONSENT,
                AccountSelectionCoordinator::buildDataSharingConsentView,
                AccountSelectionViewBinder::bindDataSharingConsentView);
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

    static View buildDataSharingConsentView(ViewGroup parent) {
        return LayoutInflater.from(parent.getContext())
                .inflate(R.layout.account_selection_data_sharing_consent_item, parent, false);
    }

    static View buildContinueButtonView(ViewGroup parent) {
        return LayoutInflater.from(parent.getContext())
                .inflate(R.layout.account_selection_continue_button, parent, false);
    }

    static View buildAutoSignInCancelButtonView(ViewGroup parent) {
        return LayoutInflater.from(parent.getContext())
                .inflate(R.layout.auto_sign_in_cancel_button, parent, false);
    }

    @Override
    public void showAccounts(GURL rpUrl, GURL idpUrl, List<Account> accounts,
            IdentityProviderMetadata idpMetadata, ClientIdMetadata clientMetadata,
            boolean isAutoSignIn) {
        mMediator.showAccounts(rpUrl, idpUrl, accounts, idpMetadata, clientMetadata, isAutoSignIn);
    }

    @Override
    public void hideBottomSheet() {
        mMediator.hideBottomSheet();
    }
}
