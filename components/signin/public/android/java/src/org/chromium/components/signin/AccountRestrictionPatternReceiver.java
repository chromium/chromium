// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.components.signin;

import android.content.BroadcastReceiver;
import android.content.Context;
import android.content.Intent;
import android.content.IntentFilter;
import android.os.Build;
import android.os.Bundle;
import android.os.UserManager;

import androidx.annotation.WorkerThread;

import org.chromium.base.Callback;
import org.chromium.base.ContextUtils;
import org.chromium.base.Log;
import org.chromium.base.Promise;
import org.chromium.base.task.AsyncTask;

import java.util.ArrayList;
import java.util.List;

/**
 * Receiver of the RestrictAccountsToPatterns policy.
 */
final class AccountRestrictionPatternReceiver {
    private static final String TAG = "AccountRestriction";
    private static final String ACCOUNT_RESTRICTION_PATTERNS_KEY = "RestrictAccountsToPatterns";

    AccountRestrictionPatternReceiver(Callback<List<PatternMatcher>> onPatternsUpdated) {
        if (Build.VERSION.SDK_INT > Build.VERSION_CODES.LOLLIPOP) {
            BroadcastReceiver receiver = new BroadcastReceiver() {
                @Override
                public void onReceive(Context context, Intent intent) {
                    getRestrictionPatternsAsync().then(onPatternsUpdated::onResult);
                }
            };
            ContextUtils.getApplicationContext().registerReceiver(
                    receiver, new IntentFilter(Intent.ACTION_APPLICATION_RESTRICTIONS_CHANGED));
        }
    }

    Promise<List<PatternMatcher>> getRestrictionPatternsAsync() {
        final Promise<List<PatternMatcher>> promise = new Promise<>();
        new AsyncTask<List<PatternMatcher>>() {
            @Override
            protected List<PatternMatcher> doInBackground() {
                return getRestrictionPatterns();
            }

            @Override
            protected void onPostExecute(List<PatternMatcher> patternMatchers) {
                promise.fulfill(patternMatchers);
            }
        }.executeOnExecutor(AsyncTask.SERIAL_EXECUTOR);
        return promise;
    }

    @WorkerThread
    List<PatternMatcher> getRestrictionPatterns() {
        final List<PatternMatcher> patternMatchers = new ArrayList<>();
        if (Build.VERSION.SDK_INT < Build.VERSION_CODES.JELLY_BEAN_MR2) {
            return patternMatchers;
        }
        try {
            Context context = ContextUtils.getApplicationContext();
            UserManager userManager = (UserManager) context.getSystemService(Context.USER_SERVICE);
            Bundle appRestrictions =
                    userManager.getApplicationRestrictions(context.getPackageName());
            String[] patterns = appRestrictions.getStringArray(ACCOUNT_RESTRICTION_PATTERNS_KEY);
            if (patterns != null) {
                for (String pattern : patterns) {
                    patternMatchers.add(new PatternMatcher(pattern));
                }
            }
        } catch (PatternMatcher.IllegalPatternException e) {
            Log.e(TAG, "Can't get account restriction patterns", e);
        }
        return patternMatchers;
    }
}
