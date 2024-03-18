// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.ui.signin.history_sync;

import android.content.Context;
import android.content.res.Configuration;
import android.view.LayoutInflater;
import android.view.View;

import org.chromium.base.metrics.RecordHistogram;
import org.chromium.chrome.browser.profiles.Profile;
import org.chromium.chrome.browser.ui.signin.R;
import org.chromium.components.signin.metrics.SigninAccessPoint;
import org.chromium.ui.modelutil.PropertyModelChangeProcessor;

public class HistorySyncCoordinator {
    /*Delegate for the History Sync MVC */
    public interface HistorySyncDelegate {
        void dismissHistorySync();

        boolean isLargeScreen();
    }

    private final HistorySyncMediator mMediator;
    private final HistorySyncView mView;
    private final PropertyModelChangeProcessor mPropertyModelChangeProcessor;
    private boolean mIsDestroyed;

    /**
     * Creates an instance of {@link HistorySyncCoordinator} and shows the sign-in bottom sheet.
     *
     * @param context The Android {@link Context}.
     * @param delegate The delegate for this coordinator.
     * @param profile The current profile.
     * @param accessPoint The entry point for the opt-in.
     * @param showEmailInFooter Whether the user's email should be shown in the UI footer. If the
     *     email is non-displayable, it won't be shown regardless of this value.
     */
    public HistorySyncCoordinator(
            Context context,
            HistorySyncDelegate delegate,
            Profile profile,
            @SigninAccessPoint int accessPoint,
            boolean showEmailInFooter) {
        mMediator =
                new HistorySyncMediator(context, delegate, profile, accessPoint, showEmailInFooter);
        LayoutInflater inflater = LayoutInflater.from(context);
        mView = inflateView(inflater, delegate);
        mPropertyModelChangeProcessor =
                PropertyModelChangeProcessor.create(
                        mMediator.getModel(), mView, HistorySyncViewBinder::bind);
        RecordHistogram.recordEnumeratedHistogram(
                "Signin.HistorySyncOptIn.Started", accessPoint, SigninAccessPoint.MAX);
    }

    public void destroy() {
        assert !mIsDestroyed;
        mPropertyModelChangeProcessor.destroy();
        mMediator.destroy();
        mIsDestroyed = true;
    }

    public View getView() {
        return mView;
    }

    private HistorySyncView inflateView(LayoutInflater inflater, HistorySyncDelegate delegate) {
        Configuration configuration = inflater.getContext().getResources().getConfiguration();
        boolean useLandscapeLayout =
                !delegate.isLargeScreen()
                        && configuration.orientation == Configuration.ORIENTATION_LANDSCAPE;

        HistorySyncView view =
                (HistorySyncView)
                        inflater.inflate(
                                useLandscapeLayout
                                        ? R.layout.history_sync_landscape_view
                                        : R.layout.history_sync_portrait_view,
                                null,
                                false);

        // For phones in portrait mode, the UI shows two vertically stacked full-width buttons. In
        // other cases, we use a horizontally stacked button bar.
        view.createButtons(/* isButtonBar= */ delegate.isLargeScreen() || useLandscapeLayout);
        return view;
    }
}
