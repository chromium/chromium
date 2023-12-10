// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.webapps;

import android.app.Activity;
import android.content.Context;
import android.content.pm.ResolveInfo;

import org.chromium.base.metrics.RecordUserAction;
import org.chromium.chrome.browser.banners.AppMenuVerbiage;
import org.chromium.chrome.browser.feature_engagement.TrackerFactory;
import org.chromium.chrome.browser.profiles.Profile;
import org.chromium.chrome.browser.tab.Tab;
import org.chromium.components.browser_ui.util.TraceEventVectorDrawableCompat;
import org.chromium.components.embedder_support.util.UrlConstants;
import org.chromium.components.feature_engagement.EventConstants;
import org.chromium.components.feature_engagement.FeatureConstants;
import org.chromium.components.feature_engagement.Tracker;
import org.chromium.components.messages.MessageBannerProperties;
import org.chromium.components.messages.MessageDispatcher;
import org.chromium.components.messages.MessageIdentifier;
import org.chromium.components.messages.MessageScopeType;
import org.chromium.components.messages.PrimaryActionClickBehavior;
import org.chromium.components.webapk.lib.client.WebApkValidator;
import org.chromium.components.webapps.AddToHomescreenCoordinator;
import org.chromium.components.webapps.AppBannerManager;
import org.chromium.components.webapps.WebappsUtils;
import org.chromium.content_public.browser.WebContents;
import org.chromium.ui.base.WindowAndroid;
import org.chromium.ui.modaldialog.ModalDialogManager;
import org.chromium.ui.modelutil.PropertyModel;
import org.chromium.url.GURL;

/**
 * This class is responsible for rendering the 'Add to home screen' IPH, such as IPH bubble/message
 * creation, metrics logging etc.
 */
public class AddToHomescreenIPHController {
    private Activity mActivity;
    private final WindowAndroid mWindowAndroid;
    private final ModalDialogManager mModalDialogManager;
    private final MessageDispatcher mMessageDispatcher;
    private final Tracker mTracker;

    /**
     * Creates an {@link AddToHomescreenIPHController}.
     *
     * @param activity The associated activity.
     * @param windowAndroid The associated {@link WindowAndroid}.
     * @param modalDialogManager The {@link ModalDialogManager} for showing the dialog.
     * @param messageDispatcher The {@link MessageDispatcher} for displaying messages.
     */
    public AddToHomescreenIPHController(
            Activity activity,
            WindowAndroid windowAndroid,
            ModalDialogManager modalDialogManager,
            MessageDispatcher messageDispatcher) {
        mActivity = activity;
        mWindowAndroid = windowAndroid;
        mModalDialogManager = modalDialogManager;
        mMessageDispatcher = messageDispatcher;
        mTracker = TrackerFactory.getTrackerForProfile(Profile.getLastUsedRegularProfile());
    }

    /**
     * Called to show in-product-help message.
     *
     * @param tab The current tab.
     */
    public void showAddToHomescreenIPH(Tab tab) {
        if (!mTracker.shouldTriggerHelpUI(FeatureConstants.ADD_TO_HOMESCREEN_MESSAGE_FEATURE)) {
            return;
        }

        if (mActivity == null) return;
        if (!canShowAddToHomescreenMenuItem(mActivity, tab)) return;
        showMessageIPH(tab);
    }

    /** Called to notify that the activity is in the process of being destroyed. */
    public void destroy() {
        mActivity = null;
    }

    private static boolean canShowAddToHomescreenMenuItem(Context context, Tab tab) {
        if (tab.isIncognito()) return false;

        GURL url = tab.getUrl();
        if (url.isEmpty() || !url.isValid()) return false;

        String urlString = url.getSpec();
        boolean isChromeScheme =
                urlString.startsWith(UrlConstants.CHROME_URL_PREFIX)
                        || urlString.startsWith(UrlConstants.CHROME_NATIVE_URL_PREFIX);
        boolean isFileScheme = urlString.startsWith(UrlConstants.FILE_URL_PREFIX);
        boolean isContentScheme = urlString.startsWith(UrlConstants.CONTENT_URL_PREFIX);
        boolean urlSchemeSupported = !isChromeScheme && !isFileScheme && !isContentScheme;

        if (!urlSchemeSupported) return false;
        if (!WebappsUtils.isAddToHomeIntentSupported()) return false;

        // If it is a web apk, don't show the IPH.
        ResolveInfo resolveInfo = WebApkValidator.queryFirstWebApkResolveInfo(context, urlString);
        boolean isInstalledAsPwa =
                resolveInfo != null && resolveInfo.activityInfo.packageName != null;
        if (isInstalledAsPwa) return false;

        // If it can be installed as a PWA, don't show the IPH.
        WebContents webContents = tab.getWebContents();
        AppBannerManager manager =
                webContents != null ? AppBannerManager.forWebContents(webContents) : null;
        boolean canInstallAsPwa = manager != null && manager.getIsPwa(webContents);
        if (canInstallAsPwa) return false;

        return true;
    }

    private void showMessageIPH(Tab tab) {
        PropertyModel model =
                new PropertyModel.Builder(MessageBannerProperties.ALL_KEYS)
                        .with(
                                MessageBannerProperties.MESSAGE_IDENTIFIER,
                                MessageIdentifier.ADD_TO_HOMESCREEN_IPH)
                        .with(
                                MessageBannerProperties.ICON,
                                TraceEventVectorDrawableCompat.create(
                                        mActivity.getResources(),
                                        R.drawable.ic_apps_blue_24dp,
                                        mActivity.getTheme()))
                        .with(
                                MessageBannerProperties.TITLE,
                                mActivity
                                        .getResources()
                                        .getString(R.string.iph_message_add_to_home_screen_title))
                        .with(
                                MessageBannerProperties.DESCRIPTION,
                                mActivity
                                        .getResources()
                                        .getString(
                                                R.string
                                                        .iph_message_add_to_home_screen_description))
                        .with(
                                MessageBannerProperties.PRIMARY_BUTTON_TEXT,
                                mActivity
                                        .getResources()
                                        .getString(R.string.iph_message_add_to_home_screen_action))
                        .with(MessageBannerProperties.ON_DISMISSED, this::onMessageDismissed)
                        .with(
                                MessageBannerProperties.ON_PRIMARY_ACTION,
                                () -> {
                                    onMessageAddButtonClicked(tab);
                                    return PrimaryActionClickBehavior.DISMISS_IMMEDIATELY;
                                })
                        .build();
        mMessageDispatcher.enqueueMessage(
                model, tab.getWebContents(), MessageScopeType.NAVIGATION, false);
        RecordUserAction.record("Android.AddToHomescreenIPH.Message.Shown");
    }

    private void onMessageAddButtonClicked(Tab tab) {
        if (tab.isDestroyed() || mActivity == null) return;

        AddToHomescreenCoordinator.showForAppMenu(
                mActivity,
                mWindowAndroid,
                mModalDialogManager,
                tab.getWebContents(),
                AppMenuVerbiage.APP_MENU_OPTION_ADD_TO_HOMESCREEN);
        mTracker.notifyEvent(EventConstants.ADD_TO_HOMESCREEN_DIALOG_SHOWN);
        RecordUserAction.record("Android.AddToHomescreenIPH.Message.Clicked");
    }

    private void onMessageDismissed(Integer dismissReason) {
        // TODO(shaktisahu): Record metrics for explicit dismiss vs timeout.
        mTracker.dismissed(FeatureConstants.ADD_TO_HOMESCREEN_MESSAGE_FEATURE);
    }
}
