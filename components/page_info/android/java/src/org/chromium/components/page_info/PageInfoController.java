// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.components.page_info;

import android.app.Activity;
import android.content.ActivityNotFoundException;
import android.content.Context;
import android.content.Intent;
import android.graphics.Color;
import android.net.Uri;
import android.provider.Settings;
import android.text.Spannable;
import android.text.SpannableString;
import android.text.SpannableStringBuilder;
import android.text.style.ForegroundColorSpan;
import android.text.style.TextAppearanceSpan;
import android.view.View;
import android.view.Window;

import androidx.annotation.IntDef;
import androidx.annotation.VisibleForTesting;
import androidx.core.view.ViewCompat;

import org.chromium.base.ApiCompatibilityUtils;
import org.chromium.base.annotations.CalledByNative;
import org.chromium.base.annotations.NativeMethods;
import org.chromium.base.metrics.RecordUserAction;
import org.chromium.components.browser_ui.settings.SettingsUtils;
import org.chromium.components.content_settings.ContentSettingValues;
import org.chromium.components.content_settings.CookieControlsBridge;
import org.chromium.components.content_settings.CookieControlsEnforcement;
import org.chromium.components.content_settings.CookieControlsObserver;
import org.chromium.components.content_settings.CookieControlsStatus;
import org.chromium.components.dom_distiller.core.DomDistillerUrlUtils;
import org.chromium.components.embedder_support.browser_context.BrowserContextHandle;
import org.chromium.components.embedder_support.util.UrlUtilities;
import org.chromium.components.omnibox.AutocompleteSchemeClassifier;
import org.chromium.components.omnibox.OmniboxUrlEmphasizer;
import org.chromium.components.page_info.PageInfoView.ConnectionInfoParams;
import org.chromium.components.page_info.PageInfoView.PageInfoViewParams;
import org.chromium.components.security_state.ConnectionSecurityLevel;
import org.chromium.components.security_state.SecurityStateModel;
import org.chromium.components.url_formatter.SchemeDisplay;
import org.chromium.components.url_formatter.UrlFormatter;
import org.chromium.content_public.browser.WebContents;
import org.chromium.content_public.browser.WebContentsObserver;
import org.chromium.ui.base.Clipboard;
import org.chromium.ui.base.DeviceFormFactor;
import org.chromium.ui.base.WindowAndroid;
import org.chromium.ui.modaldialog.DialogDismissalCause;
import org.chromium.ui.modaldialog.ModalDialogProperties;
import org.chromium.ui.modaldialog.ModalDialogProperties.ButtonType;
import org.chromium.ui.modelutil.PropertyModel;
import org.chromium.ui.util.ColorUtils;
import org.chromium.url.GURL;

import java.lang.annotation.Retention;
import java.lang.annotation.RetentionPolicy;
import java.lang.ref.WeakReference;

/**
 * Java side of Android implementation of the page info UI.
 */
public class PageInfoController implements PageInfoMainController, ModalDialogProperties.Controller,
                                           SystemSettingsActivityRequiredListener,
                                           CookieControlsObserver {
    @IntDef({OpenedFromSource.MENU, OpenedFromSource.TOOLBAR, OpenedFromSource.VR})
    @Retention(RetentionPolicy.SOURCE)
    public @interface OpenedFromSource {
        int MENU = 1;
        int TOOLBAR = 2;
        int VR = 3;
    }

    private Context mContext;
    private final WindowAndroid mWindowAndroid;
    private final WebContents mWebContents;
    private final PageInfoControllerDelegate mDelegate;

    // A pointer to the C++ object for this UI.
    private long mNativePageInfoController;

    // The view inside the popup or the main PageInfo view.
    private PageInfoView mView;

    // The view inside the popup (V2).
    private PageInfoContainer mContainer;

    // The dialog the view is placed in.
    private PageInfoDialog mDialog;

    // The full URL from the URL bar, which is copied to the user's clipboard when they select 'Copy
    // URL'.
    private String mFullUrl;

    // Whether or not this page is an internal chrome page (e.g. the
    // chrome://settings page).
    private boolean mIsInternalPage;

    // The security level of the page (a valid ConnectionSecurityLevel).
    private int mSecurityLevel;

    // The name of the content publisher, if any.
    private String mContentPublisher;

    // Observer for dismissing dialog if web contents get destroyed, navigate etc.
    private WebContentsObserver mWebContentsObserver;

    // A task that should be run once the page info popup is animated out and dismissed. Null if no
    // task is pending.
    private Runnable mPendingRunAfterDismissTask;

    // Reference to last created PageInfoController for testing.
    private static WeakReference<PageInfoController> sLastPageInfoControllerForTesting;

    // Whether Version 2 of the PageInfoView is enabled.
    private boolean mIsV2Enabled;

    // Used to show Site settings from Page Info UI.
    private final PermissionParamsListBuilder mPermissionParamsListBuilder;

    // Delegate used by PermissionParamsListBuilder.
    private final PermissionParamsListBuilderDelegate mPermissionParamsListBuilderDelegate;

    // The current page info subpage controller, if any.
    private PageInfoSubpageController mSubpageController;

    // The controller for the connection section of the page info.
    private PageInfoConnectionController mConnectionController;

    // The controller for the permissions section of the page info.
    private PageInfoPermissionsController mPermissionsController;

    // The controller for the cookies section of the page info.
    private PageInfoCookiesController mCookiesController;

    // Bridge updating the CookieControlsView when cookie settings change.
    private CookieControlsBridge mCookieBridge;

    /**
     * Creates the PageInfoController, but does not display it. Also initializes the corresponding
     * C++ object and saves a pointer to it.
     * @param webContents              The WebContents showing the page that the PageInfo is about.
     * @param securityLevel            The security level of the page being shown.
     * @param publisher                The name of the content publisher, if any.
     * @param delegate                 The PageInfoControllerDelegate used to provide
     *                                 embedder-specific info.
     */
    @VisibleForTesting(otherwise = VisibleForTesting.PROTECTED)
    public PageInfoController(WebContents webContents, int securityLevel, String publisher,
            PageInfoControllerDelegate delegate,
            PermissionParamsListBuilderDelegate permissionParamsListBuilderDelegate) {
        mWebContents = webContents;
        mSecurityLevel = securityLevel;
        mDelegate = delegate;
        mIsV2Enabled = PageInfoFeatureList.isEnabled(PageInfoFeatureList.PAGE_INFO_V2);
        mPermissionParamsListBuilderDelegate = permissionParamsListBuilderDelegate;
        PageInfoViewParams viewParams = new PageInfoViewParams();

        mWindowAndroid = webContents.getTopLevelNativeWindow();
        mContext = mWindowAndroid.getContext().get();
        mContentPublisher = publisher;

        viewParams.urlTitleClickCallback = () -> {
            // Expand/collapse the displayed URL title.
            mView.toggleUrlTruncation();
        };
        // Long press the url text to copy it to the clipboard.
        viewParams.urlTitleLongClickCallback =
                () -> Clipboard.getInstance().copyUrlToClipboard(mFullUrl);

        // Work out the URL and connection message and status visibility.
        // TODO(crbug.com/1033178): dedupe the DomDistillerUrlUtils#getOriginalUrlFromDistillerUrl()
        // calls.
        mFullUrl = mDelegate.isShowingOfflinePage()
                ? mDelegate.getOfflinePageUrl()
                : DomDistillerUrlUtils.getOriginalUrlFromDistillerUrl(
                        webContents.getVisibleUrlString());

        // This can happen if an invalid chrome-distiller:// url was entered.
        if (mFullUrl == null) mFullUrl = "";

        GURL url = new GURL(mFullUrl);
        mIsInternalPage = UrlUtilities.isInternalScheme(url);

        String displayUrl = UrlFormatter.formatUrlForDisplayOmitUsernamePassword(mFullUrl);
        if (mDelegate.isShowingOfflinePage()) {
            displayUrl = UrlUtilities.stripScheme(mFullUrl);
        }
        SpannableStringBuilder displayUrlBuilder = new SpannableStringBuilder(displayUrl);
        AutocompleteSchemeClassifier autocompleteSchemeClassifier =
                delegate.createAutocompleteSchemeClassifier();
        if (mSecurityLevel == ConnectionSecurityLevel.SECURE) {
            OmniboxUrlEmphasizer.EmphasizeComponentsResponse emphasizeResponse =
                    OmniboxUrlEmphasizer.parseForEmphasizeComponents(
                            displayUrlBuilder.toString(), autocompleteSchemeClassifier);
            if (emphasizeResponse.schemeLength > 0) {
                displayUrlBuilder.setSpan(
                        new TextAppearanceSpan(mContext, R.style.TextAppearance_RobotoMediumStyle),
                        0, emphasizeResponse.schemeLength, Spannable.SPAN_EXCLUSIVE_INCLUSIVE);
            }
        }

        boolean useDarkText = !ColorUtils.inNightMode(mContext);
        OmniboxUrlEmphasizer.emphasizeUrl(displayUrlBuilder, mContext.getResources(),
                autocompleteSchemeClassifier, mSecurityLevel, mIsInternalPage, useDarkText,
                /*emphasizeScheme=*/true);
        viewParams.url = displayUrlBuilder;
        viewParams.urlOriginLength = OmniboxUrlEmphasizer.getOriginEndIndex(
                displayUrlBuilder.toString(), autocompleteSchemeClassifier);
        autocompleteSchemeClassifier.destroy();

        if (mDelegate.isSiteSettingsAvailable()) {
            viewParams.siteSettingsButtonClickCallback = () -> {
                // Delay while the dialog closes.
                runAfterDismiss(() -> {
                    recordAction(PageInfoAction.PAGE_INFO_SITE_SETTINGS_OPENED);
                    mDelegate.showSiteSettings(mFullUrl);
                });
            };
            viewParams.cookieControlsShown = delegate.cookieControlsShown();
        } else {
            viewParams.siteSettingsButtonShown = false;
            viewParams.cookieControlsShown = false;
        }
        viewParams.onUiClosingCallback = () -> {
            // |this| may have already been destroyed by the time this is called.
            if (mCookieBridge != null) mCookieBridge.onUiClosing();
        };

        mDelegate.initPreviewUiParams(viewParams, this::runAfterDismiss);
        mDelegate.initOfflinePageUiParams(viewParams, this::runAfterDismiss);

        if (!mIsInternalPage && !mDelegate.isShowingOfflinePage() && !mDelegate.isShowingPreview()
                && mDelegate.isInstantAppAvailable(mFullUrl)) {
            final Intent instantAppIntent = mDelegate.getInstantAppIntentForUrl(mFullUrl);
            viewParams.instantAppButtonClickCallback = () -> {
                try {
                    mWindowAndroid.getActivity().get().startActivity(instantAppIntent);
                    RecordUserAction.record("Android.InstantApps.LaunchedFromWebsiteSettingsPopup");
                } catch (ActivityNotFoundException e) {
                    mView.disableInstantAppButton();
                }
            };
            RecordUserAction.record("Android.InstantApps.OpenInstantAppButtonShown");
        } else {
            viewParams.instantAppButtonShown = false;
        }

        mView = mIsV2Enabled ? new PageInfoViewV2(mContext, viewParams)
                             : new PageInfoView(mContext, viewParams);
        if (isSheet(mContext)) mView.setBackgroundColor(Color.WHITE);
        if (mIsV2Enabled) {
            mContainer = new PageInfoContainer(mContext);
            PageInfoContainer.Params containerParams = new PageInfoContainer.Params();
            containerParams.url = viewParams.url;
            containerParams.urlOriginLength = viewParams.urlOriginLength;
            containerParams.truncatedUrl = UrlFormatter.formatUrlForSecurityDisplay(
                    url, SchemeDisplay.OMIT_HTTP_AND_HTTPS);
            containerParams.backButtonClickCallback = this::exitSubpage;
            containerParams.urlTitleClickCallback = mContainer::toggleUrlTruncation;
            containerParams.urlTitleLongClickCallback = viewParams.urlTitleLongClickCallback;
            containerParams.urlTitleShown = viewParams.urlTitleShown;
            mContainer.setParams(containerParams);
            mDelegate.getFavicon(mFullUrl, favicon -> {
                if (favicon != null) {
                    mContainer.setFavicon(favicon);
                } else {
                    mContainer.setFavicon(
                            SettingsUtils.getTintedIcon(mContext, R.drawable.ic_globe_24dp));
                }
            });
            mContainer.showPage(mView, null, null);

            PageInfoViewV2 view2 = (PageInfoViewV2) mView;
            mConnectionController = new PageInfoConnectionController(
                    this, view2.getConnectionRowView(), mWebContents, mDelegate.getVrHandler());
            mPermissionsController = new PageInfoPermissionsController(
                    this, view2.getPermissionsRowView(), mDelegate, mFullUrl);
            mCookiesController = new PageInfoCookiesController(
                    this, view2.getCookiesRowView(), mDelegate, mFullUrl);
        } else {
            mView.showPerformanceInfo(mDelegate.shouldShowPerformanceBadge(mFullUrl));

            CookieControlsView.CookieControlsParams cookieControlsParams =
                    new CookieControlsView.CookieControlsParams();
            cookieControlsParams.onCheckedChangedCallback = (Boolean blockCookies) -> {
                recordAction(blockCookies ? PageInfoAction.PAGE_INFO_COOKIES_BLOCKED_FOR_SITE
                                          : PageInfoAction.PAGE_INFO_COOKIES_ALLOWED_FOR_SITE);
                mCookieBridge.setThirdPartyCookieBlockingEnabledForSite(blockCookies);
            };
            mView.getCookieControlsView().setParams(cookieControlsParams);
        }

        mView.showHttpsImageCompressionInfo(mDelegate.isHttpsImageCompressionApplied());

        // TODO(crbug.com/1040091): Remove when cookie controls are launched.
        boolean showTitle = viewParams.cookieControlsShown;
        mPermissionParamsListBuilder =
                new PermissionParamsListBuilder(mContext, mWindowAndroid, mFullUrl, showTitle, this,
                        mView::setPermissions, mPermissionParamsListBuilderDelegate);
        mNativePageInfoController = PageInfoControllerJni.get().init(this, mWebContents);
        mCookieBridge =
                mDelegate.createCookieControlsBridge(mIsV2Enabled ? mCookiesController : this);
        if (mCookiesController != null) mCookiesController.setCookieControlsBridge(mCookieBridge);

        mWebContentsObserver = new WebContentsObserver(webContents) {
            @Override
            public void navigationEntryCommitted() {
                // If a navigation is committed (e.g. from in-page redirect), the data we're showing
                // is stale so dismiss the dialog.
                mDialog.dismiss(true);
            }

            @Override
            public void wasHidden() {
                // The web contents were hidden (potentially by loading another URL via an intent),
                // so dismiss the dialog).
                mDialog.dismiss(true);
            }

            @Override
            public void destroy() {
                super.destroy();
                // Force the dialog to close immediately in case the destroy was from Chrome
                // quitting.
                PageInfoController.this.destroy();
            }

            @Override
            public void onTopLevelNativeWindowChanged(WindowAndroid windowAndroid) {
                // Destroy the dialog when the associated WebContents is detached from the window.
                if (windowAndroid == null) PageInfoController.this.destroy();
            }
        };

        mDialog = new PageInfoDialog(mContext, mView, mContainer,
                webContents.getViewAndroidDelegate().getContainerView(), isSheet(mContext),
                delegate.getModalDialogManager(), this);
        mDialog.show();
    }

    private void destroy() {
        if (mDialog != null) {
            mDialog.destroy();
            mDialog = null;
        }
        if (mCookieBridge != null) {
            mCookieBridge.destroy();
            mCookieBridge = null;
        }
    }

    /**
     * Whether to show a 'Details' link to the connection info popup.
     */
    private boolean isConnectionDetailsLinkVisible() {
        return mContentPublisher == null && !mDelegate.isShowingOfflinePage()
                && !mDelegate.isShowingPreview() && !mIsInternalPage;
    }

    /**
     * Adds a new row for the given permission.
     *
     * @param name The title of the permission to display to the user.
     * @param type The ContentSettingsType of the permission.
     * @param currentSettingValue The ContentSetting value of the currently selected setting.
     */
    @CalledByNative
    private void addPermissionSection(
            String name, int type, @ContentSettingValues int currentSettingValue) {
        mPermissionParamsListBuilder.addPermissionEntry(name, type, currentSettingValue);
    }

    /**
     * Update the permissions view based on the contents of mDisplayedPermissions.
     */
    @CalledByNative
    private void updatePermissionDisplay() {
        assert (mPermissionParamsListBuilder != null);
        PageInfoView.PermissionParams params = mPermissionParamsListBuilder.build();
        if (mIsV2Enabled) {
            mPermissionsController.setPermissions(params);
        } else {
            mView.setPermissions(params);
        }
    }

    /**
     * Sets the connection security summary and detailed description strings. These strings may be
     * overridden based on the state of the Android UI.
     */
    @CalledByNative
    private void setSecurityDescription(String summary, String details) {
        ConnectionInfoParams connectionInfoParams = new ConnectionInfoParams();

        // Display the appropriate connection message.
        SpannableStringBuilder messageBuilder = new SpannableStringBuilder();
        assert mContext != null;
        if (mContentPublisher != null) {
            messageBuilder.append(
                    mContext.getString(R.string.page_info_domain_hidden, mContentPublisher));
        } else if (mDelegate.isShowingPreview() && mDelegate.isPreviewPageInsecure()) {
            connectionInfoParams.summary = summary;
        } else if (mDelegate.getOfflinePageConnectionMessage() != null) {
            messageBuilder.append(mDelegate.getOfflinePageConnectionMessage());
        } else {
            if (!summary.isEmpty()) {
                connectionInfoParams.summary = summary;
            }
            messageBuilder.append(details);
        }

        if (isConnectionDetailsLinkVisible() && messageBuilder.length() > 0) {
            messageBuilder.append(" ");
            SpannableString detailsText =
                    new SpannableString(mContext.getString(R.string.details_link));
            final ForegroundColorSpan blueSpan =
                    new ForegroundColorSpan(ApiCompatibilityUtils.getColor(
                            mContext.getResources(), R.color.default_text_color_link));
            detailsText.setSpan(
                    blueSpan, 0, detailsText.length(), Spannable.SPAN_INCLUSIVE_EXCLUSIVE);
            messageBuilder.append(detailsText);
        }

        // When a preview is being shown for a secure page, the security message is not shown. Thus,
        // messageBuilder maybe empty.
        if (messageBuilder.length() > 0) {
            connectionInfoParams.message = messageBuilder;
        }
        if (isConnectionDetailsLinkVisible()) {
            connectionInfoParams.clickCallback = () -> {
                runAfterDismiss(() -> {
                    if (!mWebContents.isDestroyed()) {
                        recordAction(PageInfoAction.PAGE_INFO_SECURITY_DETAILS_OPENED);
                        ConnectionInfoView.show(mContext, mWebContents,
                                mDelegate.getModalDialogManager(), mDelegate.getVrHandler());
                    }
                });
            };
        }

        if (mIsV2Enabled) {
            mConnectionController.setConnectionInfo(connectionInfoParams);
        } else {
            mView.setConnectionInfo(connectionInfoParams);
        }
    }

    @Override
    public void onSystemSettingsActivityRequired(Intent intentOverride) {
        runAfterDismiss(() -> {
            Intent settingsIntent;
            if (intentOverride != null) {
                settingsIntent = intentOverride;
            } else {
                settingsIntent = new Intent(Settings.ACTION_APPLICATION_DETAILS_SETTINGS);
                settingsIntent.setData(Uri.parse("package:" + mContext.getPackageName()));
            }
            settingsIntent.setFlags(Intent.FLAG_ACTIVITY_NEW_TASK);
            mContext.startActivity(settingsIntent);
        });
    }

    /**
     * Dismiss the popup, and then run a task after the animation has completed (if there is one).
     */
    private void runAfterDismiss(Runnable task) {
        mPendingRunAfterDismissTask = task;
        mDialog.dismiss(true);
    }

    @Override
    public void onClick(PropertyModel model, @ButtonType int buttonType) {}

    @Override
    public void onDismiss(PropertyModel model, @DialogDismissalCause int dismissalCause) {
        assert mNativePageInfoController != 0;
        if (mPendingRunAfterDismissTask != null) {
            mPendingRunAfterDismissTask.run();
            mPendingRunAfterDismissTask = null;
        }
        mWebContentsObserver.destroy();
        mWebContentsObserver = null;
        PageInfoControllerJni.get().destroy(mNativePageInfoController, PageInfoController.this);
        mNativePageInfoController = 0;
        mContext = null;
    }

    @Override
    public void recordAction(@PageInfoAction int action) {
        if (mNativePageInfoController != 0) {
            PageInfoControllerJni.get().recordPageInfoAction(
                    mNativePageInfoController, PageInfoController.this, action);
        }
    }

    @Override
    public void refreshPermissions() {
        mPermissionParamsListBuilder.clearPermissionEntries();
        if (mNativePageInfoController != 0) {
            PageInfoControllerJni.get().updatePermissions(
                    mNativePageInfoController, PageInfoController.this);
        }
    }

    private boolean isSheet(Context context) {
        return !DeviceFormFactor.isNonMultiDisplayContextOnTablet(context)
                && (mDelegate.getVrHandler() == null || !mDelegate.getVrHandler().isInVr());
    }

    @VisibleForTesting
    public View getPageInfoViewForTesting() {
        if (mContainer != null) return mContainer;
        return mView;
    }

    /**
     * Shows a PageInfo dialog for the provided WebContents. The popup adds itself to the view
     * hierarchy which owns the reference while it's visible.
     *
     * @param activity The activity that is used for launching a dialog.
     * @param webContents The web contents for which to show Website information. This
     *            information is retrieved for the visible entry.
     * @param contentPublisher The name of the publisher of the content.
     * @param source Determines the source that triggered the popup.
     * @param delegate The PageInfoControllerDelegate used to provide embedder-specific info.
     */
    public static void show(final Activity activity, WebContents webContents,
            final String contentPublisher, @OpenedFromSource int source,
            PageInfoControllerDelegate delegate,
            PermissionParamsListBuilderDelegate permissionParamsListBuilderDelegate) {
        // If the activity's decor view is not attached to window, we don't show the dialog because
        // the window manager might have revoked the window token for this activity. See
        // https://crbug.com/921450.
        Window window = activity.getWindow();
        if (window == null || !ViewCompat.isAttachedToWindow(window.getDecorView())) return;

        if (source == OpenedFromSource.MENU) {
            RecordUserAction.record("MobileWebsiteSettingsOpenedFromMenu");
        } else if (source == OpenedFromSource.TOOLBAR) {
            RecordUserAction.record("MobileWebsiteSettingsOpenedFromToolbar");
        } else if (source == OpenedFromSource.VR) {
            RecordUserAction.record("MobileWebsiteSettingsOpenedFromVR");
        } else {
            assert false : "Invalid source passed";
        }

        sLastPageInfoControllerForTesting = new WeakReference<>(new PageInfoController(webContents,
                SecurityStateModel.getSecurityLevelForWebContents(webContents), contentPublisher,
                delegate, permissionParamsListBuilderDelegate));
    }

    @VisibleForTesting(otherwise = VisibleForTesting.PACKAGE_PRIVATE)
    public static PageInfoController getLastPageInfoControllerForTesting() {
        return sLastPageInfoControllerForTesting != null ? sLastPageInfoControllerForTesting.get()
                                                         : null;
    }

    @Override
    public void onCookieBlockingStatusChanged(
            @CookieControlsStatus int status, @CookieControlsEnforcement int enforcement) {
        assert !mIsV2Enabled;
        mView.getCookieControlsView().setCookieBlockingStatus(
                status, enforcement != CookieControlsEnforcement.NO_ENFORCEMENT);
    }

    @Override
    public void onCookiesCountChanged(int allowedCookies, int blockedCookies) {
        assert !mIsV2Enabled;
        mView.getCookieControlsView().setBlockedCookiesCount(blockedCookies);
    }

    @NativeMethods
    interface Natives {
        long init(PageInfoController controller, WebContents webContents);
        void destroy(long nativePageInfoControllerAndroid, PageInfoController caller);
        void recordPageInfoAction(
                long nativePageInfoControllerAndroid, PageInfoController caller, int action);
        void updatePermissions(long nativePageInfoControllerAndroid, PageInfoController caller);
    }

    @Override
    public BrowserContextHandle getBrowserContext() {
        return mDelegate.getBrowserContext();
    }

    /**
     * Launches a subpage for the specified controller.
     */
    @Override
    public void launchSubpage(PageInfoSubpageController controller) {
        if (mSubpageController != null) return;
        mSubpageController = controller;
        CharSequence title = mSubpageController.getSubpageTitle();
        View subview = mSubpageController.createViewForSubpage(mContainer);
        mContainer.showPage(subview, title, null);
    }

    /**
     * Exits the subpage of the current controller.
     */
    @Override
    public void exitSubpage() {
        if (mSubpageController == null) return;
        mContainer.showPage(mView, null, () -> {
            mSubpageController.onSubpageRemoved();
            mSubpageController = null;
        });
    }
}
