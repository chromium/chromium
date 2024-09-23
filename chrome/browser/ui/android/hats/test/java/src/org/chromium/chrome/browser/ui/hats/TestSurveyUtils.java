// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.ui.hats;

import android.app.Activity;
import android.content.Context;
import android.content.SharedPreferences;

import androidx.annotation.NonNull;
import androidx.annotation.Nullable;

import org.junit.rules.TestRule;
import org.junit.runner.Description;
import org.junit.runners.model.Statement;

import org.chromium.base.CommandLine;
import org.chromium.base.ThreadUtils;
import org.chromium.base.supplier.Supplier;
import org.chromium.base.test.util.InMemorySharedPreferences;
import org.chromium.chrome.browser.flags.ChromeSwitches;
import org.chromium.chrome.browser.lifecycle.ActivityLifecycleDispatcher;
import org.chromium.chrome.browser.profiles.Profile;

import java.util.Map;

/** Util class for survey related testing. */
public class TestSurveyUtils {
    /**
     * Template for trigger Id override for command line. Usage:
     *
     * <pre>
     *  &#64;Features.Add({MySurveyFeature + "&#60Study"})
     *  &#64;CommandlineFlags.Add({
     *      "force-fieldtrials=Study/Group",
     *      "force-fieldtrial-param=Study.Group:" +
     *          TestSurveyUtils.TEST_SURVEY_TRIGGER_ID_OVERRIDE_TEMPLATE + &#60triggerId&#62})
     *  public class MySurveyTest {
     *      // ...
     *  }
     * </pre>
     */
    public static final String TEST_SURVEY_TRIGGER_ID_OVERRIDE_TEMPLATE =
            "probability/1.0/en_site_id/";

    public static final String TEST_TRIGGER_ID_FOO = "test_trigger_id_foo";

    /**
     * Assume a test SurveyConfig exists for trigger with given PSD. Use to bypass survey config
     * parsing from native.
     */
    public static void setTestSurveyConfigForTrigger(
            String trigger, String[] psdBitFields, String[] psdStringFields) {
        SurveyConfig.setSurveyConfigForTesting(
                new SurveyConfig(
                        trigger, TEST_TRIGGER_ID_FOO, 1.0f, false, psdBitFields, psdStringFields));
    }

    /**
     * Force survey to show if SurveyClientImpl is used in test. This will bypass the throttler
     * check in the background.
     */
    static void forceShowSurveyForTesting(Boolean doForce) {
        SurveyClientImpl.setForceShowSurveyForTesting(doForce);
    }

    static TestSurveyFactory setUpTestSurveyFactory() {
        TestSurveyFactory factory = ThreadUtils.runOnUiThreadBlocking(TestSurveyFactory::new);
        SurveyClientFactory.setInstanceForTesting(factory);
        return factory;
    }

    /**
     * Create a test-only survey component. In test environment:
     *  - The survey throttler check will be ignored and always pass.
     *  - Crash upload will be allowed all the time.
     */
    public static class TestSurveyComponentRule implements TestRule {
        private TestSurveyFactory mTestSurveyFactory;

        /** Return the trigger ID of the last shown survey. */
        public String getLastShownTriggerId() {
            return mTestSurveyFactory.getLastShownTriggerId();
        }

        /** Return the set of PSD of the last shown survey. */
        public Map<String, String> getLastShownSurveyPsd() {
            return mTestSurveyFactory.getLastShownSurveyPsd();
        }

        /** Return whether the given trigger ID has been attempted to show. */
        public boolean isPromptShownForTriggerId(String triggerId) {
            return mTestSurveyFactory
                    .getMetadata()
                    .contains(SurveyMetadata.KEY_PREFIX_DATE_PROMPT_DISPLAYED + triggerId);
        }

        @Override
        public Statement apply(Statement base, Description description) {
            return new Statement() {
                @Override
                public void evaluate() throws Throwable {
                    // Append switch so throttler always passes for the survey.
                    CommandLine.getInstance()
                            .appendSwitch(ChromeSwitches.CHROME_FORCE_ENABLE_SURVEY);
                    CommandLine.getInstance()
                            .appendSwitch(ChromeSwitches.DISABLE_FIRST_RUN_EXPERIENCE);

                    mTestSurveyFactory = setUpTestSurveyFactory();
                    base.evaluate();
                }
            };
        }
    }

    /** Test impl of factory that generate SurveyClient using test set up. */
    static class TestSurveyFactory extends SurveyClientFactory {
        private final AlwaysSucceedSurveyController mTestController;
        private final SharedPreferences mMetadata;

        TestSurveyFactory() {
            super(null);
            mTestController = new AlwaysSucceedSurveyController();
            mCrashUploadPermissionSupplier.set(true);

            mMetadata = new InMemorySharedPreferences();
            SurveyMetadata.initializeForTesting(mMetadata, 1);
        }

        /** Return the trigger ID of the last shown survey. */
        String getLastShownTriggerId() {
            return mTestController.mLastShownTriggerId;
        }

        /** Return the set of PSD of the last shown survey. */
        Map<String, String> getLastShownSurveyPsd() {
            return mTestController.mLastShownSurveyPsd;
        }

        SharedPreferences getMetadata() {
            return mMetadata;
        }

        @Override
        @Nullable
        public SurveyClient createClient(
                @NonNull SurveyConfig config,
                @NonNull SurveyUiDelegate uiDelegate,
                Profile profile) {
            return new SurveyClientImpl(
                    config, uiDelegate, mTestController, mCrashUploadPermissionSupplier, profile);
        }

        @Override
        public Supplier<Boolean> getCrashUploadPermissionSupplier() {
            return mCrashUploadPermissionSupplier;
        }
    }

    private static class AlwaysSucceedSurveyController implements SurveyController {
        String mLastShownTriggerId;
        Map<String, String> mLastShownSurveyPsd;

        @Override
        public void downloadSurvey(
                Context context,
                String triggerId,
                Runnable onSuccessRunnable,
                Runnable onFailureRunnable) {
            onSuccessRunnable.run();
        }

        @Override
        public void showSurveyIfAvailable(
                Activity activity,
                String triggerId,
                int displayLogoResId,
                @Nullable ActivityLifecycleDispatcher lifecycleDispatcher,
                @Nullable Map<String, String> psd) {
            mLastShownTriggerId = triggerId;
            mLastShownSurveyPsd = psd;
        }

        @Override
        public boolean isSurveyExpired(String triggerId) {
            return false;
        }

        @Override
        public void destroy() {}
    }

    /** Test implementation of a SurveyController. */
    static class TestSurveyController implements SurveyController {
        private String mShownSurveyTriggerId;
        private SurveyEntry mSurveyEntry;

        @Override
        public void downloadSurvey(
                Context context,
                String triggerId,
                Runnable onSuccessRunnable,
                Runnable onFailureRunnable) {
            assert mSurveyEntry == null;
            mSurveyEntry = new SurveyEntry(triggerId, onSuccessRunnable, onFailureRunnable);
        }

        @Override
        public void showSurveyIfAvailable(
                Activity activity,
                String triggerId,
                int displayLogoResId,
                @Nullable ActivityLifecycleDispatcher lifecycleDispatcher,
                @Nullable Map<String, String> psd) {
            assert triggerId.equals(mSurveyEntry.triggerId) : "Survey not downloaded yet.";
            assert mShownSurveyTriggerId == null : "Survey already shown: " + mShownSurveyTriggerId;

            mShownSurveyTriggerId = triggerId;
        }

        @Override
        public boolean isSurveyExpired(String triggerId) {
            assert triggerId.equals(mSurveyEntry.triggerId);
            return mSurveyEntry.isExpired;
        }

        @Override
        public void destroy() {
            mSurveyEntry = null;
            mShownSurveyTriggerId = null;
        }

        /** Simulate download being successful with a given survey. */
        public void simulateDownloadFinished(String triggerId, boolean succeed) {
            assert mSurveyEntry != null && triggerId.equals(mSurveyEntry.triggerId);

            Runnable callback =
                    succeed ? mSurveyEntry.onSuccessRunnable : mSurveyEntry.onFailureRunnable;
            callback.run();
        }

        public void simulateSurveyExpired(String triggerId) {
            assert mSurveyEntry != null && triggerId.equals(mSurveyEntry.triggerId);
            mSurveyEntry.isExpired = true;
        }

        /** Whether the given survey is shown. */
        public boolean isSurveyShown(String triggerId) {
            return triggerId.equals(mShownSurveyTriggerId);
        }

        /** Whether there are any survey being downloaded. */
        public boolean hasSurveyDownloadInQueue() {
            return mSurveyEntry != null;
        }

        private static class SurveyEntry {
            public String triggerId;
            public Runnable onSuccessRunnable;
            public Runnable onFailureRunnable;

            public boolean isShown;
            public boolean isExpired;

            SurveyEntry(String triggerId, Runnable onSuccessRunnable, Runnable onFailureRunnable) {
                this.triggerId = triggerId;
                this.onSuccessRunnable = onSuccessRunnable;
                this.onFailureRunnable = onFailureRunnable;
            }
        }
    }

    /** Test implementation of a SurveyUiDelegate. */
    static class TestSurveyUiDelegate implements SurveyUiDelegate {
        private Runnable mOnSurveyAcceptedCallable;
        private Runnable mOnSurveyDeclinedCallable;
        private Runnable mOnSurveyPresentationFailedCallable;

        private boolean mIsShowing;
        private boolean mPresentationWillFail;

        @Override
        public void showSurveyInvitation(
                Runnable onSurveyAccepted,
                Runnable onSurveyDeclined,
                Runnable onSurveyPresentationFailed) {
            mOnSurveyAcceptedCallable = onSurveyAccepted;
            mOnSurveyDeclinedCallable = onSurveyDeclined;
            mOnSurveyPresentationFailedCallable = onSurveyPresentationFailed;

            if (mPresentationWillFail) {
                mOnSurveyPresentationFailedCallable.run();
            } else {
                mIsShowing = true;
            }
        }

        @Override
        public void dismiss() {
            if (mOnSurveyDeclinedCallable == null) return;
            mIsShowing = false;
            mOnSurveyDeclinedCallable.run();
        }

        public void acceptSurvey() {
            assert mOnSurveyAcceptedCallable != null;
            mIsShowing = false;
            mOnSurveyAcceptedCallable.run();
        }

        public boolean isShowing() {
            return mIsShowing;
        }

        public void setPresentationWillFail() {
            mPresentationWillFail = true;
        }
    }
}
