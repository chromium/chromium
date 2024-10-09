// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.ui.android.webid;

import static androidx.browser.customtabs.CustomTabsIntent.COLOR_SCHEME_DARK;
import static androidx.browser.customtabs.CustomTabsIntent.COLOR_SCHEME_LIGHT;

import android.app.Activity;
import android.content.Context;
import android.content.Intent;
import android.net.Uri;
import android.provider.Browser;
import android.view.LayoutInflater;
import android.view.View;
import android.widget.LinearLayout;
import android.widget.TextView;

import androidx.annotation.Px;
import androidx.annotation.VisibleForTesting;
import androidx.browser.customtabs.CustomTabsIntent;
import androidx.recyclerview.widget.LinearLayoutManager;
import androidx.recyclerview.widget.RecyclerView;

import org.chromium.base.IntentUtils;
import org.chromium.base.metrics.RecordHistogram;
import org.chromium.blink.mojom.RpContext;
import org.chromium.blink.mojom.RpMode;
import org.chromium.chrome.browser.IntentHandler;
import org.chromium.chrome.browser.LaunchIntentDispatcher;
import org.chromium.chrome.browser.app.ChromeActivity;
import org.chromium.chrome.browser.tab.Tab;
import org.chromium.chrome.browser.ui.android.webid.data.Account;
import org.chromium.chrome.browser.ui.android.webid.data.IdentityCredentialTokenError;
import org.chromium.chrome.browser.ui.android.webid.data.IdentityProviderData;
import org.chromium.chrome.browser.ui.android.webid.data.IdentityProviderMetadata;
import org.chromium.chrome.browser.ui.signin.account_picker.AccountPickerItemDecoration;
import org.chromium.components.browser_ui.bottomsheet.BottomSheetController;
import org.chromium.components.browser_ui.util.ConversionUtils;
import org.chromium.components.browser_ui.util.GlobalDiscardableReferencePool;
import org.chromium.components.image_fetcher.ImageFetcher;
import org.chromium.components.image_fetcher.ImageFetcherConfig;
import org.chromium.components.image_fetcher.ImageFetcherFactory;
import org.chromium.content.webid.IdentityRequestDialogDismissReason;
import org.chromium.content.webid.IdentityRequestDialogLinkType;
import org.chromium.content_public.browser.WebContents;
import org.chromium.ui.base.WindowAndroid;
import org.chromium.ui.base.WindowAndroid.ActivityStateObserver;
import org.chromium.ui.modelutil.LayoutViewBuilder;
import org.chromium.ui.modelutil.MVCListAdapter.ModelList;
import org.chromium.ui.modelutil.PropertyModel;
import org.chromium.ui.modelutil.PropertyModelChangeProcessor;
import org.chromium.ui.modelutil.SimpleRecyclerViewAdapter;
import org.chromium.ui.util.ColorUtils;
import org.chromium.url.GURL;

import java.lang.ref.WeakReference;
import java.util.HashMap;
import java.util.List;
import java.util.Map;

/**
 * Creates the AccountSelection component. AccountSelection uses a bottom sheet
 * to let the user select an account.
 */
public class AccountSelectionCoordinator
        implements AccountSelectionComponent, ActivityStateObserver {
    private static final int MAX_IMAGE_CACHE_SIZE = 500 * ConversionUtils.BYTES_PER_KILOBYTE;

    private static Map<Integer, WeakReference<AccountSelectionComponent.Delegate>>
            sFedCMDelegateMap = new HashMap<>();

    // A counter used to generate a unique ID every time a new showModalDialog()
    // call occurs.
    private static int sCurrentFedcmId;

    private Tab mTab;
    private WindowAndroid mWindowAndroid;
    private BottomSheetController mBottomSheetController;
    private AccountSelectionBottomSheetContent mBottomSheetContent;
    private AccountSelectionComponent.Delegate mDelegate;
    private AccountSelectionMediator mMediator;
    private RecyclerView mSheetItemListView;
    private WeakReference<AccountSelectionComponent> mPopupComponent;
    private WeakReference<AccountSelectionComponent.Delegate> mOpenerDelegate;

    public AccountSelectionCoordinator(
            Tab tab,
            WindowAndroid windowAndroid,
            BottomSheetController sheetController,
            @RpMode.EnumType int rpMode,
            AccountSelectionComponent.Delegate delegate) {
        mTab = tab;
        mBottomSheetController = sheetController;
        mWindowAndroid = windowAndroid;
        mDelegate = delegate;
        Context context = mWindowAndroid.getContext().get();

        PropertyModel model =
                new PropertyModel.Builder(AccountSelectionProperties.ItemProperties.ALL_KEYS)
                        .build();
        // Construct view and its related adaptor to be displayed in the bottom sheet.
        ModelList sheetItems = new ModelList();
        View contentView = setupContentView(context, model, sheetItems, rpMode);
        mSheetItemListView = contentView.findViewById(R.id.sheet_item_list);

        // Setup the bottom sheet content view.
        mBottomSheetContent =
                new AccountSelectionBottomSheetContent(
                        contentView,
                        mBottomSheetController,
                        mSheetItemListView::computeVerticalScrollOffset,
                        rpMode);

        ImageFetcher imageFetcher =
                ImageFetcherFactory.createImageFetcher(
                        ImageFetcherConfig.IN_MEMORY_ONLY,
                        tab.getProfile().getProfileKey(),
                        GlobalDiscardableReferencePool.getReferencePool(),
                        MAX_IMAGE_CACHE_SIZE);

        @Px
        int avatarSize =
                context.getResources()
                        .getDimensionPixelSize(
                                rpMode == RpMode.ACTIVE
                                        ? R.dimen.account_selection_button_mode_sheet_avatar_size
                                        : R.dimen.account_selection_account_avatar_size);
        mMediator =
                new AccountSelectionMediator(
                        tab,
                        delegate,
                        model,
                        sheetItems,
                        mBottomSheetController,
                        mBottomSheetContent,
                        imageFetcher,
                        avatarSize,
                        rpMode);

        // If this object is corresponding to the custom tab opened by showModalDialog, this
        // is the first chance to associate it with the opener, so do so now.
        int fedcmId = getFedCmId();
        if (fedcmId == -1) return;
        mOpenerDelegate = sFedCMDelegateMap.remove(fedcmId);
        if (mOpenerDelegate == null || mOpenerDelegate.get() == null) {
            return;
        }
        mOpenerDelegate.get().setPopupComponent(this);
    }

    static View setupContentView(
            Context context,
            PropertyModel model,
            ModelList sheetItems,
            @RpMode.EnumType int rpMode) {
        int accountSelectionSheetLayout =
                rpMode == RpMode.ACTIVE
                        ? R.layout.account_selection_button_mode_sheet
                        : R.layout.account_selection_sheet;
        View contentView =
                (LinearLayout)
                        LayoutInflater.from(context).inflate(accountSelectionSheetLayout, null);

        PropertyModelChangeProcessor.create(
                model, contentView, AccountSelectionViewBinder::bindContentView);

        RecyclerView sheetItemListView = contentView.findViewById(R.id.sheet_item_list);
        sheetItemListView.setLayoutManager(
                new LinearLayoutManager(
                        sheetItemListView.getContext(), LinearLayoutManager.VERTICAL, false));
        sheetItemListView.setItemAnimator(null);
        if (rpMode == RpMode.ACTIVE) {
            // AccountPickerItemDecoration updates the background and rounds the edges of the
            // account list items.
            sheetItemListView.addItemDecoration(new AccountPickerItemDecoration());
        }

        // Setup the recycler view to be updated as we update the sheet items.
        SimpleRecyclerViewAdapter adapter = new SimpleRecyclerViewAdapter(sheetItems);
        adapter.registerType(
                AccountSelectionProperties.ITEM_TYPE_ACCOUNT,
                new LayoutViewBuilder(
                        rpMode == RpMode.ACTIVE
                                ? R.layout.account_selection_button_mode_account_item
                                : R.layout.account_selection_account_item),
                AccountSelectionViewBinder::bindAccountView);
        adapter.registerType(
                AccountSelectionProperties.ITEM_TYPE_ADD_ACCOUNT,
                new LayoutViewBuilder(R.layout.account_selection_add_account_row_item),
                AccountSelectionViewBinder::bindAddAccountView);
        sheetItemListView.setAdapter(adapter);

        return contentView;
    }

    static int generatedFedCMId() {
        // Get a non-negative number so that we can use -1 as an error.
        return ++sCurrentFedcmId;
    }

    @Override
    public void showAccounts(
            String rpEtldPlusOne,
            String idpEtldPlusOne,
            List<Account> accounts,
            IdentityProviderData idpData,
            boolean isAutoReauthn,
            List<Account> newAccounts) {
        mMediator.showAccounts(
                rpEtldPlusOne, idpEtldPlusOne, accounts, idpData, isAutoReauthn, newAccounts);
    }

    @Override
    public void showFailureDialog(
            String rpForDisplay,
            String idpForDisplay,
            IdentityProviderMetadata idpMetadata,
            @RpContext.EnumType int rpContext) {
        mMediator.showFailureDialog(rpForDisplay, idpForDisplay, idpMetadata, rpContext);
    }

    @Override
    public void showErrorDialog(
            String rpForDisplay,
            String idpForDisplay,
            IdentityProviderMetadata idpMetadata,
            @RpContext.EnumType int rpContext,
            IdentityCredentialTokenError error) {
        mMediator.showErrorDialog(rpForDisplay, idpForDisplay, idpMetadata, rpContext, error);
    }

    @Override
    public void showLoadingDialog(
            String rpForDisplay, String idpForDisplay, @RpContext.EnumType int rpContext) {
        mMediator.showLoadingDialog(rpForDisplay, idpForDisplay, rpContext);
    }

    @Override
    public void close() {
        if (mOpenerDelegate == null) {
            // Close the bottom sheet.
            mMediator.close();
            return;
        }
        // This is the popup.
        Activity activity = mWindowAndroid.getActivity().get();
        if (activity != null) {
            activity.finish();
        }
    }

    @Override
    public String getTitle() {
        TextView title = mBottomSheetContent.getContentView().findViewById(R.id.header_title);
        return String.valueOf(title.getText());
    }

    @Override
    public String getSubtitle() {
        TextView subtitle = mBottomSheetContent.getContentView().findViewById(R.id.header_subtitle);
        if (subtitle == null || subtitle.getText().length() == 0) return null;
        return String.valueOf(subtitle.getText());
    }

    @Override
    public void showUrl(@IdentityRequestDialogLinkType int linkType, GURL url) {
        Context context = mWindowAndroid.getContext().get();
        mMediator.showUrl(context, linkType, url);
    }

    @Override
    public WebContents showModalDialog(GURL url) {
        Context context = mWindowAndroid.getContext().get();
        CustomTabsIntent customTabIntent =
                new CustomTabsIntent.Builder()
                        .setShowTitle(true)
                        .setColorScheme(
                                ColorUtils.inNightMode(context)
                                        ? COLOR_SCHEME_DARK
                                        : COLOR_SCHEME_LIGHT)
                        .build();
        customTabIntent.intent.setData(Uri.parse(url.getSpec()));

        Intent intent =
                LaunchIntentDispatcher.createCustomTabActivityIntent(
                        context, customTabIntent.intent);
        intent.setPackage(context.getPackageName());
        intent.putExtra(Browser.EXTRA_APPLICATION_ID, context.getPackageName());
        assert context instanceof Activity;
        // Set a new FedCM ID, and store it.
        int fedcmId = generatedFedCMId();
        sFedCMDelegateMap.put(
                fedcmId, new WeakReference<AccountSelectionComponent.Delegate>(mDelegate));
        intent.putExtra(IntentHandler.EXTRA_FEDCM_ID, fedcmId);
        IntentUtils.addTrustedIntentExtras(intent);

        mWindowAndroid.addActivityStateObserver(this);
        context.startActivity(intent);
        mMediator.onModalDialogOpened();
        // CCT is opened asynchronously, and we do not have the WebContents for it yet.
        return null;
    }

    @Override
    public void closeModalDialog() {
        if (mPopupComponent == null || mPopupComponent.get() == null) {
            return;
        }
        mPopupComponent.get().close();
        mDelegate.onModalDialogClosed();
    }

    @Override
    public void onModalDialogClosed() {
        // When the opener is notified that the CCT is about to be closed, we call
        // removeActivityStateObserver() so that we do not invoke onDismissed() once the
        // activity is resumed.
        mWindowAndroid.removeActivityStateObserver(this);
        mMediator.onModalDialogClosed();
    }

    @Override
    public WebContents getWebContents() {
        return mTab.getWebContents();
    }

    @Override
    public WebContents getRpWebContents() {
        if (mOpenerDelegate == null || mOpenerDelegate.get() == null) {
            return null;
        }
        return mOpenerDelegate.get().getWebContents();
    }

    @Override
    public void setPopupComponent(AccountSelectionComponent component) {
        mPopupComponent = new WeakReference<AccountSelectionComponent>(component);
    }

    // ActivityStateObserver
    @Override
    public void onActivityPaused() {}

    @Override
    public void onActivityResumed() {
        // This method would only be invoked after showModalDialog() is invoked and a
        // CCT is opened (which register this as an observer), and then the CCT is
        // closed such that this is resumed. This method would then be invoked on the CCT opener's
        // object.
        mWindowAndroid.removeActivityStateObserver(this);
        mMediator.onDismissed(IdentityRequestDialogDismissReason.OTHER);
    }

    @Override
    public void onActivityDestroyed() {
        // The observer is only registered while the popup is being
        // shown, so we can just unconditionally record this histogram.
        RecordHistogram.recordBooleanHistogram(
                "Blink.FedCm.Android.ActivityDestroyedWhileCctShown", true);
    }

    @VisibleForTesting
    AccountSelectionMediator getMediator() {
        return mMediator;
    }

    private int getFedCmId() {
        // This should be called on the object corresponding to the CCT.
        Activity activity = mWindowAndroid.getActivity().get();
        if (!(activity instanceof ChromeActivity)) {
            return -1;
        }
        ChromeActivity chromeActivity = (ChromeActivity) activity;
        return IntentUtils.safeGetIntExtra(
                chromeActivity.getIntent(), IntentHandler.EXTRA_FEDCM_ID, -1);
    }
}
