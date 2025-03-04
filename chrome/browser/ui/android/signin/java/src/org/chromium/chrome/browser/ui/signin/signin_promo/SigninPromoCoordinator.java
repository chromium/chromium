// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.ui.signin.signin_promo;

import android.content.Context;
import android.view.LayoutInflater;
import android.view.View;
import android.view.ViewGroup;

import org.chromium.base.ResettersForTesting;
import org.chromium.chrome.browser.profiles.Profile;
import org.chromium.chrome.browser.signin.services.IdentityServicesProvider;
import org.chromium.chrome.browser.signin.services.ProfileDataCache;
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
public class SigninPromoCoordinator {
    private static boolean sPromoDisabledForTesting;
    private final Context mContext;
    private final SigninPromoDelegate mDelegate;
    private final SigninPromoMediator mMediator;
    private ImpressionTracker mImpressionTracker;
    private PropertyModelChangeProcessor mPropertyModelChangeProcessor;

    /** Disables promo in tests. */
    public static void disablePromoForTesting() {
        sPromoDisabledForTesting = true;
        ResettersForTesting.register(() -> sPromoDisabledForTesting = false);
    }

    /**
     * Creates an instance of the {@link SigninPromoCoordinator}.
     *
     * @param context The Android {@link Context}.
     * @param profile A {@link Profile} object to access identity services. This must be the
     *     original profile, not the incognito one.
     * @param delegate A {@link SigninPromoDelegate} to customize the view.
     */
    public SigninPromoCoordinator(Context context, Profile profile, SigninPromoDelegate delegate) {
        mContext = context;
        mDelegate = delegate;
        ProfileDataCache profileDataCache =
                ProfileDataCache.createWithDefaultImageSizeAndNoBadge(mContext);
        IdentityManager identityManager =
                IdentityServicesProvider.get().getIdentityManager(profile);
        SyncService syncService = SyncServiceFactory.getForProfile(profile);
        mMediator =
                new SigninPromoMediator(
                        identityManager,
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
        return !sPromoDisabledForTesting && mMediator.canShowPromo();
    }

    /** Builds a promo view object for the corresponding access point. */
    public View buildPromoView(ViewGroup parent) {
        return LayoutInflater.from(mContext)
                .inflate(getLayoutResId(mDelegate.getAccessPoint()), parent, false);
    }

    /** Sets the view that is controlled by this coordinator. */
    public void setView(View view) {
        PersonalizedSigninPromoView promoView = view.findViewById(R.id.signin_promo_view_container);
        if (promoView == null) {
            throw new IllegalArgumentException("Promo view doesn't exist in container");
        }
        if (mPropertyModelChangeProcessor != null) {
            mPropertyModelChangeProcessor.destroy();
            mPropertyModelChangeProcessor = null;
            mImpressionTracker.setListener(null);
            mImpressionTracker = null;
        }
        mPropertyModelChangeProcessor =
                PropertyModelChangeProcessor.create(
                        mMediator.getModel(), promoView, SigninPromoViewBinder::bind);
        mImpressionTracker = new ImpressionTracker(view);
        mImpressionTracker.setListener(mMediator::recordImpression);
    }

    static int getLayoutResId(@SigninAccessPoint int accessPoint) {
        return switch (accessPoint) {
            case SigninAccessPoint.BOOKMARK_MANAGER -> R.layout.sync_promo_view_bookmarks;
            case SigninAccessPoint.HISTORY_PAGE -> R.layout.sync_promo_view_history_page;
            case SigninAccessPoint.NTP_FEED_TOP_PROMO -> R.layout
                    .sync_promo_view_content_suggestions;
            case SigninAccessPoint.RECENT_TABS -> R.layout.sync_promo_view_recent_tabs;
            default -> throw new IllegalArgumentException("Invalid sign-in promo access point");
        };
    }
}
