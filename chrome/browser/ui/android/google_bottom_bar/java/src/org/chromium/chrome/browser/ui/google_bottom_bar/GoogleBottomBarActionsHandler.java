// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.ui.google_bottom_bar;

import android.app.Activity;
import android.app.ActivityOptions;
import android.app.PendingIntent;
import android.content.Intent;
import android.net.Uri;
import android.view.View;

import org.chromium.base.ApiCompatibilityUtils;
import org.chromium.base.Log;
import org.chromium.base.supplier.Supplier;
import org.chromium.chrome.browser.page_insights.PageInsightsCoordinator;
import org.chromium.chrome.browser.share.ShareDelegate;
import org.chromium.chrome.browser.tab.Tab;
import org.chromium.chrome.browser.ui.google_bottom_bar.BottomBarConfig.ButtonConfig;
import org.chromium.chrome.browser.ui.google_bottom_bar.BottomBarConfigCreator.ButtonId;
import org.chromium.chrome.browser.ui.google_bottom_bar.GoogleBottomBarLogger.GoogleBottomBarButtonEvent;
import org.chromium.chrome.browser.util.ChromeAccessibilityUtil;
import org.chromium.components.browser_ui.widget.textbubble.TextBubble;
import org.chromium.ui.widget.ViewRectProvider;

/** A handler class for actions triggered by buttons in a GoogleBottomBar. */
class GoogleBottomBarActionsHandler {
    private static final String TAG = "GBBActionHandler";
    private final Activity mActivity;
    private final Supplier<Tab> mTabProvider;
    private final Supplier<ShareDelegate> mShareDelegateSupplier;
    private final Supplier<PageInsightsCoordinator> mPageInsightsCoordinatorSupplier;

    GoogleBottomBarActionsHandler(
            Activity activity,
            Supplier<Tab> tabProvider,
            Supplier<ShareDelegate> shareDelegateSupplier,
            Supplier<PageInsightsCoordinator> pageInsightsCoordinatorSupplier) {
        mActivity = activity;
        mTabProvider = tabProvider;
        mShareDelegateSupplier = shareDelegateSupplier;
        mPageInsightsCoordinatorSupplier = pageInsightsCoordinatorSupplier;
    }

    View.OnClickListener getClickListener(ButtonConfig buttonConfig) {
        switch (buttonConfig.getId()) {
            case ButtonId.SAVE -> {
                return v -> onSaveButtonClick(buttonConfig, v);
            }
            case ButtonId.SHARE -> {
                return v -> onShareButtonClick(buttonConfig);
            }
            case ButtonId.PIH_BASIC, ButtonId.PIH_EXPANDED, ButtonId.PIH_COLORED -> {
                return v -> onPageInsightsButtonClick(buttonConfig);
            }
            case ButtonId.ADD_NOTES, ButtonId.REFRESH -> {
                Log.e(TAG, "Unsupported action: %s", buttonConfig.getId());
                return null;
            }
        }
        return null;
    }

    private void onPageInsightsButtonClick(ButtonConfig buttonConfig) {
        if (mPageInsightsCoordinatorSupplier.get() != null) {
            mPageInsightsCoordinatorSupplier.get().launch();
            GoogleBottomBarLogger.logButtonClicked(GoogleBottomBarButtonEvent.PIH_CHROME);
        } else {
            PendingIntent pendingIntent = buttonConfig.getPendingIntent();
            if (pendingIntent != null) {
                sendPendingIntentWithUrl(pendingIntent);
                GoogleBottomBarLogger.logButtonClicked(GoogleBottomBarButtonEvent.PIH_EMBEDDER);
            } else {
                Log.e(TAG, "Can't perform page insights action as pending intent is null.");
            }
        }
    }

    private void onShareButtonClick(ButtonConfig buttonConfig) {
        PendingIntent pendingIntent = buttonConfig.getPendingIntent();
        if (pendingIntent != null) {
            sendPendingIntentWithUrl(pendingIntent);
            GoogleBottomBarLogger.logButtonClicked(GoogleBottomBarButtonEvent.SHARE_EMBEDDER);
        } else {
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
        GoogleBottomBarLogger.logButtonClicked(GoogleBottomBarButtonEvent.SHARE_CHROME);
    }

    private void onSaveButtonClick(ButtonConfig buttonConfig, View view) {
        PendingIntent pendingIntent = buttonConfig.getPendingIntent();
        if (pendingIntent != null) {
            sendPendingIntentWithUrl(pendingIntent);
            GoogleBottomBarLogger.logButtonClicked(GoogleBottomBarButtonEvent.SAVE_EMBEDDER);
        } else {
            showTooltip(view, R.string.google_bottom_bar_save_disabled_tooltip_message);
            GoogleBottomBarLogger.logButtonClicked(GoogleBottomBarButtonEvent.SAVE_DISABLED);
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
