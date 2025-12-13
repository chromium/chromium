// Copyright 2013 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.components.page_info;

import static org.chromium.build.NullUtil.assertNonNull;
import static org.chromium.build.NullUtil.assumeNonNull;

import android.app.Activity;
import android.content.Context;
import android.content.Intent;
import android.graphics.Color;
import android.net.Uri;
import android.provider.Settings;
import android.text.Spannable;
import android.text.SpannableStringBuilder;
import android.text.style.TextAppearanceSpan;
import android.view.View;
import android.view.Window;

import androidx.annotation.GravityInt;
import androidx.annotation.IntDef;
import androidx.annotation.VisibleForTesting;
import androidx.core.view.ViewCompat;

import org.jni_zero.CalledByNative;
import org.jni_zero.NativeMethods;

import org.chromium.base.metrics.RecordUserAction;
import org.chromium.build.annotations.NullMarked;
import org.chromium.build.annotations.Nullable;
import org.chromium.components.browser_ui.settings.SettingsUtils;
import org.chromium.components.content_settings.ContentSettingsType;
import org.chromium.components.dom_distiller.core.DomDistillerUrlUtils;
import org.chromium.components.embedder_support.util.UrlUtilities;
import org.chromium.components.omnibox.AutocompleteSchemeClassifier;
import org.chromium.components.omnibox.OmniboxUrlEmphasizer;
import org.chromium.components.security_state.ConnectionSecurityLevel;
import org.chromium.components.security_state.SecurityStateModel;
import org.chromium.components.url_formatter.UrlFormatter;
import org.chromium.content_public.browser.BrowserContextHandle;
import org.chromium.content_public.browser.LoadCommittedDetails;
import org.chromium.content_public.browser.Visibility;
import org.chromium.content_public.browser.WebContents;
import org.chromium.content_public.browser.WebContentsObserver;
import org.chromium.ui.base.Clipboard;
import org.chromium.ui.base.DeviceFormFactor;
import org.chromium.ui.base.ViewAndroidDelegate;
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
import java.util.ArrayDeque;
import java.util.ArrayList;
import java.util.Arrays;
import java.util.Collection;
import java.util.Deque;
import java.util.List;

/** Java side of Android implementation of the page info UI. */
@NullMarked
public class PageInfoController
        implements PageInfoMainController,
                ModalDialogProperties.Controller,
                SystemSettingsActivityRequiredListener {
    @IntDef({
        OpenedFromSource.MENU,
        OpenedFromSource.TOOLBAR,
        OpenedFromSource.VR,
        OpenedFromSource.WEBAPK_SNACKBAR,
        OpenedFromSource.PERMISSION_PROMPT
    })
    @Retention(RetentionPolicy.SOURCE)
    public @interface OpenedFromSource {
        int MENU = 1;
        int TOOLBAR = 2;
        int VR = 3;
        int WEBAPK_SNACKBAR = 4;
        int PERMISSION_PROMPT = 5;
    }

    @ContentSettingsType.EnumType
    public static final int NO_HIGHLIGHTED_PERMISSION = ContentSettingsType.DEFAULT;

    private final Context mContext;
    private final WindowAndroid mWindowAndroid;
    private final WebContents mWebContents;
    private final PageInfoControllerDelegate mDelegate;

    // A pointer to the C++ object for this UI.
    private long mNativePageInfoController;

    // The main PageInfo view.
    private final PageInfoView mView;

    // The view inside the popup.
    private final PageInfoContainer mContainer;

    // The dialog the view is placed in.
    private @Nullable PageInfoDialog mDialog;

    // The full URL from the URL bar, which is copied to the user's clipboard when they select 'Copy
    // URL'.
    private final GURL mFullUrl;

    // Whether or not this page is an internal chrome page (e.g. the
    // chrome://settings page).
    private final boolean mIsInternalPage;

    // The security level of the page (a valid ConnectionSecurityLevel).
    private final @ConnectionSecurityLevel int mSecurityLevel;

    // Observer for dismissing dialog if web contents get destroyed, navigate etc.
    private final WebContentsObserver mWebContentsObserver;

    // A task that should be run once the page info popup is animated out and dismissed. Null if no
    // task is pending.
    private @Nullable Runnable mPendingRunAfterDismissTask;

    // Reference to last created PageInfoController for testing.
    private static @Nullable WeakReference<PageInfoController> sLastPageInfoControllerForTesting;

    // Used to show Site settings from Page Info UI.
    private final PermissionParamsListBuilder mPermissionParamsListBuilder;

    // The current page info subpage controller stack, if any.
    private final Deque<PageInfoSubpageController> mCurrentSubpageControllers = new ArrayDeque<>();

    // The controller for the connection section of the page info.
    private final PageInfoConnectionController mConnectionController;
    private final PageInfoConnectionSecurityController mConnectionSecurityController;

    // The controller for the permissions section of the page info.
    private final PageInfoPermissionsController mPermissionsController;

    // The controller for the cookies section of the page info.
    private final @Nullable PageInfoCookiesController mCookiesController;

    // All subpage controllers.
    private final Collection<PageInfoSubpageController> mSubpageControllers;

    /**
     * Creates the PageInfoController, but does not display it. Also initializes the corresponding
     * C++ object and saves a pointer to it.
     *
     * @param webContents The WebContents showing the page that the PageInfo is about.
     * @param securityLevel The security level of the page being shown.
     * @param publisher The name of the content publisher, if any.
     * @param delegate The PageInfoControllerDelegate used to provide embedder-specific info.
     * @param pageInfoHighlight Providing the highlight row info related to this dialog.
     * @param source Determines the source that triggered the popup.
     * @param dialogPosition The position of the dialog, either TOP or BOTTOM.
     */
    @VisibleForTesting(otherwise = VisibleForTesting.PROTECTED)
    public PageInfoController(
            WebContents webContents,
            @ConnectionSecurityLevel int securityLevel,
            @Nullable String publisher,
            PageInfoControllerDelegate delegate,
            PageInfoHighlight pageInfoHighlight,
            @OpenedFromSource int source,
            @GravityInt int dialogPosition,
            // TODO(crbug.com/458351800): Remove this variable if the Loud Clapper experiment cannot
            // be launched.
            boolean openPermissionsSubpage) {
        mWebContents = webContents;
        mSecurityLevel = securityLevel;
        mDelegate = delegate;
        mWindowAndroid = assumeNonNull(webContents.getTopLevelNativeWindow());
        mContext = assertNonNull(mWindowAndroid.getContext().get());
        mSubpageControllers = new ArrayList<>();
        // Work out the URL and connection message and status visibility.
        // TODO(crbug.com/40663204): dedupe the
        // DomDistillerUrlUtils#getOriginalUrlFromDistillerUrl()
        // calls.
        String url =
                mDelegate.isShowingOfflinePage()
                        ? mDelegate.getOfflinePageUrl()
                        : DomDistillerUrlUtils.getOriginalUrlFromDistillerUrl(
                                        webContents.getVisibleUrl())
                                .getSpec();

        // This can happen if an invalid chrome-distiller:// url was entered.
        if (url == null) url = "";

        mFullUrl = new GURL(url);
        mIsInternalPage = UrlUtilities.isInternalScheme(mFullUrl);

        String displayUrl =
                UrlFormatter.formatUrlForDisplayOmitUsernamePassword(mFullUrl.getSpec());
        if (mDelegate.isShowingOfflinePage()) {
            displayUrl = UrlUtilities.stripScheme(mFullUrl.getSpec());
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
                        new TextAppearanceSpan(mContext, R.style.TextAppearance_MediumStyle),
                        0,
                        emphasizeResponse.schemeLength,
                        Spannable.SPAN_EXCLUSIVE_INCLUSIVE);
            }
        }

        // Setup Container.
        mContainer = new PageInfoContainer(mContext);
        boolean useDarkText = !ColorUtils.inNightMode(mContext);
        OmniboxUrlEmphasizer.emphasizeUrl(
                displayUrlBuilder,
                mContext,
                autocompleteSchemeClassifier,
                mSecurityLevel,
                useDarkText,
                /* emphasizeScheme= */ true);
        int urlOriginLength =
                OmniboxUrlEmphasizer.getOriginEndIndex(
                        displayUrlBuilder.toString(), autocompleteSchemeClassifier);
        autocompleteSchemeClassifier.destroy();
        String truncatedUrl =
                UrlFormatter.formatUrlForDisplayOmitSchemePathAndTrivialSubdomains(mFullUrl);
        PageInfoContainer.Params containerParams =
                new PageInfoContainer.Params(
                        /* url= */ displayUrlBuilder,
                        /* urlOriginLength= */ urlOriginLength,
                        /* truncatedUrl= */ truncatedUrl,
                        /* backButtonClickCallback= */ this::exitSubpage,
                        /* urlTitleClickCallback= */ mContainer::toggleUrlTruncation,

                        // Long press the url text to copy it to the clipboard.
                        /* urlTitleLongClickCallback= */ () ->
                                Clipboard.getInstance().copyUrlToClipboard(mFullUrl),
                        // Show close button for tablets and when accessibility is enabled to make
                        // it easier to close the UI.
                        /* showCloseButton= */ !isSheet() || mDelegate.isAccessibilityEnabled(),
                        /* closeButtonClickCallback= */ this::dismiss);
        mContainer.setParams(containerParams);

        // Setup View.
        PageInfoView.Params viewParams = new PageInfoView.Params();
        mDelegate.initOfflinePageUiParams(viewParams, this::runAfterDismiss);
        viewParams.httpsImageCompressionMessageShown = mDelegate.isHttpsImageCompressionApplied();
        mView = new PageInfoView(mContext, viewParams);
        if (isSheet()) mView.setBackgroundColor(Color.WHITE);
        mDelegate.getFavicon(
                mFullUrl,
                favicon -> {
                    // Return early if PageInfo has been dismissed.
                    if (mDialog == null) return;

                    if (favicon != null) {
                        mContainer.setFavicon(favicon);
                    } else {
                        mContainer.setFavicon(
                                SettingsUtils.getTintedIcon(mContext, R.drawable.ic_globe_24dp));
                    }
                });

        // Create Subcontrollers.
        mConnectionController =
                new PageInfoConnectionController(
                        this,
                        mView.getConnectionRowView(),
                        mWebContents,
                        mDelegate,
                        publisher,
                        mIsInternalPage);
        mSubpageControllers.add(mConnectionController);
        mConnectionSecurityController =
                new PageInfoConnectionSecurityController(
                        this,
                        mView.getConnectionSecurityView(),
                        mView.getConnectionRowView(),
                        mWebContents);
        mSubpageControllers.add(mConnectionSecurityController);
        mPermissionsController =
                new PageInfoPermissionsController(
                        this,
                        mView.getPermissionsRowView(),
                        mDelegate,
                        mWebContents,
                        pageInfoHighlight.getHighlightedPermission());
        mSubpageControllers.add(mPermissionsController);
        mCookiesController =
                new PageInfoCookiesController(this, mView.getCookiesRowView(), mDelegate);
        mSubpageControllers.add(mCookiesController);
        mContainer.showPage(mView, null, null);

        // TODO(crbug.com/40746014): Setup forget this site button after history delete is
        // implemented.
        // setupForgetSiteButton(mView.getForgetSiteButton());

        mSubpageControllers.addAll(mDelegate.createAdditionalRowViews(this, mView.getRowWrapper()));

        mPermissionParamsListBuilder = new PermissionParamsListBuilder(mContext, mWindowAndroid);
        mNativePageInfoController = PageInfoControllerJni.get().init(this, mWebContents);

        ViewAndroidDelegate viewAndroidDelegte =
                assumeNonNull(webContents.getViewAndroidDelegate());
        PageInfoDialog dialog =
                new PageInfoDialog(
                        mContext,
                        mContainer,
                        assumeNonNull(viewAndroidDelegte.getContainerView()),
                        isSheet(),
                        delegate.getModalDialogManager(),
                        this,
                        dialogPosition);

        mWebContentsObserver =
                new WebContentsObserver(webContents) {
                    @Override
                    public void navigationEntryCommitted(LoadCommittedDetails details) {
                        // If a navigation is committed (e.g. from in-page redirect), the data we're
                        // showing is stale so dismiss the dialog.
                        dialog.dismiss(true);
                    }

                    @Override
                    public void onVisibilityChanged(@Visibility int visibility) {
                        // The web contents were hidden or occluded (potentially by loading another
                        // URL via an intent), so dismiss the dialog).
                        if (visibility != Visibility.VISIBLE) {
                            dialog.dismiss(true);
                        }
                    }

                    @Override
                    public void webContentsDestroyed() {
                        PageInfoController.this.destroy();
                    }

                    @Override
                    public void onTopLevelNativeWindowChanged(
                            @Nullable WindowAndroid windowAndroid) {
                        // Destroy the dialog when the associated WebContents is detached from the
                        // window.
                        if (windowAndroid == null) PageInfoController.this.destroy();
                    }
                };

        mDialog = dialog;
        if (mNativePageInfoController != 0) {
            dialog.show();
        }

        if (openPermissionsSubpage) {
            launchSubpage(mPermissionsController);
        }
    }

    private void destroy() {
        if (mDialog == null) {
            return;
        }
        mWebContentsObserver.observe(null);
        mDialog.destroy();
        mDialog = null;
        if (mCookiesController != null) {
            mCookiesController.destroy();
        }
        if (mConnectionSecurityController != null) {
            mConnectionSecurityController.destroy();
        }
    }

    /**
     * Adds a new row for the given permission.
     *
     * @param name The title of the permission to display to the user.
     * @param nameMidSentence The title of the permission to display to the user when used
     *     mid-sentence.
     * @param type The ContentSettingsType of the permission.
     * @param allowed Whether the permission is allowed.
     * @param requested Whether the permission is currently being requested.
     */
    @CalledByNative
    private void addPermissionSection(
            String name, String nameMidSentence, int type, boolean allowed, boolean requested) {
        mPermissionParamsListBuilder.addPermissionEntry(
                name, nameMidSentence, type, allowed, requested);
    }

    /** Update the permissions view based on the contents of mDisplayedPermissions. */
    @CalledByNative
    private void updatePermissionDisplay() {
        assert (mPermissionParamsListBuilder != null);
        List<PageInfoPermissionsController.PermissionObject> params =
                mPermissionParamsListBuilder.build();
        mPermissionsController.setPermissions(params);
    }

    /**
     * Sets the connection security summary and detailed description strings. These strings may be
     * overridden based on the state of the Android UI.
     */
    @CalledByNative
    private void setSecurityDescription(String summary, String details) {
        mConnectionController.setSecurityDescription(summary, details);
    }

    /**
     * Creates a button in the PageInfo UI that displays only a summary line about connection
     * security; when tapped the button opens a subpage that displays the full connection security
     * info.
     */
    @CalledByNative
    private void showOpenSecurityPageButton(String summary) {
        mConnectionSecurityController.showSecurityPageButton(summary);
    }

    /** Displays the full connection security info in the PageInfo UI. */
    @CalledByNative
    private void showConnectionSecurityInfo() {
        mConnectionSecurityController.showSecurityInfo();
    }

    /** Updates the Topic view if present. */
    @CalledByNative
    private void setAdPersonalizationInfo(boolean hasJoinedUserToInterestGroup, String[] topics) {
        // This logic is a little weird. On Android we already have separate controllers for most
        // PageInfo components and they usually update themselves. On Desktop we still have one big
        // controller. Here we are reusing Desktop controller to update the Android component.
        // In the future the Desktop logic will hopefully be split as well and then we can remove
        // this logic here.
        for (PageInfoSubpageController controller : mSubpageControllers) {
            if (controller instanceof PageInfoAdPersonalizationController) {
                ((PageInfoAdPersonalizationController) controller)
                        .setAdPersonalizationInfo(
                                hasJoinedUserToInterestGroup, Arrays.asList(topics));
            }
        }
    }

    @Override
    public void onSystemSettingsActivityRequired(Intent intentOverride) {
        runAfterDismiss(
                () -> {
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
    @VisibleForTesting
    public void runAfterDismiss(Runnable task) {
        assert mPendingRunAfterDismissTask == null;
        mPendingRunAfterDismissTask = task;
        dismiss();
    }

    @Override
    public void onClick(PropertyModel model, @ButtonType int buttonType) {}

    @Override
    public void onDismiss(PropertyModel model, @DialogDismissalCause int dismissalCause) {
        assert mNativePageInfoController != 0;
        while (!mCurrentSubpageControllers.isEmpty()) {
            PageInfoSubpageController currentSubpageController =
                    mCurrentSubpageControllers.removeFirst();
            currentSubpageController.onSubpageRemoved();
        }

        destroy();

        PageInfoControllerJni.get().destroy(mNativePageInfoController);
        mNativePageInfoController = 0;
        if (mPendingRunAfterDismissTask != null) {
            mPendingRunAfterDismissTask.run();
        }
    }

    @Override
    public void recordAction(@PageInfoAction int action) {
        assert mNativePageInfoController != 0;
        if (mNativePageInfoController != 0) {
            PageInfoControllerJni.get().recordPageInfoAction(mNativePageInfoController, action);
        }
    }

    @Override
    public void refreshPermissions() {
        mPermissionParamsListBuilder.clearPermissionEntries();
        if (mNativePageInfoController != 0) {
            PageInfoControllerJni.get().updatePermissions(mNativePageInfoController);
        }
    }

    @Override
    public @ConnectionSecurityLevel int getSecurityLevel() {
        return mSecurityLevel;
    }

    private boolean isSheet() {
        return !DeviceFormFactor.isNonMultiDisplayContextOnTablet(mContext);
    }

    @VisibleForTesting
    public View getPageInfoView() {
        return mContainer;
    }

    @VisibleForTesting
    public @Nullable PageInfoCookiesController getCookiesController() {
        return mCookiesController;
    }

    @VisibleForTesting
    public boolean isDialogShowing() {
        return mDialog != null;
    }

    /**
     * Shows a PageInfo dialog for the provided WebContents. The popup adds itself to the view
     * hierarchy which owns the reference while it's visible.
     *
     * @param activity The activity that is used for launching a dialog.
     * @param webContents The web contents for which to show Website information. This information
     *     is retrieved for the visible entry.
     * @param contentPublisher The name of the publisher of the content.
     * @param source Determines the source that triggered the popup.
     * @param delegate The PageInfoControllerDelegate used to provide embedder-specific info.
     * @param pageInfoHighlight Providing the highlight row info related to this dialog.
     * @param dialogPosition The position of the dialog.
     * @param openPermissionsSubpage Whether to open the permissions subpage when the dialog is
     *     shown.
     */
    public static void show(
            final Activity activity,
            WebContents webContents,
            final @Nullable String contentPublisher,
            @OpenedFromSource int source,
            PageInfoControllerDelegate delegate,
            PageInfoHighlight pageInfoHighlight,
            @GravityInt int dialogPosition,
            // TODO(crbug.com/458351800): Create a config class and move parameters into it.
            boolean openPermissionsSubpage) {
        // Don't show the dialog if this tab doesn't have an activity. See https://crbug.com/1267383
        if (activity == null) return;
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
        } else if (source == OpenedFromSource.WEBAPK_SNACKBAR) {
            RecordUserAction.record("MobileWebsiteSettingsOpenedFromWebApkSnackbar");
        } else if (source == OpenedFromSource.PERMISSION_PROMPT) {
            RecordUserAction.record("MobileWebsiteSettingsOpenedFromPermissionPrompt");
        } else {
            assert false : "Invalid source passed";
        }

        sLastPageInfoControllerForTesting =
                new WeakReference<>(
                        new PageInfoController(
                                webContents,
                                SecurityStateModel.getSecurityLevelForWebContents(webContents),
                                contentPublisher,
                                delegate,
                                pageInfoHighlight,
                                source,
                                dialogPosition,
                                openPermissionsSubpage));
    }

    @VisibleForTesting
    public static @Nullable PageInfoController getLastPageInfoController() {
        return sLastPageInfoControllerForTesting != null
                ? sLastPageInfoControllerForTesting.get()
                : null;
    }

    @NativeMethods
    interface Natives {
        long init(PageInfoController controller, WebContents webContents);

        void destroy(long nativePageInfoControllerAndroid);

        void recordPageInfoAction(long nativePageInfoControllerAndroid, int action);

        void updatePermissions(long nativePageInfoControllerAndroid);
    }

    @Override
    public BrowserContextHandle getBrowserContext() {
        return mDelegate.getBrowserContext();
    }

    @VisibleForTesting
    public PageInfoControllerDelegate getPageInfoControllerDelegate() {
        return mDelegate;
    }

    /** Launches a subpage for the specified controller. */
    @Override
    public void launchSubpage(PageInfoSubpageController controller) {
        if (mContainer.isPageChangeInProgress()) return;
        mCurrentSubpageControllers.addFirst(controller);
        displayCurrentSubpageInStack(/* onPreviousPageRemoved= */ null);
    }

    private void displayCurrentSubpageInStack(@Nullable Runnable onPreviousPageRemoved) {
        assert !mCurrentSubpageControllers.isEmpty();
        PageInfoSubpageController currentSubpageController = mCurrentSubpageControllers.peekFirst();
        CharSequence title = currentSubpageController.getSubpageTitle();
        View subview =
                currentSubpageController.getCurrentSubpageView() != null
                        ? currentSubpageController.getCurrentSubpageView()
                        : currentSubpageController.createViewForSubpage(mContainer);

        if (subview != null) {
            currentSubpageController.updateSubpageIfNeeded();
            mContainer.showPage(subview, title, onPreviousPageRemoved);
        }
    }

    /** Exits the subpage of the current controller. */
    @Override
    public void exitSubpage() {
        if (mCurrentSubpageControllers.isEmpty()) return;
        PageInfoSubpageController previousSubpageController =
                mCurrentSubpageControllers.removeFirst();
        Runnable onPreviousPageRemoved =
                () -> {
                    previousSubpageController.onSubpageRemoved();
                    previousSubpageController.updateRowIfNeeded();
                };

        if (mCurrentSubpageControllers.isEmpty()) {
            mContainer.showPage(mView, null, onPreviousPageRemoved);
        } else {
            displayCurrentSubpageInStack(onPreviousPageRemoved);
        }
    }

    @Override
    public Activity getActivity() {
        return assertNonNull(mWindowAndroid.getActivity().get());
    }

    @Override
    public GURL getURL() {
        return mFullUrl;
    }

    /** Dismiss the page info dialog. */
    @Override
    public void dismiss() {
        if (mDialog != null) {
            mDialog.dismiss(true);
        }
    }
}
