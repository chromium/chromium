// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.video_tutorials.player;

import android.content.Context;
import android.util.Pair;
import android.view.View;

import org.chromium.base.supplier.Supplier;
import org.chromium.chrome.browser.video_tutorials.PlaybackStateObserver;
import org.chromium.chrome.browser.video_tutorials.R;
import org.chromium.chrome.browser.video_tutorials.Tutorial;
import org.chromium.chrome.browser.video_tutorials.VideoTutorialService;
import org.chromium.chrome.browser.video_tutorials.languages.LanguagePickerCoordinator;
import org.chromium.components.embedder_support.delegate.WebContentsDelegateAndroid;
import org.chromium.components.embedder_support.view.ContentView;
import org.chromium.components.thinwebview.ThinWebView;
import org.chromium.components.thinwebview.ThinWebViewConstraints;
import org.chromium.components.thinwebview.ThinWebViewFactory;
import org.chromium.content_public.browser.WebContents;
import org.chromium.ui.modelutil.PropertyModel;
import org.chromium.ui.modelutil.PropertyModelChangeProcessor;

/**
 * The top level coordinator for the video player.
 */
public class VideoPlayerCoordinatorImpl implements VideoPlayerCoordinator {
    private final Context mContext;
    private final PropertyModel mModel;
    private final VideoPlayerView mView;
    private final VideoPlayerMediator mMediator;
    private final VideoTutorialService mVideoTutorialService;
    private final LanguagePickerCoordinator mLanguagePicker;
    private WebContents mWebContents;
    private WebContentsDelegateAndroid mWebContentsDelegate;
    private PlaybackStateObserver mMediaSessionObserver;

    /**
     * Constructor.
     * @param context The activity context.
     * @param videoTutorialService The backend for serving video tutorials.
     * @param webContentsFactory A supplier to supply WebContents and ContentView.
     * @param closeCallback Callback to be invoked when this UI is closed.
     */
    public VideoPlayerCoordinatorImpl(Context context, VideoTutorialService videoTutorialService,
            Supplier<Pair<WebContents, ContentView>> webContentsFactory, Runnable closeCallback) {
        mContext = context;
        mVideoTutorialService = videoTutorialService;
        mModel = new PropertyModel(VideoPlayerProperties.ALL_KEYS);

        ThinWebView thinWebView = createThinWebView(webContentsFactory);
        mView = new VideoPlayerView(context, mModel, thinWebView);
        mLanguagePicker = new LanguagePickerCoordinator(
                mView.getView().findViewById(R.id.language_picker), mVideoTutorialService);
        mMediator = new VideoPlayerMediator(mContext, mModel, videoTutorialService, mLanguagePicker,
                mWebContents, closeCallback);
        PropertyModelChangeProcessor.create(mModel, mView, new VideoPlayerViewBinder());
    }

    @Override
    public void playVideoTutorial(Tutorial tutorial) {
        mMediator.playVideoTutorial(tutorial);
    }

    @Override
    public View getView() {
        return mView.getView();
    }

    @Override
    public boolean onBackPressed() {
        if (mMediator.handleBackPressed()) return true;
        return false;
    }

    @Override
    public void destroy() {
        mMediaSessionObserver.stopObserving();
        mView.destroy();
        mWebContents.destroy();
    }

    private ThinWebView createThinWebView(
            Supplier<Pair<WebContents, ContentView>> webContentsFactory) {
        Pair<WebContents, ContentView> pair = webContentsFactory.get();
        mWebContents = pair.first;
        ContentView webContentView = pair.second;
        mWebContentsDelegate = new WebContentsDelegateAndroid();
        mMediaSessionObserver = new PlaybackStateObserver(mWebContents, mMediator);

        ThinWebView thinWebView = ThinWebViewFactory.create(mContext, new ThinWebViewConstraints());
        thinWebView.attachWebContents(mWebContents, webContentView, mWebContentsDelegate);
        return thinWebView;
    }
}
