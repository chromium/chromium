// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.ui.signin.fullscreen_signin;

import static org.junit.Assert.assertEquals;
import static org.junit.Assert.assertFalse;
import static org.junit.Assert.assertTrue;

import org.junit.Test;
import org.junit.runner.RunWith;

import org.chromium.base.test.BaseRobolectricTestRunner;
import org.chromium.build.annotations.NullMarked;
import org.chromium.chrome.browser.ui.signin.fullscreen_signin.FullscreenSigninEffect.CreateProfileDataCache;
import org.chromium.chrome.browser.ui.signin.fullscreen_signin.FullscreenSigninEffect.RecordNativeInitialized;
import org.chromium.chrome.browser.ui.signin.fullscreen_signin.FullscreenSigninEvent.AccountsUpdated;
import org.chromium.chrome.browser.ui.signin.fullscreen_signin.FullscreenSigninEvent.NativeReady;
import org.chromium.chrome.browser.ui.signin.fullscreen_signin.FullscreenSigninReducer.Result;
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
        FullscreenSigninState state = new FullscreenSigninState();
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
}
