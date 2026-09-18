// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.toolbar.account_menu;

import static org.chromium.build.NullUtil.assumeNonNull;

import android.app.Activity;
import android.content.Context;
import android.content.Intent;

import androidx.annotation.IntDef;

import org.chromium.base.metrics.RecordHistogram;
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
import org.chromium.chrome.browser.signin.services.SigninMetricsUtils;
import org.chromium.chrome.browser.sync.SyncServiceFactory;
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
import org.chromium.components.signin.metrics.SigninPromoAction;
import org.chromium.components.sync.SyncService;
import org.chromium.components.sync.UserActionableError;
import org.chromium.ui.base.WindowAndroid;
import org.chromium.ui.modelutil.MVCListAdapter.ListItem;
import org.chromium.ui.modelutil.MVCListAdapter.ModelList;
import org.chromium.ui.modelutil.PropertyModel;

import java.lang.annotation.Retention;
import java.lang.annotation.RetentionPolicy;

/** Mediator managing business logic and menu items for the Account Menu popup. */
@NullMarked
public class AccountMenuMediator
        implements SigninManager.SignInStateObserver, ProfileDataCache.Observer {
    /**
     * Events recorded for the account menu: the menu being shown, along with the sign-in state it
     * was shown in, and the item the user selected in it. These values are persisted to logs.
     * Entries should not be renumbered and numeric values should never be reused.
     */
    // LINT.IfChange(AccountMenuEvent)
    @IntDef({
        Event.SHOWN_SIGNED_OUT,
        Event.SHOWN_SIGNED_OUT_SIGNIN_DISABLED,
        Event.SHOWN_SIGNED_IN,
        Event.SHOWN_SIGNED_IN_WITH_ERROR,
        Event.SIGNIN_PROMO_CLICKED,
        Event.PASSWORDS_AND_AUTOFILL_CLICKED,
        Event.NEW_INCOGNITO_TAB_CLICKED,
        Event.NEW_INCOGNITO_WINDOW_CLICKED,
        Event.MANAGE_GOOGLE_ACCOUNT_CLICKED,
        Event.ACCOUNT_SETTINGS_CLICKED
    })
    @Retention(RetentionPolicy.SOURCE)
    @interface Event {
        /** The menu was shown, the user is signed out and the sign-in promo card is shown. */
        int SHOWN_SIGNED_OUT = 0;

        /**
         * The menu was shown, the user is signed out and sign-in is disallowed, so no promo card is
         * shown.
         */
        int SHOWN_SIGNED_OUT_SIGNIN_DISABLED = 1;

        /** The menu was shown, the user is signed in and the identity card is shown. */
        int SHOWN_SIGNED_IN = 2;

        /** The menu was shown, the user is signed in but has an error requiring their attention. */
        int SHOWN_SIGNED_IN_WITH_ERROR = 3;

        /** The user selected the sign-in button on the promo card. */
        int SIGNIN_PROMO_CLICKED = 4;

        /** The user selected the "Passwords and autofill" item. */
        int PASSWORDS_AND_AUTOFILL_CLICKED = 5;

        /** The user selected the "New Incognito tab" item. */
        int NEW_INCOGNITO_TAB_CLICKED = 6;

        /** The user selected the "New Incognito window" item. */
        int NEW_INCOGNITO_WINDOW_CLICKED = 7;

        /** The user selected the "Manage your Google Account" item. */
        int MANAGE_GOOGLE_ACCOUNT_CLICKED = 8;

        /** The user selected the account settings item, shown to signed-in users. */
        int ACCOUNT_SETTINGS_CLICKED = 9;

        int COUNT = 10;
    }

    // LINT.ThenChange(//tools/metrics/histograms/metadata/signin/enums.xml:AccountMenuEvent)

    private final Context mContext;
    private final Profile mProfile;
    private final WindowAndroid mWindowAndroid;
    private final ModelList mModelList;
    private final @Nullable BottomSheetSigninAndHistorySyncCoordinator mSigninCoordinator;
    private final SigninAndHistorySyncActivityLauncher mSigninLauncher;
    private final Runnable mDismissCallback;
    private @Nullable ProfileDataCache mProfileDataCache;
    private @Nullable SigninManager mSigninManager;

    public AccountMenuMediator(
            Context context,
            Profile profile,
            WindowAndroid windowAndroid,
            ModelList modelList,
            @Nullable BottomSheetSigninAndHistorySyncCoordinator signinCoordinator,
            SigninAndHistorySyncActivityLauncher signinLauncher,
            Runnable dismissCallback) {
        mContext = context;
        mProfile = profile;
        mWindowAndroid = windowAndroid;
        mModelList = modelList;
        mSigninCoordinator = signinCoordinator;
        mSigninLauncher = signinLauncher;
        mDismissCallback = dismissCallback;

        if (!mProfile.isOffTheRecord()) {
            mSigninManager = IdentityServicesProvider.get().getSigninManager(mProfile);
            if (mSigninManager != null) {
                mSigninManager.addSignInStateObserver(this);
            }
        }

        updateMenuItems(/* recordShownMetrics= */ false);
    }

    // SigninManager.SignInStateObserver implementation.
    @Override
    public void onSignInAllowedChanged() {
        updateMenuItems(/* recordShownMetrics= */ false);
    }

    @Override
    public void onSignedIn() {
        updateMenuItems(/* recordShownMetrics= */ false);
    }

    @Override
    public void onSignedOut() {
        updateMenuItems(/* recordShownMetrics= */ false);
    }

    /**
     * Populates the menu action items.
     *
     * @param recordShownMetrics Whether to record that the menu became visible to the user. Only
     *     set by the coordinator when it is about to show the popup.
     */
    public void updateMenuItems(boolean recordShownMetrics) {
        mModelList.clear();

        maybeAddHeader(recordShownMetrics);

        mModelList.add(
                new ListItem(
                        ItemType.MENU_ITEM,
                        MenuItemProperties.createModel(
                                R.string.menu_passwords_and_autofill,
                                R.drawable.ic_password_manager_24dp,
                                v -> {
                                    recordEvent(Event.PASSWORDS_AND_AUTOFILL_CLICKED);
                                    mDismissCallback.run();
                                    openAutofillSettings();
                                })));

        maybeAddManageGoogleAccount();

        IdentityManager identityManager =
                assumeNonNull(IdentityServicesProvider.get().getIdentityManager(mProfile));
        if (identityManager.hasPrimaryAccount()) {
            SyncService syncService = assumeNonNull(SyncServiceFactory.getForProfile(mProfile));
            boolean showIconBadge =
                    syncService.getUserActionableError() != UserActionableError.NONE;
            mModelList.add(
                    new ListItem(
                            ItemType.MENU_ITEM,
                            MenuItemProperties.createModel(
                                    R.string.profile_menu_account_settings_button,
                                    R.drawable.settings_cog,
                                    v -> {
                                        recordEvent(Event.ACCOUNT_SETTINGS_CLICKED);
                                        mDismissCallback.run();
                                        openAccountSettings();
                                    },
                                    showIconBadge)));
        }

        if (IncognitoUtils.isIncognitoModeEnabled(mProfile)) {
            mModelList.add(new ListItem(ItemType.DIVIDER, new PropertyModel()));
            boolean openAsWindow = IncognitoUtils.shouldOpenIncognitoAsWindow();
            int titleRes =
                    openAsWindow
                            ? R.string.menu_new_incognito_window
                            : R.string.menu_new_incognito_tab;
            mModelList.add(
                    new ListItem(
                            ItemType.MENU_ITEM,
                            MenuItemProperties.createModel(
                                    titleRes,
                                    R.drawable.ic_incognito_24dp,
                                    v -> {
                                        recordEvent(
                                                openAsWindow
                                                        ? Event.NEW_INCOGNITO_WINDOW_CLICKED
                                                        : Event.NEW_INCOGNITO_TAB_CLICKED);
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

    private void maybeAddHeader(boolean recordShownMetrics) {
        IdentityManager identityManager =
                IdentityServicesProvider.get().getIdentityManager(mProfile);
        AccountInfo accountInfo =
                identityManager == null ? null : identityManager.getPrimaryAccountInfo();

        final @Event int shownEvent;
        if (identityManager != null && accountInfo != null) {
            addIdentityCard(identityManager, accountInfo);
            SyncService syncService = SyncServiceFactory.getForProfile(mProfile);
            boolean hasIdentityError =
                    syncService != null
                            && syncService.getUserActionableError() != UserActionableError.NONE;
            shownEvent =
                    hasIdentityError ? Event.SHOWN_SIGNED_IN_WITH_ERROR : Event.SHOWN_SIGNED_IN;
        } else if (mSigninManager != null && mSigninManager.isSigninAllowed()) {
            addPromoCard();
            shownEvent = Event.SHOWN_SIGNED_OUT;
            if (recordShownMetrics) {
                // The promo card is part of the menu, so sign-in was offered to the user.
                SigninMetricsUtils.logSigninOffered(
                        SigninPromoAction.NO_SIGNIN_PROMO,
                        SigninAccessPoint.ACCOUNT_MENU_SIGNED_OUT_STATE);
            }
        } else {
            shownEvent = Event.SHOWN_SIGNED_OUT_SIGNIN_DISABLED;
        }

        if (recordShownMetrics) {
            recordEvent(shownEvent);
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
                                    recordEvent(Event.SIGNIN_PROMO_CLICKED);
                                    mDismissCallback.run();
                                    startSigninFlow();
                                })));
    }

    private void startSigninFlow() {
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
            assumeNonNull(mSigninCoordinator).startSigninFlow(config);
        } else {
            @Nullable Intent intent =
                    mSigninLauncher.createBottomSheetSigninIntentOrShowError(
                            mContext,
                            originalProfile,
                            config,
                            SigninAccessPoint.ACCOUNT_MENU_SIGNED_OUT_STATE);
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

    private void openAccountSettings() {
        SettingsNavigation settingsNavigation =
                SettingsNavigationFactory.createSettingsNavigation();
        settingsNavigation.startSettings(mContext, SettingsFragment.MAIN);
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

    private void maybeAddManageGoogleAccount() {
        if (mProfile.isOffTheRecord()) {
            return;
        }
        IdentityManager identityManager =
                IdentityServicesProvider.get().getIdentityManager(mProfile);
        if (identityManager != null && identityManager.hasPrimaryAccount()) {
            mModelList.add(
                    new ListItem(
                            ItemType.MENU_ITEM,
                            MenuItemProperties.createModel(
                                    R.string.manage_your_google_account,
                                    R.drawable.ic_google_services_24dp,
                                    v -> {
                                        recordEvent(Event.MANAGE_GOOGLE_ACCOUNT_CLICKED);
                                        mDismissCallback.run();
                                        mSigninLauncher.openManageGoogleAccount(mContext);
                                    })));
        }
    }

    /** Records an account menu event: the menu being shown, or an item being selected in it. */
    private static void recordEvent(@Event int event) {
        RecordHistogram.recordEnumeratedHistogram("Signin.AccountMenu.Event", event, Event.COUNT);
    }
}
