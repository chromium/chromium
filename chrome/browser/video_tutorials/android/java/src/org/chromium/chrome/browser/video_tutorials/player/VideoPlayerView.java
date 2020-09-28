// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.video_tutorials.player;

import android.content.Context;
import android.view.LayoutInflater;
import android.view.View;
import android.widget.FrameLayout;

import org.chromium.chrome.browser.video_tutorials.R;
import org.chromium.components.thinwebview.ThinWebView;
import org.chromium.ui.modelutil.PropertyModel;

/**
 *  Represents the view component of the media player. Contains loading screen, language picker, and
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

    void showMediaControls(boolean show) {
        mControls.setVisibility(show ? View.VISIBLE : View.GONE);
    }

    void showLanguagePicker(boolean show) {
        mLanguagePickerView.setVisibility(show ? View.VISIBLE : View.GONE);
    }
}
