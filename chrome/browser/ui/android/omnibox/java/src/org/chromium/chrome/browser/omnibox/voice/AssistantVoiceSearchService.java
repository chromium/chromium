// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.omnibox.voice;

import static org.chromium.chrome.browser.preferences.ChromePreferenceKeys.ASSISTANT_VOICE_SEARCH_ENABLED;

import android.accounts.Account;
import android.content.Context;
import android.content.Intent;
import android.content.res.ColorStateList;
import android.graphics.drawable.Drawable;
import android.text.TextUtils;

import androidx.annotation.IntDef;
import androidx.annotation.NonNull;
import androidx.annotation.Nullable;
import androidx.annotation.VisibleForTesting;
import androidx.appcompat.content.res.AppCompatResources;

import org.chromium.base.Promise;
import org.chromium.base.SysUtils;
import org.chromium.base.metrics.RecordHistogram;
import org.chromium.base.task.AsyncTask;
import org.chromium.chrome.browser.flags.ChromeFeatureList;
import org.chromium.chrome.browser.gsa.GSAState;
import org.chromium.chrome.browser.omnibox.R;
import org.chromium.chrome.browser.preferences.SharedPreferencesManager;
import org.chromium.chrome.browser.profiles.Profile;
import org.chromium.chrome.browser.search_engines.TemplateUrlServiceFactory;
import org.chromium.chrome.browser.signin.services.IdentityServicesProvider;
import org.chromium.chrome.browser.theme.ThemeUtils;
import org.chromium.chrome.browser.ui.theme.BrandedColorScheme;
import org.chromium.components.externalauth.ExternalAuthUtils;
import org.chromium.components.search_engines.TemplateUrlService;
import org.chromium.components.signin.AccountManagerFacade;
import org.chromium.components.signin.AccountManagerFacadeProvider;
import org.chromium.components.signin.AccountsChangeObserver;
import org.chromium.components.signin.base.CoreAccountInfo;
import org.chromium.components.signin.identitymanager.ConsentLevel;
import org.chromium.components.signin.identitymanager.IdentityManager;
import org.chromium.components.signin.identitymanager.PrimaryAccountChangeEvent;

import java.lang.annotation.Retention;
import java.lang.annotation.RetentionPolicy;
import java.util.ArrayList;
import java.util.List;

/**
 * Service for state tracking and event delivery to classes that need to observe the state
 * of Assistant Voice Search.
 **/
public class AssistantVoiceSearchService implements TemplateUrlService.TemplateUrlServiceObserver,
                                                    IdentityManager.Observer,
                                                    AccountsChangeObserver {
    @VisibleForTesting
    static final String STARTUP_HISTOGRAM_SUFFIX = ".Startup";
    @VisibleForTesting
    static final String USER_ELIGIBILITY_HISTOGRAM = "Assistant.VoiceSearch.UserEligibility";
    @VisibleForTesting
    static final String USER_ELIGIBILITY_FAILURE_REASON_HISTOGRAM =
            "Assistant.VoiceSearch.UserEligibility.FailureReason";
    @VisibleForTesting
    static final String AGSA_VERSION_HISTOGRAM = "Assistant.VoiceSearch.AgsaVersion";
    private static final String DEFAULT_ASSISTANT_AGSA_MIN_VERSION = "11.7";
    private static final boolean DEFAULT_ASSISTANT_COLORFUL_MIC_ENABLED = false;
    private static Boolean sAlwaysUseAssistantVoiceSearchForTesting;

    // These values are persisted to logs. Entries should not be renumbered and
    // numeric values should never be reused.
    @IntDef({EligibilityFailureReason.AGSA_CANT_HANDLE_INTENT,
            EligibilityFailureReason.AGSA_VERSION_BELOW_MINIMUM,
            EligibilityFailureReason.CHROME_NOT_GOOGLE_SIGNED,
            EligibilityFailureReason.AGSA_NOT_GOOGLE_SIGNED,
            EligibilityFailureReason.NON_GOOGLE_SEARCH_ENGINE,
            EligibilityFailureReason.NO_CHROME_ACCOUNT, EligibilityFailureReason.LOW_END_DEVICE,
            EligibilityFailureReason.MULTIPLE_ACCOUNTS_ON_DEVICE,
            EligibilityFailureReason.AGSA_NOT_INSTALLED})
    @Retention(RetentionPolicy.SOURCE)
    @interface EligibilityFailureReason {
        int AGSA_CANT_HANDLE_INTENT = 0;
        int AGSA_VERSION_BELOW_MINIMUM = 1;
        // No longer used, replaced by Chrome-side eligibility checks.
        // int AGSA_DOESNT_SUPPORT_VOICE_SEARCH_CHECK_NOT_COMPLETE = 2;
        // No longer used, replaced by Chrome-side eligibility checks.
        // int AGSA_DOESNT_SUPPORT_VOICE_SEARCH = 3;
        int CHROME_NOT_GOOGLE_SIGNED = 4;
        int AGSA_NOT_GOOGLE_SIGNED = 5;
        // No longer used, AGSA now supports attribution to arbitrary accounts.
        // int ACCOUNT_MISMATCH = 6;
        int NON_GOOGLE_SEARCH_ENGINE = 7;
        int NO_CHROME_ACCOUNT = 8;
        int LOW_END_DEVICE = 9;
        int MULTIPLE_ACCOUNTS_ON_DEVICE = 10;
        int AGSA_NOT_INSTALLED = 11;

        // STOP: When updating this, also update values in enums.xml and make sure to update the
        // IntDef above.
        int NUM_ENTRIES = 12;
    }

    /** Allows outside classes to listen for changes in this service. */
    public interface Observer {
        /**
         * Called when the service changes, use relevant getters to update your state. This
         * indicates that the state of AssistantVoiceSearchService has changed and you should
         * re-query the getters you're interested in.
         * - The microphone Drawable icon can change during runtime.
         **/
        void onAssistantVoiceSearchServiceChanged();
    }

    // TODO(wylieb): Convert this to an ObserverList and add #addObserver, #removeObserver.
    private final Observer mObserver;
    private final Context mContext;
    private final ExternalAuthUtils mExternalAuthUtils;
    private final TemplateUrlService mTemplateUrlService;
    private final GSAState mGsaState;
    private final SharedPreferencesManager mSharedPrefsManager;
    private final IdentityManager mIdentityManager;
    private final AccountManagerFacade mAccountManagerFacade;

    private boolean mIsDefaultSearchEngineGoogle;
    private boolean mIsAssistantVoiceSearchEnabled;
    private boolean mIsColorfulMicEnabled;
    private boolean mShouldShowColorfulButtons;
    private boolean mIsMultiAccountCheckEnabled;
    private String mMinAgsaVersion;

    public AssistantVoiceSearchService(@NonNull Context context,
            @NonNull ExternalAuthUtils externalAuthUtils,
            @NonNull TemplateUrlService templateUrlService, @NonNull GSAState gsaState,
            @Nullable Observer observer, @NonNull SharedPreferencesManager sharedPrefsManager,
            @NonNull IdentityManager identityManager,
            @NonNull AccountManagerFacade accountManagerFacade) {
        mContext = context;
        mExternalAuthUtils = externalAuthUtils;
        mTemplateUrlService = templateUrlService;
        mGsaState = gsaState;
        mSharedPrefsManager = sharedPrefsManager;
        mObserver = observer;
        mIdentityManager = identityManager;
        mAccountManagerFacade = accountManagerFacade;

        mAccountManagerFacade.addObserver(this);
        mIdentityManager.addObserver(this);
        mTemplateUrlService.addObserver(this);
        initializeAssistantVoiceSearchState();
    }

    public void destroy() {
        mTemplateUrlService.removeObserver(this);
        mIdentityManager.removeObserver(this);
        mAccountManagerFacade.removeObserver(this);
    }

    private void notifyObserver() {
        if (mObserver == null) return;
        mObserver.onAssistantVoiceSearchServiceChanged();
    }

    /** Cache Assistant voice search variable state. */
    @VisibleForTesting
    void initializeAssistantVoiceSearchState() {
        mIsAssistantVoiceSearchEnabled =
                ChromeFeatureList.isEnabled(ChromeFeatureList.OMNIBOX_ASSISTANT_VOICE_SEARCH);

        mIsColorfulMicEnabled = ChromeFeatureList.getFieldTrialParamByFeatureAsBoolean(
                ChromeFeatureList.OMNIBOX_ASSISTANT_VOICE_SEARCH, "colorful_mic",
                DEFAULT_ASSISTANT_COLORFUL_MIC_ENABLED);

        mMinAgsaVersion = ChromeFeatureList.getFieldTrialParamByFeature(
                ChromeFeatureList.OMNIBOX_ASSISTANT_VOICE_SEARCH, "min_agsa_version");

        mIsMultiAccountCheckEnabled = ChromeFeatureList.getFieldTrialParamByFeatureAsBoolean(
                ChromeFeatureList.OMNIBOX_ASSISTANT_VOICE_SEARCH, "enable_multi_account_check",
                /* default= */ true);

        mIsDefaultSearchEngineGoogle = mTemplateUrlService.isDefaultSearchEngineGoogle();

        mShouldShowColorfulButtons = isColorfulMicEnabled();
    }

    /** @return Whether the user has had a chance to enable the feature. */
    public boolean needsEnabledCheck() {
        if (ChromeFeatureList.isEnabled(
                    ChromeFeatureList.ASSISTANT_NON_PERSONALIZED_VOICE_SEARCH)) {
            return false;
        }
        return !mSharedPrefsManager.contains(ASSISTANT_VOICE_SEARCH_ENABLED);
    }

    /**
     * Checks if the client is eligible Assistant for voice search. It's
     * {@link canRequestAssistantVoiceSearch} with additional conditions:
     * - The feature must be enabled.
     * - The consent flow must be accepted for personalized queries.
     */
    public boolean shouldRequestAssistantVoiceSearch() {
        if (sAlwaysUseAssistantVoiceSearchForTesting != null) {
            return sAlwaysUseAssistantVoiceSearchForTesting;
        }
        return mIsAssistantVoiceSearchEnabled && canRequestAssistantVoiceSearch() && isEnabled();
    }

    /**
     * Checks if the client meets the device requirements to use Assistant for voice search. This
     * doesn't check if the client should use Assistant voice search, which accounts for additional
     * conditions.
     */
    public boolean canRequestAssistantVoiceSearch() {
        if (sAlwaysUseAssistantVoiceSearchForTesting != null) {
            return sAlwaysUseAssistantVoiceSearchForTesting;
        }
        return mIsAssistantVoiceSearchEnabled
                && isDeviceEligibleForAssistant(/* returnImmediately= */ true, /* outList= */ null);
    }

    /**
     * @return Gets the current mic drawable, this will create the drawable if it doesn't already
     *         exist or reuse the existing Drawable's ConstantState if it does already exist.
     **/
    public Drawable getCurrentMicDrawable() {
        return AppCompatResources.getDrawable(mContext,
                mShouldShowColorfulButtons ? R.drawable.ic_colorful_mic : R.drawable.btn_mic);
    }

    /** @return The correct ColorStateList for the current theme. */
    public @Nullable ColorStateList getButtonColorStateList(
            @BrandedColorScheme int brandedColorScheme, Context context) {
        if (mShouldShowColorfulButtons) return null;

        return ThemeUtils.getThemedToolbarIconTint(context, brandedColorScheme);
    }

    /** Called from {@link VoiceRecognitionHandler} after the consent flow has completed. */
    public void onAssistantConsentDialogComplete(boolean useAssistant) {
        if (useAssistant) updateColorfulMicState();
    }

    /** For requests that require it - checks if the user enabled the feature. */
    private boolean isEnabled() {
        if (ChromeFeatureList.isEnabled(
                    ChromeFeatureList.ASSISTANT_NON_PERSONALIZED_VOICE_SEARCH)) {
            return true;
        }
        return isEnabledByPreference();
    }

    /**
     * @return Whether the user has enabled the feature, ensure {@link needsEnabledCheck} is
     *         called first.
     */
    private boolean isEnabledByPreference() {
        return mSharedPrefsManager.readBoolean(
                ASSISTANT_VOICE_SEARCH_ENABLED, /* default= */ false);
    }

    /** @return Whether the colorful mic is enabled. */
    private boolean isColorfulMicEnabled() {
        return mContext.getPackageManager() != null && mIsColorfulMicEnabled
                && shouldRequestAssistantVoiceSearch();
    }

    /**
     * @return The min Agsa version required for assistant voice search. This method checks the
     *         chrome feature list for this value before using the default.
     */
    private String getAgsaMinVersion() {
        if (TextUtils.isEmpty(mMinAgsaVersion)) return DEFAULT_ASSISTANT_AGSA_MIN_VERSION;
        return mMinAgsaVersion;
    }

    /**
     * Checks all Assistant eligibility conditions. Optionally assess all failure conditions.
     * @param returnImmediately Whether to return as soon as the client is ineligible.
     * @param outList A list that the failure reasons will be added to, when returnImmediately is
     *                false. This list will not be altered when returnImmediately is true. Must be
     *                non-null if returnImmediately is false.
     * @return Whether the device is eligible for Assistant.
     */
    @VisibleForTesting
    protected boolean isDeviceEligibleForAssistant(
            boolean returnImmediately, List<Integer> outList) {
        assert returnImmediately || outList != null;

        if (!mGsaState.isGsaInstalled()) {
            if (returnImmediately) return false;
            outList.add(EligibilityFailureReason.AGSA_NOT_INSTALLED);
        }

        if (!mGsaState.canAgsaHandleIntent(getAssistantVoiceSearchIntent())) {
            if (returnImmediately) return false;
            outList.add(EligibilityFailureReason.AGSA_CANT_HANDLE_INTENT);
        }
        if (mGsaState.isAgsaVersionBelowMinimum(
                    mGsaState.getAgsaVersionName(), getAgsaMinVersion())) {
            if (returnImmediately) return false;
            outList.add(EligibilityFailureReason.AGSA_VERSION_BELOW_MINIMUM);
        }

        // AGSA will throw an exception if Chrome isn't Google signed.
        if (!mExternalAuthUtils.isChromeGoogleSigned()) {
            if (returnImmediately) return false;
            outList.add(EligibilityFailureReason.CHROME_NOT_GOOGLE_SIGNED);
        }

        if (!mExternalAuthUtils.isGoogleSigned(GSAState.PACKAGE_NAME)) {
            if (returnImmediately) return false;
            outList.add(EligibilityFailureReason.AGSA_NOT_GOOGLE_SIGNED);
        }

        if (!mIsDefaultSearchEngineGoogle) {
            if (returnImmediately) return false;
            outList.add(EligibilityFailureReason.NON_GOOGLE_SEARCH_ENGINE);
        }

        // TODO(crbug/1344574): verify if we can support low end devices too with the new flow.
        if (SysUtils.isLowEndDevice()) {
            if (returnImmediately) return false;
            outList.add(EligibilityFailureReason.LOW_END_DEVICE);
        }

        if (!ChromeFeatureList.isEnabled(
                    ChromeFeatureList.ASSISTANT_NON_PERSONALIZED_VOICE_SEARCH)) {
            if (!mIdentityManager.hasPrimaryAccount(ConsentLevel.SYNC)) {
                if (returnImmediately) return false;
                outList.add(EligibilityFailureReason.NO_CHROME_ACCOUNT);
            }

            if (mIsMultiAccountCheckEnabled && doesViolateMultiAccountCheck()) {
                if (returnImmediately) return false;
                outList.add(EligibilityFailureReason.MULTIPLE_ACCOUNTS_ON_DEVICE);
            }
        }
        // Either we would have failed already or we have some errors on the list.
        // Otherwise this client is eligible for assistant.
        return returnImmediately || outList.size() == 0;
    }

    /** @return The Intent for Assistant voice search. */
    public Intent getAssistantVoiceSearchIntent() {
        Intent intent = new Intent(Intent.ACTION_SEARCH);
        intent.setPackage(GSAState.PACKAGE_NAME);
        return intent;
    }

    /** Return the current user email or null if no account is present. */
    public @Nullable String getUserEmail() {
        CoreAccountInfo info = mIdentityManager.getPrimaryAccountInfo(ConsentLevel.SYNC);
        return info == null ? null : info.getEmail();
    }

    /**
     * Records user eligibility to histograms that are specific to voice search init.
     *
     * This should be called each time an Assistant voice search is started.
     *
     * See reportUserEligibility for details on which histograms are recorded.
     */
    void reportMicPressUserEligibility() {
        // The base histogram (no suffix) is used to record the per-mic-press eligibility.
        reportUserEligibility("");
    }

    /**
     * Records user eligibility to histograms that are specific to browser startup.
     *
     * This should be only be called once per browser startup.
     *
     * See reportUserEligibility for details on which histograms are recorded.
     */
    public static void reportStartupUserEligibility(Context context) {
        AssistantVoiceSearchService service =
                new AssistantVoiceSearchService(context, ExternalAuthUtils.getInstance(),
                        TemplateUrlServiceFactory.get(), GSAState.getInstance(),
                        /*observer=*/null, SharedPreferencesManager.getInstance(),
                        IdentityServicesProvider.get().getIdentityManager(
                                Profile.getLastUsedRegularProfile()),
                        AccountManagerFacadeProvider.getInstance());
        service.reportUserEligibility(STARTUP_HISTOGRAM_SUFFIX);
    }

    /**
     * Reports user eligilibility to a histogram determined by |timingSuffix|,
     * - User eligibility is always reported to USER_ELIGIBILITY_HISTOGRAM + suffix.
     * - If the user is ineligible, it also reports the reasons for ineligibility to
     *   USER_ELIGIBILITY_FAILURE_REASON_HISTOGRAM + suffix.
     * - If AGSA is available, the version number is reported to AGSA_VERSION_HISTOGRAM + suffix.
     *
     * @param timingSuffix The suffix to attach to the histograms to indicate the point at which
     *        they are being recorded. For example, this can be used to specifically report
     *        eligibility on browser startup.
     */
    private void reportUserEligibility(String timingSuffix) {
        List<Integer> failureReasons = new ArrayList<>();
        boolean eligible = isDeviceEligibleForAssistant(
                /* returnImmediately= */ false, /* outList */ failureReasons);
        RecordHistogram.recordBooleanHistogram(USER_ELIGIBILITY_HISTOGRAM + timingSuffix, eligible);

        // See notes in {@link GSAState#parseAgsaMajorMinorVersionAsInteger} for details about this
        // number.
        Integer versionNumber =
                mGsaState.parseAgsaMajorMinorVersionAsInteger(mGsaState.getAgsaVersionName());
        if (versionNumber != null) {
            RecordHistogram.recordSparseHistogram(
                    AGSA_VERSION_HISTOGRAM + timingSuffix, (int) versionNumber);
        }

        for (@EligibilityFailureReason int reason : failureReasons) {
            RecordHistogram.recordEnumeratedHistogram(
                    USER_ELIGIBILITY_FAILURE_REASON_HISTOGRAM + timingSuffix, reason,
                    EligibilityFailureReason.NUM_ENTRIES);
        }
    }

    private void updateColorfulMicState() {
        final boolean shouldShowColorfulMic = isColorfulMicEnabled();
        // Execute the update/notification in an AsyncTask to prevent re-entrant calls.
        new AsyncTask<Boolean>() {
            @Override
            protected Boolean doInBackground() {
                return mShouldShowColorfulButtons != shouldShowColorfulMic;
            }
            @Override
            protected void onPostExecute(Boolean notify) {
                mShouldShowColorfulButtons = shouldShowColorfulMic;
                if (notify) notifyObserver();
            }
        }.executeOnExecutor(AsyncTask.THREAD_POOL_EXECUTOR);
    }

    /** Wrapped multi-account check to handle the exception. */
    @VisibleForTesting
    boolean doesViolateMultiAccountCheck() {
        // In case of the accounts cannot be fetched -- we can't be sure so default to true.
        final Promise<List<Account>> promise = mAccountManagerFacade.getAccounts();
        return !mExternalAuthUtils.canUseGooglePlayServices() || !promise.isFulfilled()
                || promise.getResult().size() > 1;
    }

    // TemplateUrlService.TemplateUrlServiceObserver implementation

    @Override
    public void onTemplateURLServiceChanged() {
        boolean searchEngineGoogle = mTemplateUrlService.isDefaultSearchEngineGoogle();
        if (mIsDefaultSearchEngineGoogle == searchEngineGoogle) return;

        mIsDefaultSearchEngineGoogle = searchEngineGoogle;
        mShouldShowColorfulButtons = isColorfulMicEnabled();
        notifyObserver();
    }

    // IdentityManager.Observer implementation

    @Override
    public void onPrimaryAccountChanged(PrimaryAccountChangeEvent eventDetails) {
        updateColorfulMicState();
    }

    // AccountsChangeObserver implementation

    @Override
    public void onAccountsChanged() {
        updateColorfulMicState();
    }

    // Test-only methods

    /** Enable the colorful mic for testing purposes. */
    void setColorfulMicEnabledForTesting(boolean enabled) {
        mIsColorfulMicEnabled = enabled;
        mShouldShowColorfulButtons = enabled;
    }

    void setMultiAccountCheckEnabledForTesting(boolean enabled) {
        mIsMultiAccountCheckEnabled = enabled;
    }

    static void setAlwaysUseAssistantVoiceSearchForTestingEnabled(Boolean enabled) {
        sAlwaysUseAssistantVoiceSearchForTesting = enabled;
    }
}
