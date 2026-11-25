// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.ui.signin.fullscreen_signin;

import org.chromium.build.annotations.NullMarked;
import org.chromium.components.signin.metrics.SigninFunnelEvent;

/**
 * Side effects returned by {@link FullscreenSigninReducer} for the mediator to run.
 *
 * <p>Effects without data are singletons ({@code INSTANCE}), so they compare by identity.
 */
@NullMarked
sealed interface FullscreenSigninEffect {
    /** Calls {@code Delegate.recordNativeInitializedHistogram()}. */
    final class RecordNativeInitialized implements FullscreenSigninEffect {
        static final RecordNativeInitialized INSTANCE = new RecordNativeInitialized();

        private RecordNativeInitialized() {}
    }

    /**
     * Creates the {@link org.chromium.chrome.browser.signin.services.ProfileDataCache}, then
     * dispatches {@link FullscreenSigninEvent.ProfileDataCacheCreated}.
     */
    final class CreateProfileDataCache implements FullscreenSigninEffect {
        static final CreateProfileDataCache INSTANCE = new CreateProfileDataCache();

        private CreateProfileDataCache() {}
    }

    /**
     * Subscribes to native initialization, and to the policies and child account status as
     * requested.
     */
    final class SubscribeToLoadInputs implements FullscreenSigninEffect {
        private final boolean mIncludePolicies;
        private final boolean mIncludeChildStatus;

        SubscribeToLoadInputs(boolean includePolicies, boolean includeChildStatus) {
            mIncludePolicies = includePolicies;
            mIncludeChildStatus = includeChildStatus;
        }

        boolean includesPolicies() {
            return mIncludePolicies;
        }

        boolean includesChildStatus() {
            return mIncludeChildStatus;
        }
    }

    /**
     * Checks whether sign-in is supported, then dispatches {@link
     * FullscreenSigninEvent.SigninSupportChecked} synchronously.
     */
    final class CheckSigninSupport implements FullscreenSigninEffect {
        static final CheckSigninSupport INSTANCE = new CheckSigninSupport();

        private CheckSigninSupport() {}
    }

    /** Calls {@code SigninMetricsUtils.logSigninFunnelEvent(accessPoint, funnelEvent)}. */
    final class RecordSigninFunnelEvent implements FullscreenSigninEffect {
        private final @SigninFunnelEvent int mFunnelEvent;

        RecordSigninFunnelEvent(@SigninFunnelEvent int funnelEvent) {
            mFunnelEvent = funnelEvent;
        }

        @SigninFunnelEvent
        int getFunnelEvent() {
            return mFunnelEvent;
        }
    }

    /** Calls {@code Delegate.onInitialLoadCompleted()}. */
    final class NotifyInitialLoadCompleted implements FullscreenSigninEffect {
        static final NotifyInitialLoadCompleted INSTANCE = new NotifyInitialLoadCompleted();

        private NotifyInitialLoadCompleted() {}
    }

    /** Calls {@code Delegate.recordLoadCompletedHistograms(slowestLoadPoint)}. */
    final class RecordLoadCompleted implements FullscreenSigninEffect {
        private final @FullscreenSigninLoadPoint int mSlowestLoadPoint;

        RecordLoadCompleted(@FullscreenSigninLoadPoint int slowestLoadPoint) {
            mSlowestLoadPoint = slowestLoadPoint;
        }

        @FullscreenSigninLoadPoint
        int getSlowestLoadPoint() {
            return mSlowestLoadPoint;
        }
    }
}
