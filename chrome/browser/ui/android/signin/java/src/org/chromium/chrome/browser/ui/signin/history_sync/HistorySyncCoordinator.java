// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.ui.signin.history_sync;

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
        void dismiss();

        boolean canUseLandscapeLayout();
    }

    private final HistorySyncMediator mMediator;
    private final HistorySyncView mView;
    private final PropertyModelChangeProcessor mPropertyModelChangeProcessor;
    private boolean mIsDestroyed;

    public HistorySyncCoordinator(
            LayoutInflater inflater,
            HistorySyncDelegate delegate,
            Profile profile,
            @SigninAccessPoint int accessPoint) {
        mMediator = new HistorySyncMediator(inflater.getContext(), delegate, profile, accessPoint);
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
                delegate.canUseLandscapeLayout()
                        && configuration.orientation == Configuration.ORIENTATION_LANDSCAPE;

        return (HistorySyncView)
                inflater.inflate(
                        useLandscapeLayout
                                ? R.layout.history_sync_landscape_view
                                : R.layout.history_sync_portrait_view,
                        null,
                        false);
    }
}
