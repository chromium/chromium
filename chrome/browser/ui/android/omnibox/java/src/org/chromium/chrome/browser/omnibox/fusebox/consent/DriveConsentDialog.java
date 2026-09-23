// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.omnibox.fusebox.consent;

import static org.chromium.build.NullUtil.assumeNonNull;

import android.app.Activity;
import android.text.TextUtils;
import android.view.LayoutInflater;
import android.view.View;
import android.view.ViewGroup.LayoutParams;
import android.webkit.JavascriptInterface;
import android.widget.FrameLayout;

import androidx.annotation.CheckResult;
import androidx.annotation.ColorInt;

import org.chromium.base.Callback;
import org.chromium.base.ThreadUtils;
import org.chromium.base.lifetime.LifetimeAssert;
import org.chromium.base.version_info.VersionInfo;
import org.chromium.build.annotations.NullMarked;
import org.chromium.build.annotations.Nullable;
import org.chromium.chrome.browser.content.WebContentsFactory;
import org.chromium.chrome.browser.omnibox.R;
import org.chromium.chrome.browser.omnibox.fusebox.DriveDisclaimerBridge;
import org.chromium.chrome.browser.profiles.Profile;
import org.chromium.components.browser_ui.styles.SemanticColorUtils;
import org.chromium.components.embedder_support.view.ContentView;
import org.chromium.components.origin_matcher.OriginMatcher;
import org.chromium.components.thinwebview.ThinWebView;
import org.chromium.components.thinwebview.ThinWebViewAttachParams;
import org.chromium.components.thinwebview.ThinWebViewConstraints;
import org.chromium.components.thinwebview.ThinWebViewFactory;
import org.chromium.content_public.browser.JavascriptInjector;
import org.chromium.content_public.browser.LoadUrlParams;
import org.chromium.content_public.browser.WebContents;
import org.chromium.content_public.browser.navigation_controller.UserAgentOverrideOption;
import org.chromium.ui.base.IntentRequestTracker;
import org.chromium.ui.base.ViewAndroidDelegate;
import org.chromium.ui.base.WindowAndroid;
import org.chromium.ui.modaldialog.DialogDismissalCause;
import org.chromium.ui.modaldialog.ModalDialogManager;
import org.chromium.ui.modaldialog.ModalDialogManager.ModalDialogType;
import org.chromium.ui.modaldialog.ModalDialogProperties;
import org.chromium.ui.modaldialog.ModalDialogProperties.ButtonType;
import org.chromium.ui.modaldialog.ModalDialogProperties.DialogStyles;
import org.chromium.ui.modelutil.PropertyModel;
import org.chromium.ui.util.ColorUtils;
import org.chromium.ui.widget.LoadingView;

import java.util.List;

/**
 * Modal dialog hosting the Google Drive ConsentKit flow.
 *
 * <p>Owns the {@link WebContents} hosting the ConsentKit page, the {@link ThinWebView} rendering
 * it, and the modal dialog containing both. Protocol decisions are delegated to {@link
 * DriveConsentKitClient}.
 */
@NullMarked
public class DriveConsentDialog
        implements ModalDialogProperties.Controller, DriveConsentKitClient.Delegate {
    private static final String BRIDGE_NAME = "ckUi";
    private static final String CONSENT_ORIGIN = "https://consent.google.com";

    private final Activity mActivity;
    private final ModalDialogManager mModalDialogManager;
    private @Nullable Callback<Boolean> mOnConsentComplete;
    private final boolean mIsDarkMode;
    private final @Nullable LifetimeAssert mLifetimeAssert = LifetimeAssert.create(this);

    private @Nullable WebContents mWebContents;
    private @Nullable ThinWebView mThinWebView;
    private @Nullable DriveConsentKitClient mClient;
    private @Nullable LoadingView mSpinner;
    private @Nullable PropertyModel mModalDialogModel;
    private boolean mDestroyed;
    private boolean mConsentGranted;

    /**
     * Shows the Drive ConsentKit dialog.
     *
     * @param windowAndroid The window hosting the dialog.
     * @param profile The current user profile.
     * @param onConsentComplete Invoked exactly once with whether consent was granted. Invoked
     *     synchronously with {@code false} if the dialog could not be shown.
     * @return The shown {@link DriveConsentDialog}, or null if it could not be shown.
     */
    @CheckResult
    public static @Nullable DriveConsentDialog show(
            WindowAndroid windowAndroid, Profile profile, Callback<Boolean> onConsentComplete) {
        Activity activity = windowAndroid.getActivity().get();
        ModalDialogManager modalDialogManager = windowAndroid.getModalDialogManager();
        IntentRequestTracker tracker = windowAndroid.getIntentRequestTracker();
        if (activity == null
                || activity.isFinishing()
                || activity.isDestroyed()
                || modalDialogManager == null
                || tracker == null) {
            onConsentComplete.onResult(false);
            return null;
        }

        DriveConsentDialog dialog =
                new DriveConsentDialog(activity, modalDialogManager, onConsentComplete);
        if (!dialog.showInternal(windowAndroid, profile)) {
            dialog.destroy();
            return null;
        }
        return dialog;
    }

    DriveConsentDialog(
            Activity activity,
            ModalDialogManager modalDialogManager,
            Callback<Boolean> onConsentComplete) {
        mActivity = activity;
        mModalDialogManager = modalDialogManager;
        mOnConsentComplete = onConsentComplete;
        mIsDarkMode = ColorUtils.inNightMode(activity);
    }

    private boolean showInternal(WindowAndroid windowAndroid, Profile profile) {
        String consentUrl = DriveDisclaimerBridge.getConsentUrl(profile, mIsDarkMode);
        if (TextUtils.isEmpty(consentUrl)) {
            return false;
        }
        // The consentUrl must not diverge from CONSENT_ORIGIN.
        assert consentUrl.equals(CONSENT_ORIGIN)
                        || consentUrl.startsWith(CONSENT_ORIGIN + "/")
                        || consentUrl.startsWith(CONSENT_ORIGIN + "?")
                : "Consent URL origin diverged from the JS bridge allowlist: " + consentUrl;

        // Create WebContents, set up the client, and inject the JS bridge.
        WebContents webContents =
                WebContentsFactory.createWebContents(
                        profile, /* initiallyHidden= */ false, /* initializeRenderer= */ false);
        mWebContents = webContents;
        DriveConsentKitClient client = new DriveConsentKitClient(webContents, profile, this);
        mClient = client;
        if (!injectBridge(webContents, client)) {
            return false;
        }

        // Attach Android view delegates.
        ContentView contentView = ContentView.createContentView(mActivity, webContents);
        webContents.setDelegates(
                VersionInfo.getProductVersion(),
                ViewAndroidDelegate.createBasicDelegate(contentView),
                contentView,
                windowAndroid,
                WebContents.createDefaultInternalsHolder());

        // Create ThinWebView and configure the ConsentKit user agent.
        IntentRequestTracker tracker = assumeNonNull(windowAndroid.getIntentRequestTracker());
        @ColorInt int backgroundColor = SemanticColorUtils.getDefaultBgColor(mActivity);
        ThinWebView thinWebView =
                createThinWebView(webContents, contentView, tracker, backgroundColor);
        mThinWebView = thinWebView;
        DriveDisclaimerBridge.setConsentKitUserAgent(webContents, mIsDarkMode);

        // Build the layout with a loading spinner and display the modal dialog.
        FrameLayout container = buildContentContainer(thinWebView.getView());
        LoadingView spinner = container.findViewById(R.id.drive_consent_spinner);
        spinner.showLoadingUi();
        mSpinner = spinner;

        PropertyModel modalDialogModel = buildModalDialogModel(container);
        mModalDialogModel = modalDialogModel;
        mModalDialogManager.showDialog(modalDialogModel, ModalDialogType.APP);
        contentView.requestFocus();

        // Start loading the page once the view is attached and sized.
        LoadUrlParams params = new LoadUrlParams(consentUrl);
        params.setOverrideUserAgent(UserAgentOverrideOption.TRUE);
        webContents.getNavigationController().loadUrl(params);
        return true;
    }

    private boolean injectBridge(WebContents webContents, DriveConsentKitClient client) {
        JavascriptInjector injector = JavascriptInjector.fromWebContents(webContents);
        if (injector == null) {
            return false;
        }

        OriginMatcher matcher = new OriginMatcher();
        try {
            List<String> badRules = matcher.setRuleList(List.of(CONSENT_ORIGIN));
            if (!badRules.isEmpty()) {
                return false;
            }
            injector.addPossiblyUnsafeInterfaceToOrigins(
                    new DriveConsentJsBridge(client),
                    BRIDGE_NAME,
                    JavascriptInterface.class,
                    matcher);
            return true;
        } finally {
            matcher.destroy();
        }
    }

    private ThinWebView createThinWebView(
            WebContents webContents,
            ContentView contentView,
            IntentRequestTracker tracker,
            @ColorInt int backgroundColor) {
        ThinWebViewConstraints constraints = new ThinWebViewConstraints();
        constraints.supportsOpacity = true;
        constraints.backgroundColor = backgroundColor;
        ThinWebView thinWebView =
                ThinWebViewFactory.create(
                        mActivity, constraints, tracker, /* enablePermissionRequests= */ false);
        thinWebView.attachWebContents(
                webContents,
                contentView,
                new ThinWebViewAttachParams.Builder().setSupportTheming(true).build());
        return thinWebView;
    }

    private FrameLayout buildContentContainer(View thinWebView) {
        FrameLayout container =
                (FrameLayout)
                        LayoutInflater.from(mActivity)
                                .inflate(R.layout.drive_consent_dialog, /* root= */ null);
        container.addView(
                thinWebView,
                /* index= */ 0,
                new FrameLayout.LayoutParams(LayoutParams.MATCH_PARENT, LayoutParams.MATCH_PARENT));
        return container;
    }

    private PropertyModel buildModalDialogModel(View customView) {
        return new PropertyModel.Builder(ModalDialogProperties.ALL_KEYS)
                .with(ModalDialogProperties.CONTROLLER, this)
                .with(ModalDialogProperties.CUSTOM_VIEW, customView)
                .with(ModalDialogProperties.CANCEL_ON_TOUCH_OUTSIDE, false)
                .with(ModalDialogProperties.DIALOG_STYLES, DialogStyles.FULLSCREEN_DIALOG)
                .build();
    }

    // ModalDialogProperties.Controller implementation.

    @Override
    public void onClick(PropertyModel model, @ButtonType int buttonType) {}

    @Override
    public void onDismiss(PropertyModel model, @DialogDismissalCause int dismissalCause) {
        mModalDialogModel = null;
        finish(/* granted= */ false);
    }

    // DriveConsentKitClient.Delegate implementation.

    @Override
    public void onPageFirstPaint() {
        if (mSpinner != null) {
            mSpinner.hideLoadingUi();
        }
    }

    @Override
    public void onLoadFailed() {
        finish(/* granted= */ false);
    }

    @Override
    public void onConsentComplete(boolean granted) {
        finish(granted);
    }

    private void finish(boolean granted) {
        if (mDestroyed) {
            return;
        }
        mConsentGranted = granted;
        destroy();
    }

    private void notifyConsentComplete(boolean granted) {
        if (mOnConsentComplete != null) {
            Callback<Boolean> callback = mOnConsentComplete;
            mOnConsentComplete = null;
            callback.onResult(granted);
        }
    }

    /** Dismisses the dialog and releases the web contents and every view built around it. */
    public void destroy() {
        if (mDestroyed) {
            return;
        }
        mDestroyed = true;
        LifetimeAssert.destroy(mLifetimeAssert);

        if (mModalDialogModel != null) {
            mModalDialogManager.dismissDialog(
                    mModalDialogModel, DialogDismissalCause.ACTION_ON_CONTENT);
            mModalDialogModel = null;
        }

        if (mClient != null) {
            mClient.destroy();
            mClient = null;
        }
        if (mThinWebView != null) {
            mThinWebView.destroy();
            mThinWebView = null;
        }
        WebContents webContents = mWebContents;
        mWebContents = null;
        if (webContents != null) {
            // Posted because this may run inside a renderer callback, where destroying
            // WebContents synchronously is unsafe.
            ThreadUtils.postOnUiThread(webContents::destroy);
        }
        if (mSpinner != null) {
            mSpinner.destroy();
            mSpinner = null;
        }

        notifyConsentComplete(mConsentGranted);
    }

    void setSpinnerForTesting(LoadingView spinner) {
        mSpinner = spinner;
    }

    void setModalDialogModelForTesting(PropertyModel model) {
        mModalDialogModel = model;
    }

    boolean isDestroyedForTesting() {
        return mDestroyed;
    }
}
