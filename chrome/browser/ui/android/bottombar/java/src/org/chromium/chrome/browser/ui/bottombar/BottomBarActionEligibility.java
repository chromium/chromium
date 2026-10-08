// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.ui.bottombar;

import org.chromium.base.ResettersForTesting;
import org.chromium.build.annotations.NullMarked;
import org.chromium.build.annotations.Nullable;
import org.chromium.chrome.browser.glic.GlicEnabling;
import org.chromium.chrome.browser.profiles.Profile;
import org.chromium.chrome.browser.ui.actions.ActionId;
import org.chromium.chrome.browser.ui.bottombar.BottomBarMetrics.GlicIneligibilityReason;

/** Helper class to resolve the eligibility of bottom bar actions based on profile. */
@NullMarked
public class BottomBarActionEligibility {

    /** Represents a sentinel value indicating that no action is eligible. */
    public static final @ActionId int ACTION_NONE = ActionId.NONE;

    private static @Nullable @ActionId Integer sCachedCandidateExtraAction;

    /** Returns the currently cached candidate extra action, or null if uninitialized. */
    public static @Nullable @ActionId Integer getCachedCandidateExtraAction() {
        return sCachedCandidateExtraAction;
    }

    /** Sets the cached candidate extra action for testing. */
    public static void setCachedCandidateExtraActionForTesting(
            @Nullable @ActionId Integer candidate) {
        sCachedCandidateExtraAction = candidate;
        ResettersForTesting.register(() -> sCachedCandidateExtraAction = null);
    }

    /**
     * Returns whether the GLIC bottom bar setting toggle should be shown for the given profile.
     *
     * <p>The toggle is only shown if candidate resolution has already resolved GLIC as the
     * candidate extra action for the bottom bar. If candidate resolution has not occurred yet or
     * resolved to another action / none, returns false.
     *
     * @param profile The current user profile.
     * @return True if the setting toggle should be shown.
     */
    public static boolean shouldShowBottomBarGlicSetting(@Nullable Profile profile) {
        if (profile == null) {
            return false;
        }
        if (sCachedCandidateExtraAction == null || sCachedCandidateExtraAction != ActionId.GLIC) {
            return false;
        }
        Profile originalProfile = profile.getOriginalProfile();
        return BottomBarConfigUtils.isGlicSettingToggleParamEnabled()
                && GlicEnabling.shouldShowSettingsPage(originalProfile);
    }

    /**
     * Resolves and caches the static candidate action (if any) that can be displayed in the bottom
     * bar's shared extra container for the given profile.
     *
     * <p>This is a pure resolver and does not record eligibility metrics; callers that need to emit
     * startup ineligibility metrics should call {@link
     * #recordGlicIneligibilityReasonIfNeeded(Profile)}.
     *
     * @param profile The current user profile.
     * @return The candidate {@link ActionId} ({@link ActionId#GLIC}), or {@link #ACTION_NONE} if no
     *     action is eligible.
     */
    @ActionId
    public static int getCandidateExtraAction(@Nullable Profile profile) {
        if (profile == null) {
            return ACTION_NONE;
        }

        Profile originalProfile = profile.getOriginalProfile();
        boolean isGlicProfileEnabled = GlicEnabling.isEnabledForProfile(originalProfile);

        // Check if GLIC is enabled for this profile.
        if (isGlicProfileEnabled) {
            sCachedCandidateExtraAction = ActionId.GLIC;
            if (GlicEnabling.isPolicyEnforced(originalProfile)) {
                return ActionId.GLIC;
            }
            if (BottomBarConfigUtils.isGlicButtonEnabled()) {
                return ActionId.GLIC;
            }
            return ACTION_NONE;
        }

        sCachedCandidateExtraAction = ACTION_NONE;
        return ACTION_NONE;
    }

    /**
     * Records the reason why GLIC is ineligible to be shown in the bottom bar for the given
     * profile, if GLIC is not currently eligible.
     *
     * @param profile The current user profile.
     */
    public static void recordGlicIneligibilityReasonIfNeeded(@Nullable Profile profile) {
        if (profile == null) {
            return;
        }

        Profile originalProfile = profile.getOriginalProfile();
        if (!GlicEnabling.isEnabledForProfile(originalProfile)) {
            BottomBarMetrics.recordGlicIneligibilityReason(
                    GlicIneligibilityReason.PROFILE_INELIGIBLE);
            return;
        }

        if (!GlicEnabling.isPolicyEnforced(originalProfile)
                && !BottomBarConfigUtils.isGlicButtonEnabled()) {
            BottomBarMetrics.recordGlicIneligibilityReason(
                    GlicIneligibilityReason.USER_DISABLED_IN_SETTINGS);
        }
    }
}
