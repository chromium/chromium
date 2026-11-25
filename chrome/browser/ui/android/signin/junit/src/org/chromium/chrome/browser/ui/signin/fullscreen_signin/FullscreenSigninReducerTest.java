// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.ui.signin.fullscreen_signin;

import static org.junit.Assert.assertEquals;
import static org.junit.Assert.assertFalse;
import static org.junit.Assert.assertSame;
import static org.junit.Assert.assertTrue;

import org.junit.Test;
import org.junit.runner.RunWith;

import org.chromium.base.test.BaseRobolectricTestRunner;
import org.chromium.build.annotations.NullMarked;
import org.chromium.chrome.browser.ui.signin.fullscreen_signin.FullscreenSigninEffect.CheckSigninSupport;
import org.chromium.chrome.browser.ui.signin.fullscreen_signin.FullscreenSigninEffect.CreateProfileDataCache;
import org.chromium.chrome.browser.ui.signin.fullscreen_signin.FullscreenSigninEffect.NotifyInitialLoadCompleted;
import org.chromium.chrome.browser.ui.signin.fullscreen_signin.FullscreenSigninEffect.RecordLoadCompleted;
import org.chromium.chrome.browser.ui.signin.fullscreen_signin.FullscreenSigninEffect.RecordNativeInitialized;
import org.chromium.chrome.browser.ui.signin.fullscreen_signin.FullscreenSigninEffect.RecordSigninFunnelEvent;
import org.chromium.chrome.browser.ui.signin.fullscreen_signin.FullscreenSigninEvent.AccountsUpdated;
import org.chromium.chrome.browser.ui.signin.fullscreen_signin.FullscreenSigninEvent.NativeReady;
import org.chromium.chrome.browser.ui.signin.fullscreen_signin.FullscreenSigninEvent.SigninSupportChecked;
import org.chromium.chrome.browser.ui.signin.fullscreen_signin.FullscreenSigninReducer.Result;
import org.chromium.chrome.browser.ui.signin.fullscreen_signin.FullscreenSigninState.Phase;
import org.chromium.components.signin.metrics.SigninFunnelEvent;
import org.chromium.components.signin.test.util.TestAccounts;

import java.util.List;

/**
 * Unit tests for {@link FullscreenSigninReducer}.
 *
 * <p>Behaviour visible outside the screen is tested through {@link FullscreenSigninMediatorTest}.
 * These tests cover only transitions that can't be observed or set up through the mediator.
 */
@RunWith(BaseRobolectricTestRunner.class)
@NullMarked
public class FullscreenSigninReducerTest {
    @Test
    public void testNativeReady_recordsNativeInitializedThenCreatesProfileDataCache() {
        // Native is only subscribed to once construction has finished.
        FullscreenSigninState state =
                new FullscreenSigninState().buildUpon().setConstructionFinished().build();
        assertFalse(state.isNativeLoaded());

        Result result = FullscreenSigninReducer.reduce(state, new NativeReady());

        assertTrue(result.getState().isNativeLoaded());
        // The native-initialized histogram must be recorded before the cache is created.
        assertEquals(
                List.of(RecordNativeInitialized.INSTANCE, CreateProfileDataCache.INSTANCE),
                result.getEffects());
    }

    @Test
    public void testAccountsUpdated_selectsFirstAccount() {
        Result result =
                FullscreenSigninReducer.reduce(
                        new FullscreenSigninState(),
                        new AccountsUpdated(
                                List.of(
                                        TestAccounts.ACCOUNT1.getId(),
                                        TestAccounts.ACCOUNT2.getId())));

        assertEquals(TestAccounts.ACCOUNT1.getId(), result.getState().getSelectedAccountId());
        assertTrue(result.getEffects().isEmpty());
    }

    @Test
    public void testAccountsUpdatedWhileFinalizingLoad_checksSigninSupportOnceWithLatestAccount() {
        // SigninSupportChecked is queued behind any pending events, so an account update can be
        // handled first. The mediator tests can't set this up.
        FullscreenSigninState state =
                new FullscreenSigninState()
                        .buildUpon()
                        .setConstructionFinished()
                        .setNativeLoaded()
                        .setPoliciesLoaded()
                        .setChildStatusLoaded()
                        .build();

        Result request = FullscreenSigninReducer.reduce(state, new AccountsUpdated(List.of()));
        assertEquals(Phase.FINALIZING_LOAD, request.getState().getPhase());
        assertEquals(List.of(CheckSigninSupport.INSTANCE), request.getEffects());

        Result update =
                FullscreenSigninReducer.reduce(
                        request.getState(),
                        new AccountsUpdated(List.of(TestAccounts.ACCOUNT1.getId())));
        assertEquals(Phase.FINALIZING_LOAD, update.getState().getPhase());
        assertTrue(update.getEffects().isEmpty());

        Result response =
                FullscreenSigninReducer.reduce(update.getState(), new SigninSupportChecked(true));
        assertEquals(Phase.READY, response.getState().getPhase());
        assertEquals(3, response.getEffects().size());
        assertEquals(
                SigninFunnelEvent.PROMO_SHOWN_WITH_ACCOUNT,
                ((RecordSigninFunnelEvent) response.getEffects().get(0)).getFunnelEvent());
        assertSame(NotifyInitialLoadCompleted.INSTANCE, response.getEffects().get(1));
        assertEquals(
                FullscreenSigninLoadPoint.ACCOUNT_FETCHING,
                ((RecordLoadCompleted) response.getEffects().get(2)).getSlowestLoadPoint());
    }
}
