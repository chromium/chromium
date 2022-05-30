// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.components.external_intents;

import android.content.ComponentName;
import android.content.Intent;
import android.content.pm.ResolveInfo;
import android.os.SystemClock;
import android.provider.Browser;
import android.text.TextUtils;

import org.chromium.base.ContextUtils;
import org.chromium.base.Function;
import org.chromium.base.IntentUtils;
import org.chromium.base.Log;
import org.chromium.base.metrics.RecordHistogram;
import org.chromium.ui.base.PageTransition;

import java.util.HashSet;
import java.util.List;

/**
 * This class contains the logic to determine effective navigation/redirect.
 */
public class RedirectHandler {
    private static final String TAG = "RedirectHandler";

    // The last committed entry index when no navigations have committed.
    public static final int NO_COMMITTED_ENTRY_INDEX = -1;
    // An invalid entry index.
    private static final int INVALID_ENTRY_INDEX = -2;
    public static final long INVALID_TIME = -1;

    private static final int NAVIGATION_TYPE_FROM_INTENT = 1;
    private static final int NAVIGATION_TYPE_FROM_USER_TYPING = 2;
    private static final int NAVIGATION_TYPE_FROM_LINK_WITHOUT_USER_GESTURE = 3;
    private static final int NAVIGATION_TYPE_FROM_RELOAD = 4;
    private static final int NAVIGATION_TYPE_OTHER = 5;

    private static class IntentState {
        final Intent mInitialIntent;
        final boolean mIsCustomTabIntent;
        final boolean mPreferToStayInChrome;
        final boolean mExternalIntentStartedTask;

        // A resolver list which includes all resolvers of |mInitialIntent|.
        HashSet<ComponentName> mCachedResolvers = new HashSet<ComponentName>();

        IntentState(Intent initialIntent, boolean preferToStayInChrome, boolean isCustomTabIntent,
                boolean externalIntentStartedTask) {
            mInitialIntent = initialIntent;
            mPreferToStayInChrome = preferToStayInChrome;
            mIsCustomTabIntent = isCustomTabIntent;
            mExternalIntentStartedTask = externalIntentStartedTask;
        }
    }

    private static class NavigationState {
        final int mInitialNavigationType;
        final boolean mHasUserStartedNonInitialNavigation;
        boolean mIsOnEffectiveRedirectChain;
        boolean mShouldNotOverrideUrlLoadingOnCurrentRedirectChain;
        boolean mShouldNotBlockOverrideUrlLoadingOnCurrentRedirectionChain;

        NavigationState(int initialNavigationType, boolean hasUserStartedNonInitialNavigation) {
            mInitialNavigationType = initialNavigationType;
            mHasUserStartedNonInitialNavigation = hasUserStartedNonInitialNavigation;
        }
    }

    private long mLastNewUrlLoadingTime = INVALID_TIME;
    private IntentState mIntentState;
    private NavigationState mNavigationState;

    // Not part of NavigationState as this should persist through resetting of the NavigationChain
    // so that the history state can be correctly set even after the tab is hidden.
    private int mLastCommittedEntryIndexBeforeStartingNavigation = INVALID_ENTRY_INDEX;

    private long mLastUserInteractionTimeMillis;

    public static RedirectHandler create() {
        return new RedirectHandler();
    }

    protected RedirectHandler() {}

    /**
     * Resets |mIntentState| for the newly received Intent.
     */
    public void updateIntent(Intent intent, boolean isCustomTabIntent, boolean sendToExternalApps,
            boolean externalIntentStartedTask) {
        if (intent == null || !Intent.ACTION_VIEW.equals(intent.getAction())) {
            mIntentState = null;
            return;
        }

        boolean preferToStayInChrome = isIntentToChrome(intent);

        // A Custom Tab Intent from a Custom Tab Session will always have the package set, so the
        // Intent will always be to Chrome. Therefore, we provide an Extra to allow the initial
        // Intent navigation chain to leave Chrome.
        if (isCustomTabIntent && sendToExternalApps) preferToStayInChrome = false;

        // A sanitized copy of the initial intent for detecting if resolvers have changed.
        Intent initialIntent = new Intent(intent);
        ExternalNavigationHandler.sanitizeQueryIntentActivitiesIntent(initialIntent);
        mIntentState = new IntentState(
                initialIntent, preferToStayInChrome, isCustomTabIntent, externalIntentStartedTask);
    }

    private static boolean isIntentToChrome(Intent intent) {
        String chromePackageName = ContextUtils.getApplicationContext().getPackageName();
        return TextUtils.equals(chromePackageName, intent.getPackage())
                || TextUtils.equals(chromePackageName,
                        IntentUtils.safeGetStringExtra(intent, Browser.EXTRA_APPLICATION_ID));
    }

    /**
     * Resets navigation and intent state.
     */
    public void clear() {
        mIntentState = null;
        mNavigationState = null;
    }

    /**
     * Will cause shouldNotOverrideUrlLoading() to return true until a new user-initiated navigation
     * occurs.
     */
    public void setShouldNotOverrideUrlLoadingOnCurrentRedirectChain() {
        mNavigationState.mShouldNotOverrideUrlLoadingOnCurrentRedirectChain = true;
    }

    /**
     * Will cause shouldNotBlockUrlLoadingOverrideOnCurrentRedirectionChain() to return true until
     * a new user-initiated navigation occurs.
     */
    public void setShouldNotBlockUrlLoadingOverrideOnCurrentRedirectionChain() {
        mNavigationState.mShouldNotBlockOverrideUrlLoadingOnCurrentRedirectionChain = true;
    }

    /**
     * @return true if the task for the Activity was created by the most recent external intent
     *         navigation to the current tab. Note that this doesn't include cold Activity starts
     *         that re-use an existing task (eg. Chrome was killed by Android without its task being
     *         swiped away or timed out).
     */
    public boolean wasTaskStartedByExternalIntent() {
        return mIntentState != null && mIntentState.mExternalIntentStartedTask;
    }

    /**
     * Updates new url loading information to trace navigation.
     * A time based heuristic is used to determine if this loading is an effective redirect or not
     * if core of |pageTransType| is LINK.
     *
     * http://crbug.com/322567 : Trace navigation started from an external app.
     * http://crbug.com/331571 : Trace navigation started from user typing to do not override such
     * navigation.
     * http://crbug.com/426679 : Trace every navigation and the last committed entry index right
     * before starting the navigation.
     *
     * @param pageTransType page transition type of this loading.
     * @param isRedirect whether this loading is http redirect or not.
     * @param hasUserGesture whether this loading is started by a user gesture.
     * @param lastUserInteractionTime time when the last user interaction was made.
     * @param lastCommittedEntryIndex the last committed entry index right before this loading.
     * @param isInitialNavigation whether this loading is for the initial navigation.
     */
    public void updateNewUrlLoading(int pageTransType, boolean isRedirect, boolean hasUserGesture,
            long lastUserInteractionTime, int lastCommittedEntryIndex,
            boolean isInitialNavigation) {
        long prevNewUrlLoadingTime = mLastNewUrlLoadingTime;
        mLastNewUrlLoadingTime = SystemClock.elapsedRealtime();
        mLastUserInteractionTimeMillis = lastUserInteractionTime;

        int pageTransitionCore = pageTransType & PageTransition.CORE_MASK;

        boolean isNewLoadingStartedByUser = mNavigationState == null;
        boolean isFromIntent = pageTransitionCore == PageTransition.LINK
                && (pageTransType & PageTransition.FROM_API) != 0;
        if (!isRedirect) {
            if ((pageTransType & PageTransition.FORWARD_BACK) != 0) {
                isNewLoadingStartedByUser = true;
            } else if (pageTransitionCore != PageTransition.LINK
                    && pageTransitionCore != PageTransition.FORM_SUBMIT) {
                isNewLoadingStartedByUser = true;
            } else if (prevNewUrlLoadingTime == INVALID_TIME || isFromIntent
                    || lastUserInteractionTime > prevNewUrlLoadingTime) {
                isNewLoadingStartedByUser = true;
            }
        }
        if (!isNewLoadingStartedByUser) {
            // Redirect chain starts from the second url loading.
            mNavigationState.mIsOnEffectiveRedirectChain = true;
            return;
        }

        // Create the NavigationState for a new Navigation chain.
        int initialNavigationType;
        if (isFromIntent && mIntentState != null) {
            initialNavigationType = NAVIGATION_TYPE_FROM_INTENT;
        } else {
            mIntentState = null;
            if (pageTransitionCore == PageTransition.TYPED) {
                initialNavigationType = NAVIGATION_TYPE_FROM_USER_TYPING;
            } else if (pageTransitionCore == PageTransition.RELOAD
                    || (pageTransType & PageTransition.FORWARD_BACK) != 0) {
                initialNavigationType = NAVIGATION_TYPE_FROM_RELOAD;
            } else if (pageTransitionCore == PageTransition.LINK && !hasUserGesture) {
                initialNavigationType = NAVIGATION_TYPE_FROM_LINK_WITHOUT_USER_GESTURE;
            } else {
                initialNavigationType = NAVIGATION_TYPE_OTHER;
            }
        }
        mNavigationState = new NavigationState(initialNavigationType, !isInitialNavigation);
        mLastCommittedEntryIndexBeforeStartingNavigation = lastCommittedEntryIndex;
    }

    /**
     * @return whether on effective intent redirect chain or not.
     */
    public boolean isOnEffectiveIntentRedirectChain() {
        return mNavigationState.mInitialNavigationType == NAVIGATION_TYPE_FROM_INTENT
                && mNavigationState.mIsOnEffectiveRedirectChain;
    }

    /**
     * @param hasExternalProtocol whether the destination URI has an external protocol or not.
     * @return whether we should stay in Chrome or not.
     */
    public boolean shouldStayInApp(boolean hasExternalProtocol) {
        return shouldStayInApp(hasExternalProtocol, false);
    }

    /**
     * @param hasExternalProtocol whether the destination URI has an external protocol or not.
     * @param isForTrustedCallingApp whether the app we would launch to is trusted and what launched
     *                               Chrome.
     * @return whether we should stay in Chrome or not.
     */
    public boolean shouldStayInApp(boolean hasExternalProtocol, boolean isForTrustedCallingApp) {
        // http://crbug/424029 : Need to stay in Chrome for an intent heading explicitly to Chrome.
        // http://crbug/881740 : Relax stay in Chrome restriction for Custom Tabs.
        return (mIntentState != null && mIntentState.mPreferToStayInChrome && !hasExternalProtocol)
                || shouldNavigationTypeStayInApp(isForTrustedCallingApp);
    }

    /**
     * @return Whether the current navigation is of the type that should always stay in Chrome.
     */
    public boolean shouldNavigationTypeStayInApp() {
        return shouldNavigationTypeStayInApp(false);
    }

    private boolean shouldNavigationTypeStayInApp(boolean isForTrustedCallingApp) {
        // http://crbug.com/162106: Never leave Chrome from a refresh.
        if (mNavigationState.mInitialNavigationType == NAVIGATION_TYPE_FROM_RELOAD) return true;

        // If the app we would navigate to is trusted and what launched Chrome, allow the
        // navigation.
        if (isForTrustedCallingApp) return false;

        // Otherwise allow navigation out of the app only with a user gesture.
        return mNavigationState.mInitialNavigationType
                == NAVIGATION_TYPE_FROM_LINK_WITHOUT_USER_GESTURE;
    }

    /**
     * @return Whether this navigation is initiated by a Custom Tabs {@link Intent}.
     */
    public boolean isFromCustomTabIntent() {
        return mIntentState != null && mIntentState.mIsCustomTabIntent;
    }

    /**
     * @return whether navigation is from a user's typing or not.
     */
    public boolean isNavigationFromUserTyping() {
        return mNavigationState.mInitialNavigationType == NAVIGATION_TYPE_FROM_USER_TYPING;
    }

    /**
     * @return whether we should stay in Chrome or not.
     */
    public boolean shouldNotOverrideUrlLoading() {
        return mNavigationState.mShouldNotOverrideUrlLoadingOnCurrentRedirectChain;
    }

    /**
     * @return whether we should continue allowing navigation handling in the current redirection
     * chain.
     */
    public boolean getAndClearShouldNotBlockOverrideUrlLoadingOnCurrentRedirectionChain() {
        boolean value = mNavigationState.mShouldNotBlockOverrideUrlLoadingOnCurrentRedirectionChain;
        mNavigationState.mShouldNotBlockOverrideUrlLoadingOnCurrentRedirectionChain = false;
        return value;
    }

    /**
     * @return whether on navigation or not.
     */
    public boolean isOnNavigation() {
        return mNavigationState != null;
    }

    /**
     * @return the last committed entry index which was saved before starting this navigation.
     */
    public int getLastCommittedEntryIndexBeforeStartingNavigation() {
        assert mLastCommittedEntryIndexBeforeStartingNavigation != INVALID_ENTRY_INDEX;
        return mLastCommittedEntryIndexBeforeStartingNavigation;
    }

    /**
     * @return whether the user has started a non-initial navigation.
     */
    public boolean hasUserStartedNonInitialNavigation() {
        return mNavigationState != null && mNavigationState.mHasUserStartedNonInitialNavigation;
    }

    /**
     * @return whether |intent| has a new resolver against |mIntentHistory| or not.
     */
    public boolean hasNewResolver(List<ResolveInfo> resolvingInfos,
            Function<Intent, List<ResolveInfo>> queryIntentActivitiesFunction) {
        if (mIntentState == null) return !resolvingInfos.isEmpty();

        if (mIntentState.mCachedResolvers.isEmpty()) {
            for (ResolveInfo r : queryIntentActivitiesFunction.apply(mIntentState.mInitialIntent)) {
                mIntentState.mCachedResolvers.add(
                        new ComponentName(r.activityInfo.packageName, r.activityInfo.name));
            }
        }
        if (resolvingInfos.size() > mIntentState.mCachedResolvers.size()) return true;
        for (ResolveInfo r : resolvingInfos) {
            if (!mIntentState.mCachedResolvers.contains(
                        new ComponentName(r.activityInfo.packageName, r.activityInfo.name))) {
                return true;
            }
        }
        return false;
    }

    /**
     * @return The initial intent of a redirect chain, if available.
     */
    public Intent getInitialIntent() {
        return mIntentState != null ? mIntentState.mInitialIntent : null;
    }

    public void maybeLogExternalRedirectBlockedWithMissingGesture() {
        if (mNavigationState.mInitialNavigationType
                == NAVIGATION_TYPE_FROM_LINK_WITHOUT_USER_GESTURE) {
            long millisSinceLastGesture =
                    SystemClock.elapsedRealtime() - mLastUserInteractionTimeMillis;
            Log.w(TAG,
                    "External navigation blocked due to missing gesture. Last input was "
                            + millisSinceLastGesture + "ms ago.");
            RecordHistogram.recordTimesHistogram(
                    "Android.Intent.BlockedExternalNavLastGestureTime", millisSinceLastGesture);
        }
    }
}
