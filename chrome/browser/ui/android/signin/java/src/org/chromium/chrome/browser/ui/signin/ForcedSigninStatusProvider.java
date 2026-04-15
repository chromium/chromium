// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.ui.signin;

import org.chromium.base.ResettersForTesting;
import org.chromium.build.annotations.NullMarked;
import org.chromium.build.annotations.Nullable;
import org.chromium.chrome.browser.profiles.Profile;
import org.chromium.chrome.browser.profiles.ProfileKeyedMap;
import org.chromium.ui.util.TokenHolder;

/**
 * Provides the status of the forced sign-in screen. This is used to determine whether the forced
 * sign-in prompt is currently being displayed, which can be used to suppress other UI elements
 * (like context menus) that shouldn't appear during the sign-in flow.
 */
@NullMarked
public class ForcedSigninStatusProvider {
    private static final ProfileKeyedMap<ForcedSigninStatusProvider> sProfileMap =
            new ProfileKeyedMap<>(ProfileKeyedMap.noRequiredCleanupAction());

    @Nullable private static ForcedSigninStatusProvider sInstanceForTesting;
    private final TokenHolder mShownForcedSigninScreens = new TokenHolder(() -> {});

    /**
     * Returns the {@link ForcedSigninStatusProvider} for the provided profile and creates a new
     * instance if there isn't one already.
     */
    public static ForcedSigninStatusProvider getForProfile(Profile profile) {
        if (sInstanceForTesting != null) {
            return sInstanceForTesting;
        }
        return sProfileMap.getForProfile(profile, ForcedSigninStatusProvider::new);
    }

    private ForcedSigninStatusProvider(Profile profile) {}

    /**
     * Records that a forced sign-in screen is being shown and returns a token that can later be
     * used to signal that the screen has been hidden.
     *
     * @return A token representing the showing state of the forced sign-in screen.
     */
    public int showForcedSigninScreen() {
        return mShownForcedSigninScreens.acquireToken();
    }

    /**
     * Records that a forced sign-in screen has been hidden, releasing the given token.
     *
     * @param token The token returned by {@link #showForcedSigninScreen()} when the screen was
     *     shown.
     */
    public void hideForcedSigninScreen(int token) {
        mShownForcedSigninScreens.releaseToken(token);
    }

    /**
     * Returns whether the forced sign-in screen is currently showing.
     *
     * @return True if the forced sign-in screen is showing in the current browser session.
     */
    public boolean isForcedSigninShowing() {
        return mShownForcedSigninScreens.hasTokens();
    }

    public static void setInstanceForTesting(
            ForcedSigninStatusProvider forcedSigninStatusProvider) {
        sInstanceForTesting = forcedSigninStatusProvider;
        ResettersForTesting.register(() -> sInstanceForTesting = null);
    }
}
