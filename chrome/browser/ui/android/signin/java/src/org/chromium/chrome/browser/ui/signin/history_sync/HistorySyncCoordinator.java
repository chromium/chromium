// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.ui.signin.history_sync;

import android.content.Context;
import android.content.res.Configuration;
import android.view.LayoutInflater;
import android.view.View;

import androidx.annotation.Nullable;

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
    private final HistorySyncDelegate mDelegate;
    private final boolean mUseLandscapeLayout;
    private HistorySyncView mView;
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
            @Nullable View view) {
        mMediator =
                new HistorySyncMediator(
                        context,
                        delegate,
                        profile,
                        accessPoint,
                        showEmailInFooter,
                        shouldSignOutOnDecline);
        mDelegate = delegate;
        LayoutInflater inflater = LayoutInflater.from(context);
        mUseLandscapeLayout =
                !delegate.isLargeScreen()
                        && context.getResources().getConfiguration().orientation
                                == Configuration.ORIENTATION_LANDSCAPE;
        if (view == null) {
            mView = inflateView(inflater);
        } else {
            mView = (HistorySyncView) view;
            mView.createButtons(mDelegate.isLargeScreen() || mUseLandscapeLayout);
        }
        mPropertyModelChangeProcessor =
                PropertyModelChangeProcessor.create(
                        mMediator.getModel(), mView, HistorySyncViewBinder::bind);
        RecordHistogram.recordEnumeratedHistogram(
                "Signin.HistorySyncOptIn.Started", accessPoint, SigninAccessPoint.MAX);
    }

    public void destroy() {
        if (mPropertyModelChangeProcessor != null) {
            mPropertyModelChangeProcessor.destroy();
            mPropertyModelChangeProcessor = null;
        }
        mMediator.destroy();
    }

    public View getView() {
        return mView;
    }

    private HistorySyncView inflateView(LayoutInflater inflater) {
        HistorySyncView view =
                (HistorySyncView)
                        inflater.inflate(
                                mUseLandscapeLayout
                                        ? R.layout.history_sync_landscape_view
                                        : R.layout.history_sync_portrait_view,
                                null,
                                false);

        // For phones in portrait mode, the UI shows two vertically stacked full-width buttons. In
        // other cases, we use a horizontally stacked button bar.
        view.createButtons(/* isButtonBar= */ mDelegate.isLargeScreen() || mUseLandscapeLayout);
        return view;
    }
}
