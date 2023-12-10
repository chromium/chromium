// Copyright 2013 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.components.embedder_support.media;

import android.annotation.SuppressLint;
import android.app.Activity;
import android.view.Gravity;
import android.view.View;
import android.view.ViewGroup;
import android.view.WindowManager;
import android.widget.FrameLayout;

import org.chromium.content_public.browser.ContentVideoViewEmbedder;

/** Uses an existing Activity to handle displaying video in full screen. */
public class ActivityContentVideoViewEmbedder implements ContentVideoViewEmbedder {
    private final Activity mActivity;
    private View mView;

    public ActivityContentVideoViewEmbedder(Activity activity) {
        this.mActivity = activity;
    }

    @Override
    public void enterFullscreenVideo(View view, boolean isVideoLoaded) {
        FrameLayout decor = (FrameLayout) mActivity.getWindow().getDecorView();
        decor.addView(
                view,
                0,
                new FrameLayout.LayoutParams(
                        ViewGroup.LayoutParams.MATCH_PARENT,
                        ViewGroup.LayoutParams.MATCH_PARENT,
                        Gravity.CENTER));
        setSystemUiVisibility(true);
        mView = view;
    }

    @Override
    public void fullscreenVideoLoaded() {}

    @Override
    public void exitFullscreenVideo() {
        FrameLayout decor = (FrameLayout) mActivity.getWindow().getDecorView();
        decor.removeView(mView);
        setSystemUiVisibility(false);
        mView = null;
    }

    @Override
    @SuppressLint("InlinedApi")
    public void setSystemUiVisibility(boolean enterFullscreen) {
        View decor = mActivity.getWindow().getDecorView();
        if (enterFullscreen) {
            mActivity
                    .getWindow()
                    .setFlags(
                            WindowManager.LayoutParams.FLAG_FULLSCREEN,
                            WindowManager.LayoutParams.FLAG_FULLSCREEN);
        } else {
            mActivity.getWindow().clearFlags(WindowManager.LayoutParams.FLAG_FULLSCREEN);
        }

        int systemUiVisibility = decor.getSystemUiVisibility();
        int flags =
                View.SYSTEM_UI_FLAG_LAYOUT_HIDE_NAVIGATION
                        | View.SYSTEM_UI_FLAG_LAYOUT_FULLSCREEN
                        | View.SYSTEM_UI_FLAG_HIDE_NAVIGATION
                        | View.SYSTEM_UI_FLAG_FULLSCREEN
                        | View.SYSTEM_UI_FLAG_IMMERSIVE_STICKY;
        if (enterFullscreen) {
            systemUiVisibility |= flags;
        } else {
            systemUiVisibility &= ~flags;
        }
        decor.setSystemUiVisibility(systemUiVisibility);
    }
}
