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
import android.view.ViewGroup;
import android.widget.LinearLayout;
import android.widget.TextView;

import androidx.annotation.Px;
import androidx.browser.customtabs.CustomTabsIntent;
import androidx.recyclerview.widget.LinearLayoutManager;
import androidx.recyclerview.widget.RecyclerView;

import org.chromium.base.IntentUtils;
import org.chromium.chrome.browser.IntentHandler;
import org.chromium.chrome.browser.LaunchIntentDispatcher;
import org.chromium.chrome.browser.app.ChromeActivity;
import org.chromium.chrome.browser.profiles.Profile;
import org.chromium.chrome.browser.tab.Tab;
import org.chromium.chrome.browser.ui.android.webid.data.Account;
import org.chromium.chrome.browser.ui.android.webid.data.ClientIdMetadata;
import org.chromium.chrome.browser.ui.android.webid.data.IdentityCredentialTokenError;
import org.chromium.chrome.browser.ui.android.webid.data.IdentityProviderMetadata;
import org.chromium.components.browser_ui.bottomsheet.BottomSheetController;
import org.chromium.components.browser_ui.util.ConversionUtils;
import org.chromium.components.browser_ui.util.GlobalDiscardableReferencePool;
import org.chromium.components.image_fetcher.ImageFetcher;
import org.chromium.components.image_fetcher.ImageFetcherConfig;
import org.chromium.components.image_fetcher.ImageFetcherFactory;
import org.chromium.content.webid.IdentityRequestDialogDismissReason;
import org.chromium.content_public.browser.WebContents;
import org.chromium.ui.base.WindowAndroid;
import org.chromium.ui.base.WindowAndroid.ActivityStateObserver;
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

    private WindowAndroid mWindowAndroid;
    private BottomSheetController mBottomSheetController;
    private AccountSelectionBottomSheetContent mBottomSheetContent;
    private AccountSelectionComponent.Delegate mDelegate;
    private AccountSelectionMediator mMediator;
    private RecyclerView mSheetItemListView;

    public AccountSelectionCoordinator(Tab tab, WindowAndroid windowAndroid,
            BottomSheetController sheetController, AccountSelectionComponent.Delegate delegate) {
        mBottomSheetController = sheetController;
        mWindowAndroid = windowAndroid;
        mDelegate = delegate;
        Context context = mWindowAndroid.getContext().get();

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
        mMediator = new AccountSelectionMediator(tab, delegate, model, sheetItems,
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

    static int generatedFedCMId() {
        // Get a non-negative number so that we can use -1 as an error.
        return ++sCurrentFedcmId;
    }

    @Override
    public void showAccounts(String topFrameEtldPlusOne, String iframeEtldPlusOne,
            String idpEtldPlusOne, List<Account> accounts, IdentityProviderMetadata idpMetadata,
            ClientIdMetadata clientMetadata, boolean isAutoReauthn, String rpContext) {
        mMediator.showAccounts(topFrameEtldPlusOne, iframeEtldPlusOne, idpEtldPlusOne, accounts,
                idpMetadata, clientMetadata, isAutoReauthn, rpContext);
    }

    @Override
    public void showFailureDialog(String topFrameForDisplay, String iframeForDisplay,
            String idpForDisplay, IdentityProviderMetadata idpMetadata, String rpContext) {
        mMediator.showFailureDialog(
                topFrameForDisplay, iframeForDisplay, idpForDisplay, idpMetadata, rpContext);
    }

    @Override
    public void showErrorDialog(String topFrameForDisplay, String iframeForDisplay,
            String idpForDisplay, IdentityProviderMetadata idpMetadata, String rpContext,
            IdentityCredentialTokenError error) {
        mMediator.showErrorDialog(
                topFrameForDisplay, iframeForDisplay, idpForDisplay, idpMetadata, rpContext, error);
    }

    @Override
    public void close() {
        mMediator.close();
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
    public WebContents showModalDialog(GURL url) {
        Context context = mWindowAndroid.getContext().get();
        CustomTabsIntent customTabIntent =
                new CustomTabsIntent.Builder()
                        .setShowTitle(true)
                        .setColorScheme(ColorUtils.inNightMode(context) ? COLOR_SCHEME_DARK
                                                                        : COLOR_SCHEME_LIGHT)
                        .build();
        customTabIntent.intent.setData(Uri.parse(url.getSpec()));

        Intent intent = LaunchIntentDispatcher.createCustomTabActivityIntent(
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
        // Note that this method is invoked on the object corresponding to the CCT. It
        // will notify the opener that it is being closed and close itself.
        Activity activity = mWindowAndroid.getActivity().get();
        if (!(activity instanceof ChromeActivity)) {
            return;
        }
        ChromeActivity chromeActivity = (ChromeActivity) activity;
        int fedcmId = IntentUtils.safeGetIntExtra(
                chromeActivity.getIntent(), IntentHandler.EXTRA_FEDCM_ID, -1);
        // Close the current tab by finishing the activity, if we know it was initiated
        // by the FedCM API.
        if (fedcmId == -1) return;
        activity.finish();
        WeakReference<AccountSelectionComponent.Delegate> delegate =
                sFedCMDelegateMap.remove(fedcmId);
        if (delegate != null && delegate.get() != null) {
            delegate.get().onModalDialogClosed();
        }
    }

    @Override
    public void onModalDialogClosed() {
        // When the opener is notified that the CCT is about to be closed, we call
        // removeActivityStateObserver() so that we do not invoke onDismissed() once the
        // activity is resumed.
        mWindowAndroid.removeActivityStateObserver(this);
        mMediator.onModalDialogClosed();
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
    public void onActivityDestroyed() {}
}
