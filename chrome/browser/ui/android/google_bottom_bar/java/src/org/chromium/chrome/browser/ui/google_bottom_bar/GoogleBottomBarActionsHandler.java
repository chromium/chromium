// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.ui.google_bottom_bar;

import static org.chromium.chrome.browser.gsa.GSAUtils.GSA_CLASS_NAME;
import static org.chromium.chrome.browser.gsa.GSAUtils.GSA_PACKAGE_NAME;
import static org.chromium.chrome.browser.gsa.GSAUtils.VOICE_SEARCH_INTENT_ACTION;
import static org.chromium.chrome.browser.ui.google_bottom_bar.GoogleBottomBarLogger.GoogleBottomBarButtonEvent.SEARCHBOX_HOME;
import static org.chromium.chrome.browser.ui.google_bottom_bar.GoogleBottomBarLogger.GoogleBottomBarButtonEvent.SEARCHBOX_LENS;
import static org.chromium.chrome.browser.ui.google_bottom_bar.GoogleBottomBarLogger.GoogleBottomBarButtonEvent.SEARCHBOX_SEARCH;
import static org.chromium.chrome.browser.ui.google_bottom_bar.GoogleBottomBarLogger.GoogleBottomBarButtonEvent.SEARCHBOX_VOICE_SEARCH;

import android.app.Activity;
import android.app.ActivityOptions;
import android.app.PendingIntent;
import android.app.SearchManager;
import android.content.Intent;
import android.net.Uri;
import android.view.View;

import androidx.annotation.VisibleForTesting;

import org.chromium.base.ApiCompatibilityUtils;
import org.chromium.base.Log;
import org.chromium.base.PackageManagerUtils;
import org.chromium.base.metrics.RecordUserAction;
import org.chromium.base.supplier.Supplier;
import org.chromium.chrome.browser.lens.LensController;
import org.chromium.chrome.browser.lens.LensEntryPoint;
import org.chromium.chrome.browser.lens.LensIntentParams;
import org.chromium.chrome.browser.lens.LensQueryParams;
import org.chromium.chrome.browser.share.ShareDelegate;
import org.chromium.chrome.browser.tab.Tab;
import org.chromium.chrome.browser.ui.google_bottom_bar.BottomBarConfig.ButtonConfig;
import org.chromium.chrome.browser.ui.google_bottom_bar.BottomBarConfig.ButtonId;
import org.chromium.chrome.browser.ui.google_bottom_bar.GoogleBottomBarLogger.GoogleBottomBarButtonEvent;
import org.chromium.chrome.browser.util.ChromeAccessibilityUtil;
import org.chromium.components.browser_ui.widget.textbubble.TextBubble;
import org.chromium.ui.base.DeviceFormFactor;
import org.chromium.ui.base.WindowAndroid;
import org.chromium.ui.widget.ViewRectProvider;

/** A handler class for actions triggered by buttons in a GoogleBottomBar. */
class GoogleBottomBarActionsHandler {
    private static final String TAG = "GBBActionHandler";

    @VisibleForTesting
    static final String EXTRA_IS_LAUNCHED_FROM_CHROME_SEARCH_ENTRYPOINT =
            "launched_from_chrome_search_entrypoint";

    private final Activity mActivity;
    private final Supplier<Tab> mTabProvider;
    private final Supplier<ShareDelegate> mShareDelegateSupplier;

    GoogleBottomBarActionsHandler(
            Activity activity,
            Supplier<Tab> tabProvider,
            Supplier<ShareDelegate> shareDelegateSupplier) {
        mActivity = activity;
        mTabProvider = tabProvider;
        mShareDelegateSupplier = shareDelegateSupplier;
    }

    View.OnClickListener getClickListener(ButtonConfig buttonConfig) {
        switch (buttonConfig.getId()) {
            case ButtonId.SAVE -> {
                return v -> onSaveButtonClick(buttonConfig, v);
            }
            case ButtonId.SHARE -> {
                return v -> onShareButtonClick(buttonConfig);
            }
            case ButtonId.SEARCH -> {
                return v -> onSearchButtonClick(buttonConfig);
            }
            case ButtonId.HOME -> {
                return v -> onHomeButtonClick(buttonConfig);
            }
            case ButtonId.PIH_BASIC,
                    ButtonId.PIH_EXPANDED,
                    ButtonId.PIH_COLORED,
                    ButtonId.CUSTOM -> {
                return v -> startPendingIntentIfPresentOrThrowError(buttonConfig);
            }
            case ButtonId.ADD_NOTES, ButtonId.REFRESH -> {
                Log.e(TAG, "Unsupported action: %s", buttonConfig.getId());
                return null;
            }
        }
        return null;
    }

    void onSearchboxHomeTap() {
        GoogleBottomBarLogger.logButtonClicked(SEARCHBOX_HOME);
        openGoogleAppHome();
    }

    void onSearchboxHintTextTap() {
        GoogleBottomBarLogger.logButtonClicked(SEARCHBOX_SEARCH);
        openGoogleAppSearch();
    }

    void onSearchboxMicTap() {
        GoogleBottomBarLogger.logButtonClicked(SEARCHBOX_VOICE_SEARCH);
        Intent intent = new Intent(VOICE_SEARCH_INTENT_ACTION);
        intent.setPackage(GSA_PACKAGE_NAME);

        startGoogleAppActivityForResult(intent, "openGoogleAppVoiceSearch");
    }

    void onSearchboxLensTap(View buttonView) {
        GoogleBottomBarLogger.logButtonClicked(SEARCHBOX_LENS);
        Tab tab = mTabProvider.get();
        if (tab == null) {
            Log.e(TAG, "Can't open Lens as tab is not available.");
            return;
        }
        WindowAndroid window = tab.getWindowAndroid();

        if (window == null) {
            Log.e(TAG, "Can't open Lens as window is not available.");
            return;
        }

        boolean isIncognito = tab.isIncognito();
        LensController lensController = LensController.getInstance();
        LensQueryParams lensQueryParams =
                new LensQueryParams.Builder(
                                LensEntryPoint.GOOGLE_BOTTOM_BAR,
                                isIncognito,
                                DeviceFormFactor.isWindowOnTablet(window))
                        .build();

        if (lensController.isLensEnabled(lensQueryParams)) {
            LensIntentParams lensIntentParams =
                    new LensIntentParams.Builder(LensEntryPoint.GOOGLE_BOTTOM_BAR, isIncognito)
                            .build();
            lensController.startLens(window, lensIntentParams);
        } else {
            showTooltip(
                    buttonView,
                    R.string.google_bottom_bar_searchbox_lens_not_enabled_tooltip_message);
        }
    }

    private void openGoogleAppSearch() {
        Intent intent = new Intent(SearchManager.INTENT_ACTION_GLOBAL_SEARCH);
        intent.setPackage(GSA_PACKAGE_NAME);

        startGoogleAppActivityForResult(intent, "openGoogleAppSearch");
    }

    private void openGoogleAppHome() {
        Intent intent = new Intent(Intent.ACTION_MAIN);
        intent.addCategory(Intent.CATEGORY_INFO);
        intent.setClassName(GSA_PACKAGE_NAME, GSA_CLASS_NAME);

        startGoogleAppActivityForResult(intent, "openGoogleAppHome");
    }

    private void startGoogleAppActivityForResult(Intent intent, String actionName) {
        intent.putExtra(EXTRA_IS_LAUNCHED_FROM_CHROME_SEARCH_ENTRYPOINT, true);

        if (PackageManagerUtils.canResolveActivity(intent)) {
            Log.w(TAG, "Starts action: %s.", actionName);
            // startActivityForResult is added so that Google App can verify that the calling
            // activity is Chrome
            // Request code will not be checked in onActivityResult
            mActivity.startActivityForResult(intent, /* requestCode= */ 0);
        } else {
            String message = String.format("Can't resolve activity for action: %s", actionName);
            Log.e(TAG, message);
            throw new IllegalStateException(message);
        }
    }

    private void onSearchButtonClick(ButtonConfig buttonConfig) {
        PendingIntent pendingIntent = buttonConfig.getPendingIntent();
        if (pendingIntent != null) {
            GoogleBottomBarLogger.logButtonClicked(GoogleBottomBarButtonEvent.SEARCH_EMBEDDER);
            sendPendingIntentWithUrl(pendingIntent);
        } else {
            GoogleBottomBarLogger.logButtonClicked(GoogleBottomBarButtonEvent.SEARCH_CHROME);
            openGoogleAppSearch();
        }
    }

    private void onHomeButtonClick(ButtonConfig buttonConfig) {
        PendingIntent pendingIntent = buttonConfig.getPendingIntent();
        if (pendingIntent != null) {
            GoogleBottomBarLogger.logButtonClicked(GoogleBottomBarButtonEvent.HOME_EMBEDDER);
            sendPendingIntentWithUrl(pendingIntent);
        } else {
            GoogleBottomBarLogger.logButtonClicked(GoogleBottomBarButtonEvent.HOME_CHROME);
            openGoogleAppHome();
        }
    }

    private void startPendingIntentIfPresentOrThrowError(ButtonConfig buttonConfig) {
        PendingIntent pendingIntent = buttonConfig.getPendingIntent();
        if (pendingIntent != null) {
            sendPendingIntentWithUrl(pendingIntent);
            GoogleBottomBarLogger.logButtonClicked(
                    GoogleBottomBarLogger.getGoogleBottomBarButtonEvent(buttonConfig));
        } else {
            Log.e(
                    TAG,
                    "Can't perform action with id: %s as pending intent is null.",
                    buttonConfig.getId());
        }
    }

    private void onShareButtonClick(ButtonConfig buttonConfig) {
        // TODO(b/342576463) Remove after GBB experiment
        RecordUserAction.record("CustomTabsCustomActionButtonClick");
        PendingIntent pendingIntent = buttonConfig.getPendingIntent();
        if (pendingIntent != null) {
            GoogleBottomBarLogger.logButtonClicked(GoogleBottomBarButtonEvent.SHARE_EMBEDDER);
            sendPendingIntentWithUrl(pendingIntent);
        } else {
            GoogleBottomBarLogger.logButtonClicked(GoogleBottomBarButtonEvent.SHARE_CHROME);
            initiateShareForCurrentTab();
        }
    }

    private void initiateShareForCurrentTab() {
        Tab tab = mTabProvider.get();
        if (tab == null) {
            Log.e(TAG, "Can't perform share action as tab is null.");
            return;
        }

        ShareDelegate shareDelegate = mShareDelegateSupplier.get();
        if (shareDelegate == null) {
            Log.e(TAG, "Can't perform share action as share delegate is null.");
            return;
        }
        shareDelegate.share(
                tab, /* shareDirectly= */ false, ShareDelegate.ShareOrigin.GOOGLE_BOTTOM_BAR);
    }

    private void onSaveButtonClick(ButtonConfig buttonConfig, View view) {
        // TODO(b/342576463) Remove after GBB experiment
        RecordUserAction.record("CustomTabsCustomActionButtonClick");
        PendingIntent pendingIntent = buttonConfig.getPendingIntent();
        if (pendingIntent != null) {
            GoogleBottomBarLogger.logButtonClicked(GoogleBottomBarButtonEvent.SAVE_EMBEDDER);
            sendPendingIntentWithUrl(pendingIntent);

        } else {
            GoogleBottomBarLogger.logButtonClicked(GoogleBottomBarButtonEvent.SAVE_DISABLED);
            showTooltip(view, R.string.google_bottom_bar_save_disabled_tooltip_message);
        }
    }

    private void showTooltip(View view, int messageId) {
        ViewRectProvider rectProvider = new ViewRectProvider(view);
        TextBubble textBubble =
                new TextBubble(
                        view.getContext(),
                        view,
                        /* stringId= */ messageId,
                        /* accessibilityStringId= */ messageId,
                        /* showArrow= */ true,
                        rectProvider,
                        ChromeAccessibilityUtil.get().isAccessibilityEnabled());
        textBubble.setFocusable(true);
        textBubble.setDismissOnTouchInteraction(true);
        textBubble.show();
    }

    private void sendPendingIntentWithUrl(PendingIntent pendingIntent) {
        Tab tab = mTabProvider.get();
        if (tab == null) {
            Log.e(TAG, "Can't send pending intent as tab is null.");
            return;
        }
        Intent addedIntent = new Intent();
        addedIntent.setData(Uri.parse(tab.getUrl().getSpec()));
        try {
            ActivityOptions options = ActivityOptions.makeBasic();
            ApiCompatibilityUtils.setActivityOptionsBackgroundActivityStartMode(options);
            pendingIntent.send(
                    mActivity,
                    /* code= */ 0,
                    addedIntent,
                    /* onFinished= */ null,
                    /* handler= */ null,
                    /* requiredPermissions= */ null,
                    /* options= */ options.toBundle());
        } catch (PendingIntent.CanceledException e) {
            Log.e(TAG, "CanceledException when sending pending intent.", e);
        }
    }
}
