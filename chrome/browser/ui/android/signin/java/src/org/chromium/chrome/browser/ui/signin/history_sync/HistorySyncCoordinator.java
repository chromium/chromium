// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.ui.signin.history_sync;

import static org.chromium.build.NullUtil.assumeNonNull;

import android.app.Activity;
import android.view.LayoutInflater;
import android.widget.ImageView;

import org.chromium.base.metrics.RecordHistogram;
import org.chromium.build.annotations.NullMarked;
import org.chromium.build.annotations.Nullable;
import org.chromium.chrome.browser.flags.ChromeFeatureList;
import org.chromium.chrome.browser.profiles.Profile;
import org.chromium.chrome.browser.signin.services.IdentityServicesProvider;
import org.chromium.chrome.browser.signin.services.SigninManager;
import org.chromium.chrome.browser.ui.signin.MinorModeHelper;
import org.chromium.chrome.browser.ui.signin.R;
import org.chromium.chrome.browser.ui.signin.SigninUtils;
import org.chromium.components.signin.identitymanager.ConsentLevel;
import org.chromium.components.signin.identitymanager.IdentityManager;
import org.chromium.components.signin.metrics.SigninAccessPoint;
import org.chromium.ui.modelutil.PropertyModelChangeProcessor;

@NullMarked
public class HistorySyncCoordinator {

    /*Delegate for the History Sync MVC */
    public interface HistorySyncDelegate {
        /**
         * Notifies the delegate that the history sync UI has completed its flow and is dismissed.
         * The delegate is responsible for destroying the HistorySyncCoordinator after this call.
         *
         * @param didSignOut Whether the user was signed out as a result of dismissing history sync.
         *     This can happen if the user clicks the decline button and `shouldSignOutOnDecline` is
         *     true, or if a sign-out event is triggered externally while the UI is shown.
         * @param isHistorySyncAccepted Whether the user accepted history sync.
         */
        void dismissHistorySync(boolean didSignOut, boolean isHistorySyncAccepted);

        // TODO(crbug.com/453950636): inline delegate implementation directly in HistorySyncMediator
        void recordHistorySyncOptIn(
                @SigninAccessPoint int accessPoint, boolean isHistorySyncAccepted);
    }

    private final Activity mActivity;
    private final Profile mProfile;
    private @Nullable HistorySyncView mView;
    private final HistorySyncMediator mMediator;
    private boolean mUseLandscapeLayout;
    private @Nullable PropertyModelChangeProcessor mPropertyModelChangeProcessor;

    /**
     * Creates an instance of {@link HistorySyncCoordinator} and shows the sign-in bottom sheet.
     *
     * @param activity The Android {@link Activity} holding the fragment.
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
            Activity activity,
            HistorySyncDelegate delegate,
            Profile profile,
            HistorySyncConfig config,
            @SigninAccessPoint int accessPoint,
            boolean showEmailInFooter,
            boolean shouldSignOutOnDecline,
            @Nullable HistorySyncView view) {
        mActivity = activity;
        mProfile = profile;
        mView = view;

        mUseLandscapeLayout = SigninUtils.shouldShowDualPanesHorizontalLayout(activity);
        mMediator =
                new HistorySyncMediator(
                        activity,
                        delegate,
                        profile,
                        config,
                        accessPoint,
                        showEmailInFooter,
                        shouldSignOutOnDecline,
                        mUseLandscapeLayout);

        setView(view, mUseLandscapeLayout);
        RecordHistogram.recordEnumeratedHistogram(
                "Signin.HistorySyncOptIn.Started", accessPoint, SigninAccessPoint.MAX_VALUE);
        SigninManager signinManager = IdentityServicesProvider.get().getSigninManager(mProfile);
        assumeNonNull(signinManager);
        IdentityManager identityManager = signinManager.getIdentityManager();
        MinorModeHelper.resolveMinorMode(
                identityManager,
                assumeNonNull(identityManager.getPrimaryAccountInfo(ConsentLevel.SIGNIN)),
                mMediator::onMinorModeRestrictionStatusUpdated);
    }

    public void destroy() {
        setView(null, false);
        mMediator.destroy();
    }

    public @Nullable HistorySyncView getView() {
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
            boolean useAlternateIllustration =
                    ChromeFeatureList.isEnabled(
                            ChromeFeatureList.USE_ALTERNATE_HISTORY_SYNC_ILLUSTRATION);
            ImageView headerImage = view.findViewById(R.id.history_sync_illustration);
            headerImage.setImageResource(
                    useAlternateIllustration
                            ? R.drawable.history_sync_illustration_alternative
                            : R.drawable.history_sync_illustration);

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

    /** Creates a view if needed and and sets the view that is controlled by the coordinator. */
    public @Nullable HistorySyncView maybeRecreateView() {
        HistorySyncView view = null;
        boolean useLandscapeLayout = SigninUtils.shouldShowDualPanesHorizontalLayout(mActivity);

        if (getView() == null || mUseLandscapeLayout != useLandscapeLayout) {
            mUseLandscapeLayout = useLandscapeLayout;
            view = inflateView(mActivity, mUseLandscapeLayout);
            setView(view, mUseLandscapeLayout);
        }

        return view;
    }

    private static HistorySyncView inflateView(Activity activity, boolean useLandscapeLayout) {
        LayoutInflater inflater = LayoutInflater.from(activity);
        return (HistorySyncView)
                inflater.inflate(
                        useLandscapeLayout
                                ? R.layout.history_sync_landscape_view
                                : R.layout.history_sync_portrait_view,
                        null,
                        false);
    }
}
