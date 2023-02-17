// Copyright 2021 The Chromium Authors
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
import org.chromium.components.image_fetcher.ImageFetcher;
import org.chromium.components.image_fetcher.ImageFetcherConfig;
import org.chromium.components.image_fetcher.ImageFetcherFactory;
import org.chromium.ui.modelutil.MVCListAdapter.ModelList;
import org.chromium.ui.modelutil.PropertyModel;
import org.chromium.ui.modelutil.PropertyModelChangeProcessor;
import org.chromium.ui.modelutil.SimpleRecyclerViewAdapter;

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

    public AccountSelectionCoordinator(Context context, BottomSheetController sheetController,
            AccountSelectionComponent.Delegate delegate) {
        mBottomSheetController = sheetController;
        mContext = context;

        PropertyModel model =
                new PropertyModel.Builder(AccountSelectionProperties.ItemProperties.ALL_KEYS)
                        .build();
        // Construct view and its related adaptor to be displayed in the bottom sheet.
        ModelList sheetItems = new ModelList();
        View contentView = setupContentView(context, model, sheetItems);
        mSheetItemListView = contentView.findViewById(R.id.sheet_item_list);

        // Setup the bottom sheet content view.
        mBottomSheetContent = new AccountSelectionBottomSheetContent(
                contentView, mSheetItemListView::computeVerticalScrollOffset);

        // TODO(crbug.com/1199088): This is currently using the regular profile which is incorrect
        // if the API is being used in an incognito tabs. We should instead use the profile
        // associated with the RP's web contents.
        Profile profile = Profile.getLastUsedRegularProfile();
        ImageFetcher imageFetcher = ImageFetcherFactory.createImageFetcher(
                ImageFetcherConfig.IN_MEMORY_ONLY, profile.getProfileKey(),
                GlobalDiscardableReferencePool.getReferencePool(), MAX_IMAGE_CACHE_SIZE);

        @Px
        int avatarSize = context.getResources().getDimensionPixelSize(
                R.dimen.account_selection_account_avatar_size);
        mMediator = new AccountSelectionMediator(delegate, model, sheetItems,
                mBottomSheetController, mBottomSheetContent, imageFetcher, avatarSize);
    }

    static View setupContentView(Context context, PropertyModel model, ModelList sheetItems) {
        View contentView = (LinearLayout) LayoutInflater.from(context).inflate(
                R.layout.account_selection_sheet, null);

        PropertyModelChangeProcessor.create(
                model, contentView, AccountSelectionViewBinder::bindContentView);

        RecyclerView sheetItemListView = contentView.findViewById(R.id.sheet_item_list);
        sheetItemListView.setLayoutManager(new LinearLayoutManager(
                sheetItemListView.getContext(), LinearLayoutManager.VERTICAL, false));
        sheetItemListView.setItemAnimator(null);

        // Setup the recycler view to be updated as we update the sheet items.
        SimpleRecyclerViewAdapter adapter = new SimpleRecyclerViewAdapter(sheetItems);
        adapter.registerType(AccountSelectionProperties.ITEM_TYPE_ACCOUNT,
                AccountSelectionCoordinator::buildAccountView,
                AccountSelectionViewBinder::bindAccountView);
        sheetItemListView.setAdapter(adapter);

        return contentView;
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

    @Override
    public void showAccounts(String rpEtldPlusOne, String idpEtldPlusOne, List<Account> accounts,
            IdentityProviderMetadata idpMetadata, ClientIdMetadata clientMetadata,
            boolean isAutoReauthn) {
        mMediator.showAccounts(rpEtldPlusOne, idpEtldPlusOne, accounts, idpMetadata, clientMetadata,
                isAutoReauthn);
    }

    @Override
    public void close() {
        mMediator.close();
    }
}
