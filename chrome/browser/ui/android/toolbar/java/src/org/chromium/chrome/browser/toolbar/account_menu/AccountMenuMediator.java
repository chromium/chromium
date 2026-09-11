// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.toolbar.account_menu;

import android.app.Activity;
import android.content.Context;
import android.content.Intent;

import org.chromium.build.annotations.NullMarked;
import org.chromium.build.annotations.Nullable;
import org.chromium.chrome.browser.incognito.IncognitoUtils;
import org.chromium.chrome.browser.multiwindow.MultiInstanceManager.NewWindowAppSource;
import org.chromium.chrome.browser.multiwindow.MultiInstanceOrchestratorFactory;
import org.chromium.chrome.browser.profiles.Profile;
import org.chromium.chrome.browser.settings.SettingsNavigationFactory;
import org.chromium.chrome.browser.signin.services.DisplayableProfileData;
import org.chromium.chrome.browser.signin.services.IdentityServicesProvider;
import org.chromium.chrome.browser.signin.services.ProfileDataCache;
import org.chromium.chrome.browser.signin.services.SigninManager;
import org.chromium.chrome.browser.tabmodel.TabCreator;
import org.chromium.chrome.browser.tabmodel.TabCreatorUtil;
import org.chromium.chrome.browser.tabmodel.TabModelSelector;
import org.chromium.chrome.browser.tabmodel.TabModelSelectorSupplier;
import org.chromium.chrome.browser.toolbar.R;
import org.chromium.chrome.browser.toolbar.account_menu.AccountMenuProperties.IdentityCardProperties;
import org.chromium.chrome.browser.toolbar.account_menu.AccountMenuProperties.ItemType;
import org.chromium.chrome.browser.toolbar.account_menu.AccountMenuProperties.MenuItemProperties;
import org.chromium.chrome.browser.toolbar.account_menu.AccountMenuProperties.PromoCardProperties;
import org.chromium.chrome.browser.ui.signin.BottomSheetSigninAndHistorySyncConfig;
import org.chromium.chrome.browser.ui.signin.BottomSheetSigninAndHistorySyncConfig.NoAccountSigninMode;
import org.chromium.chrome.browser.ui.signin.BottomSheetSigninAndHistorySyncConfig.WithAccountSigninMode;
import org.chromium.chrome.browser.ui.signin.BottomSheetSigninAndHistorySyncCoordinator;
import org.chromium.chrome.browser.ui.signin.SigninAndHistorySyncActivityLauncher;
import org.chromium.chrome.browser.ui.signin.SigninSurveyController;
import org.chromium.chrome.browser.ui.signin.account_picker.AccountPickerBottomSheetStrings;
import org.chromium.chrome.browser.ui.signin.history_sync.HistorySyncConfig;
import org.chromium.components.browser_ui.settings.SettingsNavigation;
import org.chromium.components.browser_ui.settings.SettingsNavigation.SettingsFragment;
import org.chromium.components.signin.SigninFeatureMap;
import org.chromium.components.signin.base.AccountInfo;
import org.chromium.components.signin.identitymanager.IdentityManager;
import org.chromium.components.signin.metrics.SigninAccessPoint;
import org.chromium.ui.base.WindowAndroid;
import org.chromium.ui.modelutil.MVCListAdapter.ListItem;
import org.chromium.ui.modelutil.MVCListAdapter.ModelList;
import org.chromium.ui.modelutil.PropertyModel;

import java.util.function.Supplier;

/** Mediator managing business logic and menu items for the Account Menu popup. */
@NullMarked
public class AccountMenuMediator
        implements SigninManager.SignInStateObserver, ProfileDataCache.Observer {
    private final Context mContext;
    private final ModelList mModelList;
    private final WindowAndroid mWindowAndroid;
    private final Profile mProfile;
    private final Supplier<@Nullable BottomSheetSigninAndHistorySyncCoordinator>
            mSigninCoordinatorSupplier;
    private final SigninAndHistorySyncActivityLauncher mSigninLauncher;
    private final Runnable mDismissCallback;
    private @Nullable ProfileDataCache mProfileDataCache;
    private @Nullable SigninManager mSigninManager;

    public AccountMenuMediator(
            Context context,
            ModelList modelList,
            WindowAndroid windowAndroid,
            Profile profile,
            Supplier<@Nullable BottomSheetSigninAndHistorySyncCoordinator>
                    signinCoordinatorSupplier,
            SigninAndHistorySyncActivityLauncher signinLauncher,
            Runnable dismissCallback) {
        mContext = context;
        mModelList = modelList;
        mWindowAndroid = windowAndroid;
        mProfile = profile;
        mSigninCoordinatorSupplier = signinCoordinatorSupplier;
        mSigninLauncher = signinLauncher;
        mDismissCallback = dismissCallback;

        if (!mProfile.isOffTheRecord()) {
            mSigninManager = IdentityServicesProvider.get().getSigninManager(mProfile);
            if (mSigninManager != null) {
                mSigninManager.addSignInStateObserver(this);
            }
        }

        updateMenuItems();
    }

    // SigninManager.SignInStateObserver implementation.
    @Override
    public void onSignInAllowedChanged() {
        updateMenuItems();
    }

    @Override
    public void onSignedIn() {
        updateMenuItems();
    }

    @Override
    public void onSignedOut() {
        updateMenuItems();
    }

    /** Populates the menu action items. */
    public void updateMenuItems() {
        mModelList.clear();

        maybeAddHeader();

        mModelList.add(
                new ListItem(
                        ItemType.MENU_ITEM,
                        MenuItemProperties.createModel(
                                R.string.menu_passwords_and_autofill,
                                R.drawable.ic_password_manager_24dp,
                                v -> {
                                    mDismissCallback.run();
                                    openAutofillSettings();
                                })));

        if (IncognitoUtils.isIncognitoModeEnabled(mProfile)) {
            mModelList.add(new ListItem(ItemType.DIVIDER, new PropertyModel()));
            int titleRes =
                    IncognitoUtils.shouldOpenIncognitoAsWindow()
                            ? R.string.menu_new_incognito_window
                            : R.string.menu_new_incognito_tab;
            mModelList.add(
                    new ListItem(
                            ItemType.MENU_ITEM,
                            MenuItemProperties.createModel(
                                    titleRes,
                                    R.drawable.ic_incognito_24dp,
                                    v -> {
                                        mDismissCallback.run();
                                        openIncognito();
                                    })));
        }
    }

    /** Cleans up observers and resources. */
    public void destroy() {
        if (mSigninManager != null) {
            mSigninManager.removeSignInStateObserver(this);
            mSigninManager = null;
        }
        if (mProfileDataCache != null) {
            mProfileDataCache.removeObserver(this);
            mProfileDataCache = null;
        }
    }

    @Override
    public void onProfileDataUpdated(DisplayableProfileData profileData) {
        if (mProfile.isOffTheRecord()) {
            return;
        }
        IdentityManager identityManager =
                IdentityServicesProvider.get().getIdentityManager(mProfile);
        AccountInfo primaryAccount =
                identityManager != null ? identityManager.getPrimaryAccountInfo() : null;
        if (primaryAccount == null || !primaryAccount.getId().equals(profileData.getAccountId())) {
            return;
        }

        for (ListItem item : mModelList) {
            if (item.type == ItemType.IDENTITY_CARD) {
                item.model.set(IdentityCardProperties.PROFILE_DATA, profileData);
                break;
            }
        }
    }

    private void maybeAddHeader() {
        if (mProfile.isOffTheRecord()) {
            return;
        }

        IdentityManager identityManager =
                IdentityServicesProvider.get().getIdentityManager(mProfile);
        if (identityManager != null) {
            AccountInfo accountInfo = identityManager.getPrimaryAccountInfo();
            if (accountInfo != null) {
                addIdentityCard(identityManager, accountInfo);
                return;
            }
        }
        if (mSigninManager != null && mSigninManager.isSigninAllowed()) {
            addPromoCard();
        }
    }

    private void addIdentityCard(IdentityManager identityManager, AccountInfo accountInfo) {
        if (mProfileDataCache == null) {
            mProfileDataCache =
                    ProfileDataCache.createWithoutBadge(
                            mContext, identityManager, R.dimen.account_menu_avatar_size);
            mProfileDataCache.addObserver(this);
        }
        DisplayableProfileData profileData = mProfileDataCache.getById(accountInfo.getId());
        mModelList.add(
                new ListItem(
                        ItemType.IDENTITY_CARD, IdentityCardProperties.createModel(profileData)));
    }

    private void addPromoCard() {
        mModelList.add(
                new ListItem(
                        ItemType.PROMO_CARD,
                        PromoCardProperties.createModel(
                                v -> {
                                    mDismissCallback.run();
                                    startSigninFlow();
                                })));
    }

    private void startSigninFlow() {
        if (mProfile.isOffTheRecord()) {
            return;
        }

        if (mSigninManager == null || !mSigninManager.isSigninAllowed()) {
            return;
        }

        Profile originalProfile = mProfile.getOriginalProfile();
        String title = mContext.getString(R.string.signin_account_picker_bottom_sheet_title);
        String subtitle =
                mContext.getString(R.string.signin_account_picker_bottom_sheet_benefits_subtitle);
        AccountPickerBottomSheetStrings bottomSheetStrings =
                new AccountPickerBottomSheetStrings.Builder(title)
                        .setSubtitleString(subtitle)
                        .build();
        BottomSheetSigninAndHistorySyncConfig config =
                new BottomSheetSigninAndHistorySyncConfig.Builder(
                                bottomSheetStrings,
                                NoAccountSigninMode.BOTTOM_SHEET,
                                WithAccountSigninMode.DEFAULT_ACCOUNT_BOTTOM_SHEET,
                                HistorySyncConfig.OptInMode.OPTIONAL,
                                mContext.getString(R.string.history_sync_title),
                                mContext.getString(R.string.history_sync_subtitle))
                        .signinSurveyType(SigninSurveyController.SigninSurveyType.NTP_SIGNIN_BUTTON)
                        .build();

        if (SigninFeatureMap.getInstance().isActivitylessSigninAllEntryPointEnabled()) {
            BottomSheetSigninAndHistorySyncCoordinator signinCoordinator =
                    mSigninCoordinatorSupplier.get();
            if (signinCoordinator != null) {
                signinCoordinator.startSigninFlow(config);
            }
        } else {
            @Nullable Intent intent =
                    mSigninLauncher.createBottomSheetSigninIntentOrShowError(
                            mContext,
                            originalProfile,
                            config,
                            SigninAccessPoint.NTP_SIGNED_OUT_ICON);
            if (intent != null) {
                mContext.startActivity(intent);
            }
        }
    }

    private void openAutofillSettings() {
        SettingsNavigation settingsNavigation =
                SettingsNavigationFactory.createSettingsNavigation();
        settingsNavigation.startSettings(mContext, SettingsFragment.AUTOFILL_AND_PASSWORDS);
    }

    /** Opens a new Incognito window if supported, or a new Incognito tab otherwise. */
    private void openIncognito() {
        if (IncognitoUtils.shouldOpenIncognitoAsWindow()) {
            Activity activity = mWindowAndroid.getActivity().get();
            if (activity != null) {
                MultiInstanceOrchestratorFactory.getInstance()
                        .createNewWindow(
                                activity,
                                /* isIncognito= */ true,
                                /* additionalIntentExtras= */ null,
                                /* startActivityOptions= */ null,
                                NewWindowAppSource.ACCOUNT_MENU);
            }
        } else {
            TabModelSelector selector = TabModelSelectorSupplier.getValueOrNullFrom(mWindowAndroid);
            if (selector != null) {
                TabCreator incognitoTabCreator =
                        selector.getTabCreatorManager().getTabCreator(/* incognito= */ true);
                TabCreatorUtil.launchNtp(incognitoTabCreator);
            }
        }
    }
}
