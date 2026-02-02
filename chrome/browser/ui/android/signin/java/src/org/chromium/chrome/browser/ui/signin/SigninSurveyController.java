// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.ui.signin;

import static org.chromium.build.NullUtil.assertNonNull;

import android.app.Activity;

import androidx.annotation.IntDef;

import org.chromium.base.ResettersForTesting;
import org.chromium.base.lifetime.Destroyable;
import org.chromium.base.metrics.RecordHistogram;
import org.chromium.base.task.PostTask;
import org.chromium.base.task.TaskTraits;
import org.chromium.build.BuildConfig;
import org.chromium.build.annotations.NullMarked;
import org.chromium.build.annotations.Nullable;
import org.chromium.chrome.browser.lifecycle.ActivityLifecycleDispatcher;
import org.chromium.chrome.browser.lifecycle.LifecycleObserver;
import org.chromium.chrome.browser.lifecycle.PauseResumeWithNativeObserver;
import org.chromium.chrome.browser.profiles.Profile;
import org.chromium.chrome.browser.profiles.ProfileKeyedMap;
import org.chromium.chrome.browser.signin.services.IdentityServicesProvider;
import org.chromium.chrome.browser.tabmodel.TabModelSelector;
import org.chromium.chrome.browser.ui.hats.MessageSurveyUiDelegate;
import org.chromium.chrome.browser.ui.hats.SurveyClient;
import org.chromium.chrome.browser.ui.hats.SurveyClientFactory;
import org.chromium.chrome.browser.ui.hats.SurveyConfig;
import org.chromium.components.messages.MessageBannerProperties;
import org.chromium.components.messages.MessageDispatcher;
import org.chromium.components.messages.MessageIdentifier;
import org.chromium.components.signin.AccountManagerFacadeProvider;
import org.chromium.components.signin.AccountUtils;
import org.chromium.components.signin.identitymanager.ConsentLevel;
import org.chromium.ui.base.ImmutableWeakReference;
import org.chromium.ui.modelutil.PropertyModel;

import java.lang.annotation.Retention;
import java.lang.annotation.RetentionPolicy;
import java.util.Collections;
import java.util.HashMap;
import java.util.Map;

/** Class that contains logic for showing signin survey. */
@NullMarked
public class SigninSurveyController implements Destroyable {
    // LINT.IfChange(SigninSurveyTrigger)
    @IntDef({
        SigninSurveyType.FRE,
        SigninSurveyType.WEB,
        SigninSurveyType.NTP_SIGNIN_BUTTON,
        SigninSurveyType.NTP_ACCOUNT_AVATAR_TAP,
        SigninSurveyType.NTP_PROMO,
        SigninSurveyType.BOOKMARK_PROMO
    })
    @Retention(RetentionPolicy.SOURCE)
    public @interface SigninSurveyType {
        int FRE = 0;
        int WEB = 1;
        int NTP_SIGNIN_BUTTON = 2;
        int NTP_ACCOUNT_AVATAR_TAP = 3;
        int NTP_PROMO = 4;
        int BOOKMARK_PROMO = 5;
        int MAX_VALUE = BOOKMARK_PROMO;
    }

    // LINT.ThenChange(//tools/metrics/histograms/metadata/signin/enums.xml:SigninSurveyTrigger)

    private static final ProfileKeyedMap<SigninSurveyController> sProfileMap =
            ProfileKeyedMap.createMapOfDestroyables();

    // 5 second delay.
    private static int sDelay = 5000;
    private static boolean sEnableForTesting;

    private final Profile mProfile;
    private @Nullable ImmutableWeakReference<Activity> mActivityHolder;
    private @Nullable ActivityLifecycleDispatcher mActivityLifecycleDispatcher;
    private @Nullable TabModelSelector mTabModelSelector;
    private @Nullable MessageDispatcher mMessageDispatcher;

    private final LifecycleObserver mLifecycleObserver;
    private @Nullable Integer mRegisteredTrigger;
    private boolean mAlreadyTriedShowing;

    /** Initializes a SigninSurveyController instance for the given profile. */
    public static void initialize(
            Profile profile,
            TabModelSelector tabModelSelector,
            ActivityLifecycleDispatcher lifecycleDispatcher,
            Activity activity,
            MessageDispatcher messageDispatcher) {
        // Do not create the client for testing unless explicitly enabled.
        if (BuildConfig.IS_FOR_TEST && !sEnableForTesting) {
            return;
        }
        if (profile.isOffTheRecord()) {
            return;
        }
        SigninSurveyController controller = getForProfile(profile);
        controller.mActivityHolder = new ImmutableWeakReference<>(activity);
        controller.mActivityLifecycleDispatcher = lifecycleDispatcher;
        controller.mTabModelSelector = tabModelSelector;
        controller.mMessageDispatcher = messageDispatcher;
        lifecycleDispatcher.register(controller.mLifecycleObserver);
        // LifecycleObserver doesn't trigger when coming from FRE. So call #maybeShowSurvey() here.
        controller.maybeShowSurvey();
    }

    /**
     * Registers a survey trigger for the given profile. We will try to show the survey when the
     * controller for the profile is initialized and the user comes back to {@link
     * ChromeTabbedActivity}.
     */
    public static void registerTrigger(
            @Nullable Profile profile, @SigninSurveyType int surveyType) {
        if (profile == null || profile.isOffTheRecord()) {
            return;
        }
        SigninSurveyController controller = getForProfile(profile);
        if (controller.mRegisteredTrigger != null) {
            return;
        }
        RecordHistogram.recordEnumeratedHistogram(
                "Signin.HatsSurveyAndroid.TriggerRegistered",
                surveyType,
                SigninSurveyType.MAX_VALUE);
        controller.mRegisteredTrigger = surveyType;
    }

    // Enables triggering the survey in tests and a delay after which the survey will be shown.
    public static void enableWithoutDelayForTesting() {
        int prevDelay = sDelay;
        sEnableForTesting = true;
        sDelay = 0;
        ResettersForTesting.register(
                () -> {
                    sEnableForTesting = false;
                    sDelay = prevDelay;
                });
    }

    private static SigninSurveyController getForProfile(Profile profile) {
        return sProfileMap.getForProfile(profile, SigninSurveyController::buildForProfile);
    }

    private static SigninSurveyController buildForProfile(Profile profile) {
        return new SigninSurveyController(profile);
    }

    private SigninSurveyController(Profile profile) {
        mProfile = profile;
        mLifecycleObserver =
                new PauseResumeWithNativeObserver() {
                    @Override
                    public void onResumeWithNative() {
                        maybeShowSurvey();
                    }

                    @Override
                    public void onPauseWithNative() {}
                };
    }

    @Override
    public void destroy() {
        if (mActivityLifecycleDispatcher != null) {
            mActivityLifecycleDispatcher.unregister(mLifecycleObserver);
            mActivityLifecycleDispatcher = null;
        }
    }

    private void maybeShowSurvey() {
        if (mRegisteredTrigger == null || mAlreadyTriedShowing) {
            return;
        }

        mAlreadyTriedShowing = true;
        SurveyClient surveyClient = constructSurveyClient(mRegisteredTrigger);
        if (surveyClient == null) {
            return;
        }
        Runnable task =
                () -> {
                    Activity activity = assertNonNull(mActivityHolder).get();
                    if (activity == null || activity.isFinishing() || activity.isDestroyed()) {
                        return;
                    }
                    RecordHistogram.recordEnumeratedHistogram(
                            "Signin.HatsSurveyAndroid.TriedShowing",
                            assertNonNull(mRegisteredTrigger),
                            SigninSurveyType.MAX_VALUE);
                    surveyClient.showSurvey(
                            activity,
                            assertNonNull(mActivityLifecycleDispatcher),
                            Collections.emptyMap(),
                            getSurveyPsd());
                    // Destroy after showing the survey to not show it again.
                    destroy();
                };
        // Post the task to not show the survey abruptly. If the user navigates away from the
        // ChromeTabbedActivity within sDelay then the ui message is queued to be shown the next
        // time
        // ChromeTabbedActivity activity is resumed.
        PostTask.postDelayedTask(TaskTraits.UI_DEFAULT, task, sDelay);
    }

    private @Nullable SurveyClient constructSurveyClient(@SigninSurveyType int surveyType) {
        String triggerId = getSurveyTrigger(surveyType);
        assert triggerId != null;
        SurveyConfig surveyConfig = SurveyConfig.get(mProfile, triggerId);
        Activity activity = assertNonNull(mActivityHolder).get();
        if (surveyConfig == null || activity == null) {
            return null;
        }

        PropertyModel message =
                new PropertyModel.Builder(MessageBannerProperties.ALL_KEYS)
                        .with(
                                MessageBannerProperties.MESSAGE_IDENTIFIER,
                                MessageIdentifier.SIGNIN_SURVEY)
                        .build();
        MessageSurveyUiDelegate.populateDefaultValuesForSurveyMessage(
                activity.getResources(), message);
        MessageSurveyUiDelegate messageDelegate =
                new MessageSurveyUiDelegate(
                        message,
                        assertNonNull(mMessageDispatcher),
                        assertNonNull(mTabModelSelector),
                        SurveyClientFactory.getInstance().getCrashUploadPermissionSupplier());
        return SurveyClientFactory.getInstance()
                .createClient(surveyConfig, messageDelegate, mProfile, mTabModelSelector);
    }

    /** Maps {@link SigninSurveyType} to their survey triggerid. */
    private static String getSurveyTrigger(@SigninSurveyType int type) {
        return switch (type) {
            case SigninSurveyType.FRE -> "signin-first-run";
            case SigninSurveyType.WEB -> "signin-web";
            case SigninSurveyType.NTP_SIGNIN_BUTTON -> "signin-ntp-signin-button";
            case SigninSurveyType.NTP_ACCOUNT_AVATAR_TAP -> "signin-ntp-account-avatar-tap";
            case SigninSurveyType.NTP_PROMO -> "signin-ntp-promo";
            case SigninSurveyType.BOOKMARK_PROMO -> "signin-bookmark-promo";
            default -> throw new IllegalStateException("Invalid signin survey type");
        };
    }

    // Returns product specific data (psd) for the survey.
    // Psd fields defined in //chrome/browser/ui/hats/survey_config.cc
    private Map<String, String> getSurveyPsd() {
        Map<String, String> psd = new HashMap<>();
        // chrome_channel and chrome_version is already recorded by SurveyClient infra.
        int numberOfAccounts =
                AccountUtils.getAccountsIfFulfilledOrEmpty(
                                AccountManagerFacadeProvider.getInstance().getAccounts())
                        .size();
        psd.put("Number of Google Accounts", String.valueOf(numberOfAccounts));
        boolean isSignedIn =
                assertNonNull(IdentityServicesProvider.get().getIdentityManager(mProfile))
                        .hasPrimaryAccount(ConsentLevel.SIGNIN);
        psd.put("Sign-in Status", isSignedIn ? "Signed In" : "Signed Out");
        return psd;
    }
}
