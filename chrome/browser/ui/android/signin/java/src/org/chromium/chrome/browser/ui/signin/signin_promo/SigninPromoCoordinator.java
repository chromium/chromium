// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.ui.signin.signin_promo;

import android.content.Context;

import androidx.annotation.StringRes;

import org.chromium.chrome.browser.preferences.ChromePreferenceKeys;
import org.chromium.chrome.browser.preferences.ChromeSharedPreferences;
import org.chromium.chrome.browser.profiles.Profile;
import org.chromium.chrome.browser.signin.services.IdentityServicesProvider;
import org.chromium.chrome.browser.signin.services.SigninPreferencesManager;
import org.chromium.chrome.browser.sync.SyncServiceFactory;
import org.chromium.chrome.browser.ui.signin.PersonalizedSigninPromoView;
import org.chromium.components.signin.identitymanager.ConsentLevel;
import org.chromium.components.sync.SyncService;
import org.chromium.components.sync.UserSelectableType;
import org.chromium.ui.modelutil.PropertyModelChangeProcessor;

import java.util.Set;

/** Coordinator for the signin promo card. */
public final class SigninPromoCoordinator {
    private static final int MAX_IMPRESSIONS_BOOKMARKS = 20;
    private final SigninPromoMediator mMediator;

    private PropertyModelChangeProcessor mPropertyModelChangeProcessor;

    /**
     * Creates and instance of the {@link SigninPromoCoordinator}.
     *
     * @param context The Android {@link Context}.
     * @param titleStringId The resource id for the title string.
     * @param descriptionStringId The resource id for the description string.
     * @param shouldSuppressSecondaryButton Whether the secondary button should be suppressed.
     * @param shouldHideDismissButton Whether the dismiss button fo the promo card should be hidden.
     */
    public SigninPromoCoordinator(
            Context context,
            @StringRes int titleStringId,
            @StringRes int descriptionStringId,
            boolean shouldSuppressSecondaryButton,
            boolean shouldHideDismissButton) {
        // TODO(crbug.com/327387704): Observe the AccountManagerFacade so that the promo gets
        // properly updated when the list of accounts changes.
        mMediator =
                new SigninPromoMediator(
                        context,
                        titleStringId,
                        descriptionStringId,
                        shouldSuppressSecondaryButton,
                        shouldHideDismissButton);
    }

    /** Sets the view that is controlled by this coordinator. */
    public void setView(PersonalizedSigninPromoView view) {
        if (mPropertyModelChangeProcessor != null) {
            mPropertyModelChangeProcessor.destroy();
            mPropertyModelChangeProcessor = null;
        }
        mPropertyModelChangeProcessor =
                PropertyModelChangeProcessor.create(
                        mMediator.getModel(), view, SigninPromoViewBinder::bind);
    }

    public void increasePromoShowCount() {
        // TODO(crbug.com/327387704): Implement this method
    }

    // TODO(crbug.com/327387704): When this MVC is integrated into more access points, move this
    // inside a switch statement.
    public static boolean canShowBookmarkSigninPromo(Profile profile) {
        if (IdentityServicesProvider.get()
                .getIdentityManager(profile)
                .hasPrimaryAccount(ConsentLevel.SIGNIN)) {
            return false;
        }

        SyncService syncService = SyncServiceFactory.getForProfile(profile);
        if (syncService
                .getSelectedTypes()
                .containsAll(
                        Set.of(UserSelectableType.BOOKMARKS, UserSelectableType.READING_LIST))) {
            return false;
        }

        boolean isTypeManagedByPolicy =
                syncService.isTypeManagedByPolicy(UserSelectableType.BOOKMARKS);
        boolean isMaxImpressionCountReached =
                ChromeSharedPreferences.getInstance()
                                .readInt(
                                        ChromePreferenceKeys.SYNC_PROMO_SHOW_COUNT.createKey(
                                                SigninPreferencesManager.SyncPromoAccessPointId
                                                        .BOOKMARKS))
                        >= MAX_IMPRESSIONS_BOOKMARKS;
        boolean isPromoDismissed =
                ChromeSharedPreferences.getInstance()
                        .readBoolean(ChromePreferenceKeys.SIGNIN_PROMO_BOOKMARKS_DECLINED, false);

        return !isTypeManagedByPolicy && !isMaxImpressionCountReached && !isPromoDismissed;
    }
}
