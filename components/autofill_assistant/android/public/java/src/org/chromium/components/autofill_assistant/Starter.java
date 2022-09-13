// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.components.autofill_assistant;

import android.app.Activity;

import androidx.annotation.Nullable;
import androidx.annotation.VisibleForTesting;

import org.chromium.base.UserData;
import org.chromium.base.annotations.CalledByNative;
import org.chromium.base.annotations.JNINamespace;
import org.chromium.base.annotations.NativeMethods;
import org.chromium.base.supplier.Supplier;
import org.chromium.components.autofill_assistant.metrics.FeatureModuleInstallation;
import org.chromium.content_public.browser.WebContents;
import org.chromium.ui.base.WindowAndroid;

import java.util.HashMap;
import java.util.Map;

/**
 * Connects to a native starter for which it acts as a platform delegate, providing the necessary
 * dependencies to start autofill-assistant flows.
 */
@JNINamespace("autofill_assistant")
public class Starter implements AssistantTabObserver, UserData {
    /** A supplier for the activity of the tab that this starter tracks. */
    private final Supplier<Activity> mActivitySupplier;

    private final AssistantStaticDependencies mStaticDependencies;
    private final AssistantIsGsaFunction mIsGsaFunction;
    private final AssistantModuleInstallUi.Provider mModuleInstallUiProvider;

    /**
     * The WebContents associated with the tab which this starter is monitoring, unless detached.
     */
    private @Nullable WebContents mWebContents;

    /**
     * The pointer to the native C++ starter. Can be 0 while waiting for the web contents to be
     * available.
     */
    private long mNativeStarter;

    /** The dependencies required to start a flow. */
    @Nullable
    private AssistantDependencies mDependencies;

    /** A helper to show and hide the onboarding. */
    @Nullable
    private AssistantOnboardingHelper mOnboardingHelper;

    /**
     * A field to temporarily hold a startup request's trigger context while the tab is
     * being initialized.
     */
    @Nullable
    private TriggerContext mPendingTriggerContext;

    /**
     * Constructs a java-side starter.
     *
     * This will wait for dependencies to become available and then create the native-side starter.
     * NOTE: The caller must register the Starter as a {@link AssistantTabObserver} so it can keep
     * track of changes.
     */
    public Starter(Supplier<Activity> activitySupplier, @Nullable WebContents webContents,
            AssistantStaticDependencies staticDependencies, AssistantIsGsaFunction isGsaFunction,
            AssistantModuleInstallUi.Provider moduleInstallUiProvider) {
        mActivitySupplier = activitySupplier;
        mStaticDependencies = staticDependencies;
        mIsGsaFunction = isGsaFunction;
        mModuleInstallUiProvider = moduleInstallUiProvider;
        detectWebContentsChange(webContents);
    }

    @Override
    public void destroy() {
        safeNativeDetach();
    }

    /**
     * Attempts to start a new flow for {@code triggerContext}. This will wait for the necessary
     * dependencies (such as the web-contents) to be available before attempting the startup. New
     * calls to this method will supersede earlier invocations, potentially cancelling the previous
     * flow (as there can be only one flow maximum per tab).
     */
    public void start(TriggerContext triggerContext) {
        // Starter is not yet ready, we need to wait for the web-contents to be available.
        if (mNativeStarter == 0) {
            mPendingTriggerContext = triggerContext;
            return;
        }

        StarterJni.get().start(mNativeStarter, Starter.this, triggerContext.getExperimentIds(),
                triggerContext.getParameters().keySet().toArray(new String[0]),
                triggerContext.getParameters().values().toArray(new String[0]),
                triggerContext.getDeviceOnlyParameters().keySet().toArray(new String[0]),
                triggerContext.getDeviceOnlyParameters().values().toArray(new String[0]),
                triggerContext.getInitialUrl());
    }

    /**
     * Should be called whenever the Tab's WebContents may have changed. Disconnects from the
     * existing WebContents, if necessary, and then connects to the new WebContents.
     */
    private void detectWebContentsChange(@Nullable WebContents webContents) {
        if (mWebContents != webContents) {
            mWebContents = webContents;
            safeNativeDetach();
            if (mWebContents != null) {
                // Some dependencies are tied to the web-contents and need to be fetched again.
                mDependencies = null;
                mNativeStarter =
                        StarterJni.get().fromWebContents(mWebContents, mStaticDependencies);
                // Note: This is intentionally split into two methods (fromWebContents, attach).
                // It ensures that at the time of attach, the native pointer is already set and
                // this instance is ready to serve requests from native.
                StarterJni.get().attach(mNativeStarter, Starter.this);

                if (mPendingTriggerContext != null) {
                    start(mPendingTriggerContext);
                    mPendingTriggerContext = null;
                }
            }
        }
    }

    @Override
    public void onContentChanged(@Nullable WebContents webContents) {
        detectWebContentsChange(webContents);
    }

    @Override
    public void onWebContentsSwapped(
            @Nullable WebContents webContents, boolean didStartLoad, boolean didFinishLoad) {
        detectWebContentsChange(webContents);
    }

    @Override
    public void onDestroyed(@Nullable WebContents webContents) {
        safeNativeDetach();
    }

    @Override
    public void onActivityAttachmentChanged(
            @Nullable WebContents webContents, @Nullable WindowAndroid window) {
        detectWebContentsChange(webContents);
        safeNativeOnActivityAttachmentChanged();
    }

    @Override
    public void onInteractabilityChanged(
            @Nullable WebContents webContents, boolean isInteractable) {
        safeNativeOnInteractabilityChanged(isInteractable);
    }

    /**
     * Forces native to re-evaluate the Chrome settings. Integration tests may need to call this to
     * ensure that programmatic updates to the Chrome settings are received by the native starter.
     */
    @VisibleForTesting
    public void forceSettingsChangeNotificationForTesting() {
        safeNativeOnInteractabilityChanged(true);
    }

    private void safeNativeDetach() {
        if (mNativeStarter == 0) {
            return;
        }
        StarterJni.get().detach(mNativeStarter, Starter.this);
        mNativeStarter = 0;
    }

    private void safeNativeOnFeatureModuleInstalled(int result) {
        if (mNativeStarter == 0) {
            return;
        }
        StarterJni.get().onFeatureModuleInstalled(mNativeStarter, Starter.this, result);
    }

    private void safeNativeOnInteractabilityChanged(boolean isInteractable) {
        if (mNativeStarter == 0) {
            return;
        }

        StarterJni.get().onInteractabilityChanged(mNativeStarter, Starter.this, isInteractable);
    }

    private void safeNativeOnActivityAttachmentChanged() {
        if (mNativeStarter == 0) {
            return;
        }

        StarterJni.get().onActivityAttachmentChanged(mNativeStarter, Starter.this);
    }

    @CalledByNative
    static boolean getFeatureModuleInstalled() {
        return AutofillAssistantModuleEntryProvider.INSTANCE.isInstalled();
    }

    @CalledByNative
    private void installFeatureModule(boolean showUi) {
        if (getFeatureModuleInstalled()) {
            safeNativeOnFeatureModuleInstalled(FeatureModuleInstallation.DFM_ALREADY_INSTALLED);
            return;
        }

        AutofillAssistantModuleEntryProvider.INSTANCE.getModuleEntry(
                (moduleEntry)
                        -> safeNativeOnFeatureModuleInstalled(moduleEntry != null
                                        ? FeatureModuleInstallation
                                                  .DFM_FOREGROUND_INSTALLATION_SUCCEEDED
                                        : FeatureModuleInstallation
                                                  .DFM_FOREGROUND_INSTALLATION_FAILED),
                mModuleInstallUiProvider, showUi);
    }

    @CalledByNative
    private static boolean getIsFirstTimeUser() {
        return AutofillAssistantPreferencesUtil.isAutofillAssistantFirstTimeTriggerScriptUser();
    }

    @CalledByNative
    private static void setIsFirstTimeUser(boolean firstTimeUser) {
        AutofillAssistantPreferencesUtil.setFirstTimeTriggerScriptUserPreference(firstTimeUser);
    }

    @CalledByNative
    private static boolean getOnboardingAccepted() {
        return !AutofillAssistantPreferencesUtil.getShowOnboarding();
    }

    @CalledByNative
    private static void setOnboardingAccepted(boolean accepted) {
        AutofillAssistantPreferencesUtil.setInitialPreferences(accepted);
    }

    @CalledByNative
    private void showOnboarding(AssistantOnboardingHelper onboardingHelper,
            boolean useDialogOnboarding, String experimentIds, String[] parameterKeys,
            String[] parameterValues, boolean hideBottomSheetOnOnboardingAccepted) {
        if (!AutofillAssistantPreferencesUtil.getShowOnboarding()) {
            safeNativeOnOnboardingFinished(
                    /* shown = */ false, 3 /* AssistantOnboardingResult.ACCEPTED*/);
            return;
        }

        assert parameterKeys.length == parameterValues.length;
        Map<String, String> parameters = new HashMap<>();
        for (int i = 0; i < parameterKeys.length; i++) {
            parameters.put(parameterKeys[i], parameterValues[i]);
        }
        onboardingHelper.showOnboarding(useDialogOnboarding, experimentIds, parameters,
                hideBottomSheetOnOnboardingAccepted,
                result -> safeNativeOnOnboardingFinished(true, result));
    }

    @CalledByNative
    private void hideOnboarding(AssistantOnboardingHelper onboardingHelper) {
        onboardingHelper.hideOnboarding();
    }

    private void safeNativeOnOnboardingFinished(boolean shown, int result) {
        if (mNativeStarter == 0) {
            return;
        }
        StarterJni.get().onOnboardingFinished(mNativeStarter, Starter.this, shown, result);
    }

    @CalledByNative
    static boolean getProactiveHelpSettingEnabled() {
        return AutofillAssistantPreferencesUtil.isProactiveHelpOn();
    }

    @CalledByNative
    private static void setProactiveHelpSettingEnabled(boolean enabled) {
        AutofillAssistantPreferencesUtil.setProactiveHelpPreference(enabled);
    }

    private AutofillAssistantModuleEntry getModuleOrThrow() {
        if (!getFeatureModuleInstalled()) {
            throw new RuntimeException(
                    "Failed to create dependencies: Feature module not installed");
        }

        return AutofillAssistantModuleEntryProvider.INSTANCE.getModuleEntryIfInstalled();
    }

    /**
     * Returns and optionally refreshes the dependencies and the onboarding helper. Since the
     * onboarding helper gets invalidated when the dependencies are invalidated we use the same
     * method to refresh them.
     * */
    @CalledByNative
    private Object[] getOrCreateDependenciesAndOnboardingHelper() {
        if (mDependencies == null) {
            AutofillAssistantModuleEntry module = getModuleOrThrow();
            mDependencies = mStaticDependencies.createDependencies(mActivitySupplier.get());
            mOnboardingHelper = module.createOnboardingHelper(mWebContents, mDependencies);
        }

        return new Object[] {mDependencies, mOnboardingHelper};
    }

    @CalledByNative
    private boolean getIsTabCreatedByGSA() {
        return mIsGsaFunction.apply(mActivitySupplier.get());
    }

    @NativeMethods
    interface Natives {
        long fromWebContents(
                WebContents webContents, AssistantStaticDependencies staticDependencies);
        void attach(long nativeStarterDelegateAndroid, Starter caller);
        void detach(long nativeStarterDelegateAndroid, Starter caller);
        void onFeatureModuleInstalled(
                long nativeStarterDelegateAndroid, Starter caller, int result);
        void onOnboardingFinished(
                long nativeStarterDelegateAndroid, Starter caller, boolean shown, int result);
        void onInteractabilityChanged(
                long nativeStarterDelegateAndroid, Starter caller, boolean isInteractable);
        void onActivityAttachmentChanged(long nativeStarterDelegateAndroid, Starter caller);
        void start(long nativeStarterDelegateAndroid, Starter caller, String experimentIds,
                String[] parameterNames, String[] parameterValues,
                String[] deviceOnlyParameterNames, String[] deviceOnlyParameterValues,
                String initialUrl);
    }
}
