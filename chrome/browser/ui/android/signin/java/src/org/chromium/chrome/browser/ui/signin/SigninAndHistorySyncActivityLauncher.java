// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.ui.signin;

import android.content.Context;
import android.content.Intent;

import androidx.annotation.IntDef;
import androidx.annotation.MainThread;

import org.chromium.build.annotations.NullMarked;
import org.chromium.build.annotations.Nullable;
import org.chromium.chrome.browser.profiles.Profile;
import org.chromium.components.signin.metrics.SigninAccessPoint;

import java.lang.annotation.Retention;
import java.lang.annotation.RetentionPolicy;

/** Allows for launching {@link SigninAndHistorySyncActivity} in modularized code. */
@NullMarked
public interface SigninAndHistorySyncActivityLauncher {
    /** Sign-in access points that are eligible to the sign-in and history opt-in flow. */
    @IntDef({
        SigninAccessPoint.RECENT_TABS,
        SigninAccessPoint.BOOKMARK_MANAGER,
        SigninAccessPoint.HISTORY_PAGE,
        SigninAccessPoint.NTP_FEED_TOP_PROMO,
        SigninAccessPoint.NTP_FEED_BOTTOM_PROMO,
        SigninAccessPoint.SAFETY_CHECK,
        SigninAccessPoint.SETTINGS,
        SigninAccessPoint.WEB_SIGNIN,
        SigninAccessPoint.NTP_SIGNED_OUT_ICON,
        SigninAccessPoint.NTP_FEED_CARD_MENU_PROMO,
        SigninAccessPoint.SEND_TAB_TO_SELF_PROMO,
        SigninAccessPoint.CCT_ACCOUNT_MISMATCH_NOTIFICATION,
        SigninAccessPoint.COLLABORATION_JOIN_TAB_GROUP,
        SigninAccessPoint.COLLABORATION_SHARE_TAB_GROUP,
        SigninAccessPoint.COLLABORATION_LEAVE_OR_DELETE_TAB_GROUP,
        SigninAccessPoint.HISTORY_SYNC_EDUCATIONAL_TIP,
    })
    @Retention(RetentionPolicy.SOURCE)
    @interface AccessPoint {}

    /**
     * Create {@Intent} for the {@link SigninAndHistorySyncActivity} from an eligible access point,
     * Show an error if the intent can't be created.
     *
     * @param profile the current profile.
     * @param config The object containing configurations for the sign-in & history sync views.
     * @param accessPoint The access point from which the sign-in was triggered.
     */
    @MainThread
    @Nullable Intent createBottomSheetSigninIntentOrShowError(
            Context context,
            Profile profile,
            BottomSheetSigninAndHistorySyncConfig config,
            @AccessPoint int accessPoint);

    /**
     * Create {@Intent} for the fullscreen flavor of the {@link SigninAndHistorySyncActivity} if
     * sign-in and history opt-in are allowed. Does not show any error if the intent can't be
     * created.
     *
     * @param config The object containing IDS of resources for the sign-in & history sync views.
     * @param accessPoint The access point from which the sign-in was triggered.
     */
    @MainThread
    @Nullable
    Intent createFullscreenSigninIntent(
            Context context,
            Profile profile,
            FullscreenSigninAndHistorySyncConfig config,
            @SigninAccessPoint int signinAccessPoint);

    /**
     * Create {@Intent} for the fullscreen flavor of the {@link SigninAndHistorySyncActivity} if
     * sign-in and history opt-in are allowed. Show an error if the intent can't be created.
     *
     * @param config The object containing IDS of resources for the sign-in & history sync views.
     * @param accessPoint The access point from which the sign-in was triggered.
     */
    @MainThread
    @Nullable
    Intent createFullscreenSigninIntentOrShowError(
            Context context,
            Profile profile,
            FullscreenSigninAndHistorySyncConfig config,
            @SigninAccessPoint int signinAccessPoint);
}
