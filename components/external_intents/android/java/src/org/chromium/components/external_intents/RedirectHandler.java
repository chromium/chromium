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
import org.chromium.base.IntentUtils;
import org.chromium.base.PackageManagerUtils;
import org.chromium.ui.base.PageTransition;

import java.util.HashSet;
import java.util.List;

/**
 * This class contains the logic to determine effective navigation/redirect.
 */
public class RedirectHandler {
    /**
     * An invalid entry index.
     */
    public static final int INVALID_ENTRY_INDEX = -1;
    public static final long INVALID_TIME = -1;

    private static final int NAVIGATION_TYPE_NONE = 0;
    private static final int NAVIGATION_TYPE_FROM_INTENT = 1;
    private static final int NAVIGATION_TYPE_FROM_USER_TYPING = 2;
    private static final int NAVIGATION_TYPE_FROM_LINK_WITHOUT_USER_GESTURE = 3;
    private static final int NAVIGATION_TYPE_FROM_RELOAD = 4;
    private static final int NAVIGATION_TYPE_OTHER = 5;

    private Intent mInitialIntent;
    // A resolver list which includes all resolvers of |mInitialIntent|.
    private final HashSet<ComponentName> mCachedResolvers = new HashSet<ComponentName>();
    private boolean mIsInitialIntentHeadingToChrome;
    private boolean mIsCustomTabIntent;

    private long mLastNewUrlLoadingTime = INVALID_TIME;
    private boolean mIsOnEffectiveRedirectChain;
    private int mInitialNavigationType;
    private int mLastCommittedEntryIndexBeforeStartingNavigation;

    private boolean mShouldNotOverrideUrlLoadingOnCurrentRedirectChain;
    private boolean mShouldNotBlockOverrideUrlLoadingOnCurrentRedirectionChain;
    private boolean mExternalIntentStartedTask;

    public static RedirectHandler create() {
        return new RedirectHandler();
    }

    protected RedirectHandler() {}

    /**
     * Updates |mIntentHistory| and |mLastIntentUpdatedTime|. If |intent| comes from chrome and
     * currently |mIsOnEffectiveIntentRedirectChain| is true, that means |intent| was sent from
     * this tab because only the front tab or a new tab can receive an intent from chrome. In that
     * case, |intent| is added to |mIntentHistory|.
     * Otherwise, |mIntentHistory| and |mPreviousResolvers| are cleared, and then |intent| is put
     * into |mIntentHistory|.
     */
    public void updateIntent(Intent intent, boolean isCustomTabIntent, boolean sendToExternalApps,
            boolean isCCTExternalLinkHandlingEnabled, boolean externalIntentStartedTask) {
        clear();

        if (intent == null || !Intent.ACTION_VIEW.equals(intent.getAction())) {
            return;
        }

        mIsCustomTabIntent = isCustomTabIntent;
        mExternalIntentStartedTask = externalIntentStartedTask;
        boolean checkIsToChrome = true;
        // All custom tabs VIEW intents are by design explicit intents, so the presence of package
        // name doesn't imply they have to be handled by Chrome explicitly. Check if external apps
        // should be checked for handling the initial redirect chain.
        if (mIsCustomTabIntent) {
            checkIsToChrome = !(sendToExternalApps && isCCTExternalLinkHandlingEnabled);
        }

        if (checkIsToChrome) mIsInitialIntentHeadingToChrome = isIntentToChrome(intent);

        // A copy of the intent with component cleared to find resolvers.
        mInitialIntent = new Intent(intent).setComponent(null);
        Intent selector = mInitialIntent.getSelector();
        if (selector != null) selector.setComponent(null);
    }

    private static boolean isIntentToChrome(Intent intent) {
        String chromePackageName = ContextUtils.getApplicationContext().getPackageName();
        return TextUtils.equals(chromePackageName, intent.getPackage())
                || TextUtils.equals(chromePackageName,
                        IntentUtils.safeGetStringExtra(intent, Browser.EXTRA_APPLICATION_ID));
    }

    private void clearIntentHistory() {
        mIsInitialIntentHeadingToChrome = false;
        mIsCustomTabIntent = false;
        mInitialIntent = null;
        mExternalIntentStartedTask = false;
        mCachedResolvers.clear();
    }

    /**
     * Resets all variables except timestamps.
     */
    public void clear() {
        clearIntentHistory();
        mInitialNavigationType = NAVIGATION_TYPE_NONE;
        mIsOnEffectiveRedirectChain = false;
        mLastCommittedEntryIndexBeforeStartingNavigation = 0;
        mShouldNotOverrideUrlLoadingOnCurrentRedirectChain = false;
        mShouldNotBlockOverrideUrlLoadingOnCurrentRedirectionChain = false;
    }

    /**
     * Will cause shouldNotOverrideUrlLoading() to return true until a new user-initiated navigation
     * occurs.
     */
    public void setShouldNotOverrideUrlLoadingOnCurrentRedirectChain() {
        mShouldNotOverrideUrlLoadingOnCurrentRedirectChain = true;
    }

    /**
     * Will cause shouldNotBlockUrlLoadingOverrideOnCurrentRedirectionChain() to return true until
     * a new user-initiated navigation occurs.
     */
    public void setShouldNotBlockUrlLoadingOverrideOnCurrentRedirectionChain() {
        mShouldNotBlockOverrideUrlLoadingOnCurrentRedirectionChain = true;
    }

    /**
     * @return true if the task for the Activity was created by the most recent external intent
     *         navigation to the current tab. Note that this doesn't include cold Activity starts
     *         that re-use an existing task (eg. Chrome was killed by Android without its task being
     *         swiped away or timed out).
     */
    public boolean wasTaskStartedByExternalIntent() {
        return mExternalIntentStartedTask;
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
     */
    public void updateNewUrlLoading(int pageTransType, boolean isRedirect, boolean hasUserGesture,
            long lastUserInteractionTime, int lastCommittedEntryIndex) {
        long prevNewUrlLoadingTime = mLastNewUrlLoadingTime;
        mLastNewUrlLoadingTime = SystemClock.elapsedRealtime();

        int pageTransitionCore = pageTransType & PageTransition.CORE_MASK;

        boolean isNewLoadingStartedByUser = false;
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

        if (isNewLoadingStartedByUser) {
            // Updates mInitialNavigationType for a new loading started by a user's gesture.
            if (isFromIntent && mInitialIntent != null) {
                mInitialNavigationType = NAVIGATION_TYPE_FROM_INTENT;
            } else {
                clearIntentHistory();
                if (pageTransitionCore == PageTransition.TYPED) {
                    mInitialNavigationType = NAVIGATION_TYPE_FROM_USER_TYPING;
                } else if (pageTransitionCore == PageTransition.RELOAD
                        || (pageTransType & PageTransition.FORWARD_BACK) != 0) {
                    mInitialNavigationType = NAVIGATION_TYPE_FROM_RELOAD;
                } else if (pageTransitionCore == PageTransition.LINK && !hasUserGesture) {
                    mInitialNavigationType = NAVIGATION_TYPE_FROM_LINK_WITHOUT_USER_GESTURE;
                } else {
                    mInitialNavigationType = NAVIGATION_TYPE_OTHER;
                }
            }
            mIsOnEffectiveRedirectChain = false;
            mLastCommittedEntryIndexBeforeStartingNavigation = lastCommittedEntryIndex;
            mShouldNotOverrideUrlLoadingOnCurrentRedirectChain = false;
            mShouldNotBlockOverrideUrlLoadingOnCurrentRedirectionChain = false;
        } else if (mInitialNavigationType != NAVIGATION_TYPE_NONE) {
            // Redirect chain starts from the second url loading.
            mIsOnEffectiveRedirectChain = true;
        }
    }

    /**
     * @return whether on effective intent redirect chain or not.
     */
    public boolean isOnEffectiveIntentRedirectChain() {
        return mInitialNavigationType == NAVIGATION_TYPE_FROM_INTENT && mIsOnEffectiveRedirectChain;
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
        return (mIsInitialIntentHeadingToChrome && !hasExternalProtocol)
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
        if (mInitialNavigationType == NAVIGATION_TYPE_FROM_RELOAD) return true;

        // If the app we would navigate to is trusted and what launched Chrome, allow the
        // navigation.
        if (isForTrustedCallingApp) return false;

        // Otherwise allow navigation out of the app only with a user gesture.
        return mInitialNavigationType == NAVIGATION_TYPE_FROM_LINK_WITHOUT_USER_GESTURE;
    }

    /**
     * @return Whether this navigation is initiated by a Custom Tabs {@link Intent}.
     */
    public boolean isFromCustomTabIntent() {
        return mIsCustomTabIntent;
    }

    /**
     * @return whether navigation is from a user's typing or not.
     */
    public boolean isNavigationFromUserTyping() {
        return mInitialNavigationType == NAVIGATION_TYPE_FROM_USER_TYPING;
    }

    /**
     * @return whether we should stay in Chrome or not.
     */
    public boolean shouldNotOverrideUrlLoading() {
        return mShouldNotOverrideUrlLoadingOnCurrentRedirectChain;
    }

    /**
     * @return whether we should continue allowing navigation handling in the current redirection
     * chain.
     */
    public boolean getAndClearShouldNotBlockOverrideUrlLoadingOnCurrentRedirectionChain() {
        boolean value = mShouldNotBlockOverrideUrlLoadingOnCurrentRedirectionChain;
        mShouldNotBlockOverrideUrlLoadingOnCurrentRedirectionChain = false;
        return value;
    }

    /**
     * @return whether on navigation or not.
     */
    public boolean isOnNavigation() {
        return mInitialNavigationType != NAVIGATION_TYPE_NONE;
    }

    /**
     * @return the last committed entry index which was saved before starting this navigation.
     */
    public int getLastCommittedEntryIndexBeforeStartingNavigation() {
        return mLastCommittedEntryIndexBeforeStartingNavigation;
    }

    /**
     * @return whether |intent| has a new resolver against |mIntentHistory| or not.
     */
    public boolean hasNewResolver(List<ResolveInfo> resolvingInfos) {
        if (mInitialIntent == null) {
            return !resolvingInfos.isEmpty();
        }

        if (mCachedResolvers.isEmpty()) {
            for (ResolveInfo r : PackageManagerUtils.queryIntentActivities(mInitialIntent, 0)) {
                mCachedResolvers.add(
                        new ComponentName(r.activityInfo.packageName, r.activityInfo.name));
            }
        }
        if (resolvingInfos.size() > mCachedResolvers.size()) return true;
        for (ResolveInfo r : resolvingInfos) {
            if (!mCachedResolvers.contains(
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
        return mInitialIntent;
    }
}
