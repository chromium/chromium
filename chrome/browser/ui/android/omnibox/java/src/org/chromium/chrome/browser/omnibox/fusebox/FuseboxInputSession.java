// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.omnibox.fusebox;

import androidx.annotation.VisibleForTesting;

import org.chromium.build.annotations.NullMarked;
import org.chromium.build.annotations.Nullable;
import org.chromium.chrome.browser.profiles.Profile;

import java.util.Objects;

/**
 * Fusebox / Omnibox session state object. Captures controllers and state details needed to fulfill
 * or reconstruct the user input.
 *
 * <p>A non-null instance of this class indicates that the Fusebox is active and the input session
 * has begun. A null instance indicates that the input is no longer active.
 *
 * <p>The logic should be kept to minimum in this class.
 */
@NullMarked
public class FuseboxInputSession {
    /** The Profile associated with the current Omnibox session. */
    public final Profile profile;

    /** The ComposeBoxQueryControllerBridge for the current session, if available. */
    public final @Nullable ComposeBoxQueryControllerBridge composeBoxController;

    /**
     * Creates a new FuseboxInputSession for the given Profile.
     *
     * @param profile The Profile to create the session for.
     * @return A new FuseboxInputSession, or null if ineligible for given parameters.
     */
    public static @Nullable FuseboxInputSession createForProfile(@Nullable Profile profile) {
        if (profile == null) return null;
        return new FuseboxInputSession(profile);
    }

    private FuseboxInputSession(Profile profile) {
        this(profile, ComposeBoxQueryControllerBridge.getForProfile(profile));
    }

    @VisibleForTesting
    public FuseboxInputSession(
            Profile profile,
            @Nullable ComposeBoxQueryControllerBridge composeBoxQueryControllerBridge) {
        this.profile = profile;
        this.composeBoxController = composeBoxQueryControllerBridge;
    }

    @Override
    public int hashCode() {
        return Objects.hash(profile, composeBoxController);
    }

    @Override
    public boolean equals(Object obj) {
        if (!(obj instanceof FuseboxInputSession)) return false;

        var other = (FuseboxInputSession) obj;
        return Objects.equals(profile, other.profile)
                && Objects.equals(composeBoxController, other.composeBoxController);
    }
}
