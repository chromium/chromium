// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.ui.hats;

import android.app.Activity;
import android.content.Context;

import androidx.annotation.Nullable;

import org.chromium.chrome.browser.lifecycle.ActivityLifecycleDispatcher;

import java.util.Map;

/**
 * Util class for survey related testing.
 */
public class TestSurveyUtils {
    /**
     * Test implementation of a SurveyController.
     */
    static class TestSurveyController implements SurveyController {
        private String mShownSurveyTriggerId;
        private SurveyEntry mSurveyEntry;

        @Override
        public void downloadSurvey(Context context, String triggerId, Runnable onSuccessRunnable,
                Runnable onFailureRunnable) {
            assert mSurveyEntry == null;
            mSurveyEntry = new SurveyEntry(triggerId, onSuccessRunnable, onFailureRunnable);
        }

        @Override
        public void showSurveyIfAvailable(Activity activity, String triggerId, int displayLogoResId,
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

        /**
         * Simulate download being successful with a given survey.
         */
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

        /**
         * Whether the given survey is shown.
         */
        public boolean isSurveyShown(String triggerId) {
            return triggerId.equals(mShownSurveyTriggerId);
        }

        /**
         * Whether there are any survey being downloaded.
         */
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

    /**
     * Test implementation of a SurveyUiDelegate.
     */
    static class TestSurveyUiDelegate implements SurveyUiDelegate {
        private Runnable mOnSurveyAcceptedCallable;
        private Runnable mOnSurveyDeclinedCallable;
        private Runnable mOnSurveyPresentationFailedCallable;

        private boolean mIsShowing;
        private boolean mPresentationWillFail;

        @Override
        public void showSurveyInvitation(Runnable onSurveyAccepted, Runnable onSurveyDeclined,
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
