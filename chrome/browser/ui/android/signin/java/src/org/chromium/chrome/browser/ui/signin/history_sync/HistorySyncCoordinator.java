// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.ui.signin.history_sync;

import android.content.Context;
import android.content.res.Configuration;
import android.view.LayoutInflater;

import androidx.annotation.Nullable;

import org.chromium.base.metrics.RecordHistogram;
import org.chromium.chrome.browser.profiles.Profile;
import org.chromium.chrome.browser.signin.services.IdentityServicesProvider;
import org.chromium.chrome.browser.ui.signin.MinorModeHelper;
import org.chromium.chrome.browser.ui.signin.R;
import org.chromium.components.signin.identitymanager.ConsentLevel;
import org.chromium.components.signin.metrics.SigninAccessPoint;
import org.chromium.ui.modelutil.PropertyModelChangeProcessor;

public class HistorySyncCoordinator {
    /*Delegate for the History Sync MVC */
    public interface HistorySyncDelegate {
        void dismissHistorySync();

        boolean isLargeScreen();
    }

    private final Context mContext;
    private final HistorySyncDelegate mDelegate;
    private final Profile mProfile;
    private @Nullable HistorySyncView mView;
    private final HistorySyncMediator mMediator;
    private boolean mUseLandscapeLayout;
    private PropertyModelChangeProcessor mPropertyModelChangeProcessor;

    /**
     * Creates an instance of {@link HistorySyncCoordinator} and shows the sign-in bottom sheet.
     *
     * @param context The Android {@link Context}.
     * @param delegate The delegate for this coordinator.
     * @param profile The current profile.
     * @param accessPoint The entry point for the opt-in.
     * @param showEmailInFooter Whether the user's email should be shown in the UI footer. If the
     *     email is non-displayable, it won't be shown regardless of this value.
     * @param shouldSignOutOnDecline Whether the user should be signed out if they decline history
     *     sync.
     * @param view The view that will be controlled by the coordinator. If null, the coordinator
     *     will inflate its own view.
     */
    public HistorySyncCoordinator(
            Context context,
            HistorySyncDelegate delegate,
            Profile profile,
            @SigninAccessPoint int accessPoint,
            boolean showEmailInFooter,
            boolean shouldSignOutOnDecline,
            @Nullable HistorySyncView view) {
        mContext = context;
        mDelegate = delegate;
        mProfile = profile;
        mView = view;

        mUseLandscapeLayout =
                !delegate.isLargeScreen()
                        && context.getResources().getConfiguration().orientation
                                == Configuration.ORIENTATION_LANDSCAPE;
        mMediator =
                new HistorySyncMediator(
                        context,
                        delegate,
                        profile,
                        accessPoint,
                        showEmailInFooter,
                        shouldSignOutOnDecline,
                        mUseLandscapeLayout);

        setView(view, mUseLandscapeLayout);
        RecordHistogram.recordEnumeratedHistogram(
                "Signin.HistorySyncOptIn.Started", accessPoint, SigninAccessPoint.MAX);
        MinorModeHelper.resolveMinorMode(
                IdentityServicesProvider.get().getSigninManager(mProfile).getIdentityManager(),
                IdentityServicesProvider.get()
                        .getSigninManager(mProfile)
                        .getIdentityManager()
                        .getPrimaryAccountInfo(ConsentLevel.SIGNIN),
                mMediator::onMinorModeRestrictionStatusUpdated);
    }

    public void destroy() {
        setView(null, false);
        mMediator.destroy();
    }

    public HistorySyncView getView() {
        return mView;
    }

    /**
     * Sets the view that is controlled by the coordinator.
     *
     * @param view the HistorySyncView for the selected account
     * @param landscapeLayout whether a landscape layout is used.
     */
    public void setView(@Nullable HistorySyncView view, boolean landscapeLayout) {
        if (mPropertyModelChangeProcessor != null) {
            mPropertyModelChangeProcessor.destroy();
            mPropertyModelChangeProcessor = null;
        }
        if (view != null) {
            mUseLandscapeLayout = landscapeLayout;
            mMediator
                    .getModel()
                    .set(HistorySyncProperties.USE_LANDSCAPE_LAYOUT, mUseLandscapeLayout);
            mPropertyModelChangeProcessor =
                    PropertyModelChangeProcessor.create(
                            mMediator.getModel(), view, HistorySyncViewBinder::bind);
            mView = view;
        }
    }

    /**
     * Creates a view if needed and and sets the view that is controlled by the coordinator.
     *
     * @param context The Android Context used to inflate the view
     */
    public @Nullable HistorySyncView maybeRecreateView() {
        HistorySyncView view = null;
        boolean useLandscapeLayout =
                !mDelegate.isLargeScreen()
                        && mContext.getResources().getConfiguration().orientation
                                == Configuration.ORIENTATION_LANDSCAPE;

        if (getView() == null || mUseLandscapeLayout != useLandscapeLayout) {
            mUseLandscapeLayout = useLandscapeLayout;
            view = inflateView(mContext, mUseLandscapeLayout);
            setView(view, mUseLandscapeLayout);
        }

        return view;
    }

    private static HistorySyncView inflateView(Context context, boolean useLandscapeLayout) {
        LayoutInflater inflater = LayoutInflater.from(context);
        return (HistorySyncView)
                inflater.inflate(
                        useLandscapeLayout
                                ? R.layout.history_sync_landscape_view
                                : R.layout.history_sync_portrait_view,
                        null,
                        false);
    }
}
