// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.ui.signin.history_sync;

import android.view.LayoutInflater;
import android.view.View;

import org.chromium.chrome.browser.profiles.Profile;
import org.chromium.chrome.browser.ui.signin.R;
import org.chromium.ui.modelutil.PropertyModelChangeProcessor;

public class HistorySyncCoordinator {
    /*Delegate for the History Sync MVC */
    public interface HistorySyncDelegate {
        void dismiss();
    }

    private final HistorySyncMediator mMediator;
    private final HistorySyncView mView;
    private final PropertyModelChangeProcessor mPropertyModelChangeProcessor;
    private boolean mIsDestroyed;

    public HistorySyncCoordinator(
            LayoutInflater inflater,
            HistorySyncDelegate delegate,
            Profile profile) {
        mView = (HistorySyncView) inflater.inflate(R.layout.history_sync_view, null, false);
        mMediator = new HistorySyncMediator(inflater.getContext(), delegate, profile);
        mPropertyModelChangeProcessor =
                PropertyModelChangeProcessor.create(
                        mMediator.getModel(), mView, HistorySyncViewBinder::bind);
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
}
