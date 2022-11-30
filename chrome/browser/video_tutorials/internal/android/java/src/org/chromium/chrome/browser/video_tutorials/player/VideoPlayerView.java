// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.video_tutorials.player;

import android.content.Context;
import android.view.LayoutInflater;
import android.view.View;
import android.widget.FrameLayout;
import android.widget.LinearLayout;
import android.widget.LinearLayout.LayoutParams;

import org.chromium.chrome.browser.video_tutorials.PlaybackStateObserver.WatchStateInfo;
import org.chromium.chrome.browser.video_tutorials.PlaybackStateObserver.WatchStateInfo.State;
import org.chromium.chrome.browser.video_tutorials.R;
import org.chromium.components.thinwebview.ThinWebView;
import org.chromium.ui.modelutil.PropertyModel;

/**
 * Represents the view component of the media player. Contains loading screen, language picker, and
 * media controls.
 */
class VideoPlayerView {
    private final PropertyModel mModel;
    private final FrameLayout mFrameLayout;
    private final ThinWebView mThinWebView;
    private final View mLoadingView;
    private final View mControls;
    private final View mLanguagePickerView;

    /** Constructor. */
    public VideoPlayerView(Context context, PropertyModel model, ThinWebView thinWebView) {
        mModel = model;
        mThinWebView = thinWebView;
        mFrameLayout = new FrameLayout(context);
        mFrameLayout.addView(mThinWebView.getView());

        mControls = LayoutInflater.from(context).inflate(R.layout.video_player_controls, null);
        mLoadingView = LayoutInflater.from(context).inflate(R.layout.video_player_loading, null);
        mLanguagePickerView = LayoutInflater.from(context).inflate(R.layout.language_picker, null);

        mFrameLayout.addView(mControls);
        mFrameLayout.addView(mLoadingView);
        mFrameLayout.addView(mLanguagePickerView);
    }

    View getView() {
        return mFrameLayout;
    }

    void destroy() {
        mThinWebView.destroy();
    }

    void showLoadingAnimation(boolean show) {
        mLoadingView.setVisibility(show ? View.VISIBLE : View.GONE);
    }

    void showLanguagePicker(boolean show) {
        mLanguagePickerView.setVisibility(show ? View.VISIBLE : View.GONE);
    }

    void setTryNowButtonPosition(WatchStateInfo.State state) {
        View topHalf = mControls.findViewById(R.id.top_half);
        View bottomHalf = mControls.findViewById(R.id.bottom_half);
        LinearLayout.LayoutParams topLayoutParams = (LayoutParams) topHalf.getLayoutParams();
        LinearLayout.LayoutParams bottomLayoutParams = (LayoutParams) bottomHalf.getLayoutParams();
        if (state == State.PAUSED) {
            topLayoutParams.weight = 0.5f;
            bottomLayoutParams.weight = 0.5f;
        } else if (state == State.ENDED) {
            topLayoutParams.weight = 0.62f;
            bottomLayoutParams.weight = 0.38f;
        } else {
            assert false : "Unexpected state " + state;
        }
    }
}
