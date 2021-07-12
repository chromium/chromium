// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.ui.android.webid;

import androidx.annotation.Px;
import androidx.annotation.VisibleForTesting;

import org.chromium.chrome.browser.ui.android.webid.AccountSelectionProperties.AccountProperties;
import org.chromium.chrome.browser.ui.android.webid.AccountSelectionProperties.ContinueButtonProperties;
import org.chromium.chrome.browser.ui.android.webid.AccountSelectionProperties.HeaderProperties;
import org.chromium.chrome.browser.ui.android.webid.AccountSelectionProperties.ItemType;
import org.chromium.chrome.browser.ui.android.webid.data.Account;
import org.chromium.components.browser_ui.bottomsheet.BottomSheetContent;
import org.chromium.components.browser_ui.bottomsheet.BottomSheetController;
import org.chromium.components.browser_ui.bottomsheet.BottomSheetController.StateChangeReason;
import org.chromium.components.browser_ui.bottomsheet.BottomSheetObserver;
import org.chromium.components.browser_ui.bottomsheet.EmptyBottomSheetObserver;
import org.chromium.components.favicon.LargeIconBridge;
import org.chromium.components.favicon.LargeIconBridge.LargeIconCallback;
import org.chromium.components.image_fetcher.ImageFetcher;
import org.chromium.components.url_formatter.SchemeDisplay;
import org.chromium.components.url_formatter.UrlFormatter;
import org.chromium.ui.modelutil.MVCListAdapter.ListItem;
import org.chromium.ui.modelutil.MVCListAdapter.ModelList;
import org.chromium.ui.modelutil.PropertyModel;
import org.chromium.url.GURL;

import java.util.List;

/**
 * Contains the logic for the AccountSelection component. It sets the state of the model and reacts
 * to events like clicks.
 */
class AccountSelectionMediator {
    private boolean mVisible;
    private final AccountSelectionComponent.Delegate mDelegate;
    private final ModelList mSheetItems;
    private final ImageFetcher mImageFetcher;
    private final LargeIconBridge mLargeIconBridge;
    private final @Px int mDesiredAvatarSize;
    private final @Px int mDesiredIconSize;

    private final BottomSheetController mBottomSheetController;
    private final BottomSheetContent mBottomSheetContent;
    private final BottomSheetObserver mBottomSheetObserver;

    AccountSelectionMediator(AccountSelectionComponent.Delegate delegate, ModelList sheetItems,
            BottomSheetController bottomSheetController, BottomSheetContent bottomSheetContent,
            ImageFetcher imageFetcher, @Px int desiredAvatarSize, LargeIconBridge largeIconBridge,
            @Px int desiredIconSize) {
        assert delegate != null;
        mVisible = false;
        mDelegate = delegate;
        mSheetItems = sheetItems;
        mImageFetcher = imageFetcher;
        mDesiredAvatarSize = desiredAvatarSize;
        mLargeIconBridge = largeIconBridge;
        mDesiredIconSize = desiredIconSize;
        mBottomSheetController = bottomSheetController;
        mBottomSheetContent = bottomSheetContent;

        mBottomSheetObserver = new EmptyBottomSheetObserver() {
            // TODO(majidvp): We should override #onSheetStateChanged() and react to HIDDEN state
            // since closed is a legacy fixture that can get out of sync with the state is some
            // situations. https://crbug.com/1215174
            @Override
            public void onSheetClosed(@BottomSheetController.StateChangeReason int reason) {
                super.onSheetClosed(reason);
                mBottomSheetController.removeObserver(mBottomSheetObserver);

                if (!mVisible) return;
                onDismissed(reason);
            }
        };
    }

    void showAccounts(String url, List<Account> accounts) {
        mSheetItems.clear();

        // We remove the HTTPS from URL since it is the only protocol that is
        // allowed with WebID.
        mSheetItems.add(new ListItem(ItemType.HEADER,
                new PropertyModel.Builder(HeaderProperties.ALL_KEYS)
                        .with(HeaderProperties.SINGLE_ACCOUNT, accounts.size() == 1)
                        .with(HeaderProperties.FORMATTED_URL,
                                UrlFormatter.formatUrlForSecurityDisplay(
                                        url, SchemeDisplay.OMIT_HTTP_AND_HTTPS))
                        .build()));

        for (Account account : accounts) {
            final PropertyModel model = createAccountItem(account);
            mSheetItems.add(new ListItem(ItemType.ACCOUNT, model));
            requestIconOrFallbackImage(model);
            requestAvatarImage(model);
            // If there is only a single account we need to show the continue button.
            if (accounts.size() == 1) {
                final PropertyModel continueBtnModel = createContinueBtnItem(account);
                mSheetItems.add(new ListItem(ItemType.CONTINUE_BUTTON, continueBtnModel));
            }
        }

        showContent();
    }

    /**
     * Requests to show the bottom sheet. If it is not possible to immediately show the content
     * (e.g., higher priority content is being shown) it removes the request from the bottom sheet
     * controller queue and notifies the delegate of the dismissal.
     */
    @VisibleForTesting
    void showContent() {
        if (mBottomSheetController.requestShowContent(mBottomSheetContent, true)) {
            mVisible = true;
            mBottomSheetController.addObserver(mBottomSheetObserver);
        } else {
            onDismissed(BottomSheetController.StateChangeReason.NONE);
        }
    }

    /**
     * Requests to hide the bottom sheet.
     */
    void hideContent() {
        mVisible = false;
        mBottomSheetController.hideContent(mBottomSheetContent, true);
    }

    private void requestAvatarImage(PropertyModel accountModel) {
        Account account = accountModel.get(AccountProperties.ACCOUNT);
        final String name = account.getName();
        final String avatarURL = account.getPictureUrl().getSpec();

        if (!avatarURL.isEmpty()) {
            ImageFetcher.Params params = ImageFetcher.Params.create(avatarURL,
                    ImageFetcher.WEB_ID_ACCOUNT_SELECTION_UMA_CLIENT_NAME, mDesiredAvatarSize,
                    mDesiredAvatarSize);

            mImageFetcher.fetchImage(params, bitmap -> {
                accountModel.set(AccountProperties.AVATAR,
                        new AccountProperties.Avatar(name, bitmap, mDesiredAvatarSize));
            });
        } else {
            accountModel.set(AccountProperties.AVATAR,
                    new AccountProperties.Avatar(name, null, mDesiredAvatarSize));
        }
    }

    private void requestIconOrFallbackImage(PropertyModel accountModel) {
        Account account = accountModel.get(AccountProperties.ACCOUNT);
        final GURL iconOrigin = account.getOriginUrl();
        final LargeIconCallback setIcon = (icon, fallbackColor, hasDefaultColor, type) -> {
            accountModel.set(AccountProperties.FAVICON_OR_FALLBACK,
                    new AccountProperties.FaviconOrFallback(
                            iconOrigin, icon, fallbackColor, mDesiredIconSize));
        };
        mLargeIconBridge.getLargeIconForUrl(iconOrigin, mDesiredIconSize, setIcon);
    }

    boolean isVisible() {
        return mVisible;
    }

    void onAccountSelected(Account account) {
        hideContent();
        mDelegate.onAccountSelected(account);
    }

    void onDismissed(@StateChangeReason int reason) {
        hideContent();
        mDelegate.onDismissed();
    }

    private PropertyModel createAccountItem(Account account) {
        return new PropertyModel.Builder(AccountProperties.ALL_KEYS)
                .with(AccountProperties.ACCOUNT, account)
                .with(AccountProperties.ON_CLICK_LISTENER, this::onAccountSelected)
                .build();
    }

    private PropertyModel createContinueBtnItem(Account account) {
        return new PropertyModel.Builder(ContinueButtonProperties.ALL_KEYS)
                .with(ContinueButtonProperties.ACCOUNT, account)
                .with(ContinueButtonProperties.ON_CLICK_LISTENER, this::onAccountSelected)
                .build();
    }
}
