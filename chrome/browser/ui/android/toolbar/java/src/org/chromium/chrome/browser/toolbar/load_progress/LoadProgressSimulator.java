// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.toolbar.load_progress;

import android.os.Handler;
import android.os.Looper;
import android.os.Message;

import androidx.annotation.VisibleForTesting;

import org.chromium.base.MathUtils;
import org.chromium.ui.modelutil.PropertyModel;

/**
 * Simulator for  load progress changes when the page being rendered doesn't actually send load
 * progress signals, e.g. when swapping in a pre-rendered page. Uses a message loop to send update
 * messages to itself to update the simulated progress value on a regular interval.
 */
class LoadProgressSimulator {
    private static final int MSG_ID_UPDATE_PROGRESS = 1;
    private static final int PROGRESS_INCREMENT_DELAY_MS = 10;
    @VisibleForTesting
    static final float PROGRESS_INCREMENT = 0.1f;

    private final PropertyModel mModel;
    private final Handler mHandler;

    private float mProgress;

    public LoadProgressSimulator(PropertyModel model) {
        mModel = model;
        mHandler = new Handler(Looper.getMainLooper()) {
            @Override
            public void handleMessage(Message msg) {
                assert msg.what == MSG_ID_UPDATE_PROGRESS;
                mProgress = Math.min(1, mProgress += PROGRESS_INCREMENT);
                mModel.set(LoadProgressProperties.PROGRESS, mProgress);

                if (MathUtils.areFloatsEqual(mProgress, 1.0f)) {
                    mModel.set(LoadProgressProperties.COMPLETION_STATE,
                            LoadProgressProperties.CompletionState.FINISHED_DO_ANIMATE);
                    return;
                }
                sendEmptyMessageDelayed(MSG_ID_UPDATE_PROGRESS, PROGRESS_INCREMENT_DELAY_MS);
            }
        };
    }

    /**
     * Start simulating load progress from a baseline of 0.
     */
    public void start() {
        mProgress = 0.0f;
        mModel.set(LoadProgressProperties.COMPLETION_STATE,
                LoadProgressProperties.CompletionState.UNFINISHED);
        mModel.set(LoadProgressProperties.PROGRESS, mProgress);
        mHandler.sendEmptyMessage(MSG_ID_UPDATE_PROGRESS);
    }

    /**
     * Cancels simulating load progress.
     */
    public void cancel() {
        mModel.set(LoadProgressProperties.COMPLETION_STATE,
                LoadProgressProperties.CompletionState.FINISHED_DONT_ANIMATE);
        mHandler.removeMessages(MSG_ID_UPDATE_PROGRESS);
    }
}