// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.ui.signin.signin_promo;

import android.content.Context;

import org.chromium.chrome.browser.profiles.Profile;
import org.chromium.chrome.browser.signin.services.IdentityServicesProvider;
import org.chromium.chrome.browser.signin.services.ProfileDataCache;
import org.chromium.chrome.browser.signin.services.SigninManager;
import org.chromium.chrome.browser.sync.SyncServiceFactory;
import org.chromium.chrome.browser.ui.signin.PersonalizedSigninPromoView;
import org.chromium.chrome.browser.ui.signin.R;
import org.chromium.components.browser_ui.widget.impression.ImpressionTracker;
import org.chromium.components.signin.AccountManagerFacadeProvider;
import org.chromium.components.signin.identitymanager.IdentityManager;
import org.chromium.components.signin.metrics.SigninAccessPoint;
import org.chromium.components.sync.SyncService;
import org.chromium.ui.modelutil.PropertyModelChangeProcessor;

/** Coordinator for the signin promo card. */
public final class SigninPromoCoordinator {
    private final SigninPromoMediator mMediator;
    private ImpressionTracker mImpressionTracker;
    private PropertyModelChangeProcessor mPropertyModelChangeProcessor;

    /**
     * Creates an instance of the {@link SigninPromoCoordinator}.
     *
     * @param context The Android {@link Context}.
     * @param profile A {@link Profile} object to access identity services.
     * @param delegate A {@link SigninPromoDelegate} to customize the view.
     */
    public SigninPromoCoordinator(Context context, Profile profile, SigninPromoDelegate delegate) {
        // TODO(crbug.com/327387704): Observe the AccountManagerFacade so that the promo gets
        // properly updated when the list of accounts changes.
        ProfileDataCache profileDataCache =
                ProfileDataCache.createWithDefaultImageSizeAndNoBadge(context);
        IdentityManager identityManager =
                IdentityServicesProvider.get().getIdentityManager(profile);
        SigninManager signinManager = IdentityServicesProvider.get().getSigninManager(profile);
        SyncService syncService = SyncServiceFactory.getForProfile(profile);
        mMediator =
                new SigninPromoMediator(
                        identityManager,
                        signinManager,
                        syncService,
                        AccountManagerFacadeProvider.getInstance(),
                        profileDataCache,
                        delegate);
    }

    public void destroy() {
        mMediator.destroy();
        if (mPropertyModelChangeProcessor != null) {
            mPropertyModelChangeProcessor.destroy();
            mPropertyModelChangeProcessor = null;
            mImpressionTracker.setListener(null);
            mImpressionTracker = null;
        }
    }

    /** Determines whether the signin promo can be shown. */
    public boolean canShowPromo() {
        return mMediator.canShowPromo();
    }

    /** Sets the view that is controlled by this coordinator. */
    public void setView(PersonalizedSigninPromoView view) {
        if (mPropertyModelChangeProcessor != null) {
            mPropertyModelChangeProcessor.destroy();
            mPropertyModelChangeProcessor = null;
            mImpressionTracker.setListener(null);
            mImpressionTracker = null;
        }
        mPropertyModelChangeProcessor =
                PropertyModelChangeProcessor.create(
                        mMediator.getModel(), view, SigninPromoViewBinder::bind);
        mImpressionTracker = new ImpressionTracker(view);
        mImpressionTracker.setListener(mMediator::recordImpression);
    }

    public void increasePromoShowCount() {
        // TODO(crbug.com/327387704): Implement this method
    }

    static int getLayoutResId(@SigninAccessPoint int accessPoint) {
        return switch (accessPoint) {
            case SigninAccessPoint.BOOKMARK_MANAGER -> R.layout.sync_promo_view_bookmarks;
            case SigninAccessPoint.NTP_FEED_TOP_PROMO -> R.layout
                    .sync_promo_view_content_suggestions;
            case SigninAccessPoint.RECENT_TABS -> R.layout.sync_promo_view_recent_tabs;
            default -> throw new IllegalArgumentException("Invalid sign-in promo access point");
        };
    }
}
