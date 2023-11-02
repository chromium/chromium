// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.video_tutorials;

import android.content.Context;
import android.util.Pair;
import android.view.ViewGroup;
import android.view.ViewStub;

import org.chromium.base.Callback;
import org.chromium.base.annotations.NativeMethods;
import org.chromium.base.supplier.Supplier;
import org.chromium.chrome.browser.profiles.Profile;
import org.chromium.chrome.browser.video_tutorials.iph.TryNowTrackerImpl;
import org.chromium.chrome.browser.video_tutorials.iph.VideoIPHCoordinator;
import org.chromium.chrome.browser.video_tutorials.iph.VideoIPHCoordinatorImpl;
import org.chromium.chrome.browser.video_tutorials.iph.VideoTutorialTryNowTracker;
import org.chromium.chrome.browser.video_tutorials.list.TutorialListCoordinator;
import org.chromium.chrome.browser.video_tutorials.list.TutorialListCoordinatorImpl;
import org.chromium.chrome.browser.video_tutorials.player.VideoPlayerCoordinator;
import org.chromium.chrome.browser.video_tutorials.player.VideoPlayerCoordinatorImpl;
import org.chromium.components.embedder_support.view.ContentView;
import org.chromium.components.image_fetcher.ImageFetcher;
import org.chromium.content_public.browser.WebContents;
import org.chromium.ui.base.IntentRequestTracker;

/**
 * Basic factory that creates and returns an {@link VideoTutorialService} that is attached
 * natively to the given {@link Profile}.
 */
public class VideoTutorialServiceFactory {
    private static VideoTutorialService sVideoTutorialServiceForTesting;

    /**
     * Used to get access to the video tutorials backend.
     * @return An {@link VideoTutorialService} instance.
     */
    public static VideoTutorialService getForProfile(Profile profile) {
        if (sVideoTutorialServiceForTesting != null) return sVideoTutorialServiceForTesting;
        return VideoTutorialServiceFactoryJni.get().getForProfile(profile);
    }

    /** See {@link VideoIPHCoordinator} constructor.*/
    public static VideoIPHCoordinator createVideoIPHCoordinator(ViewStub viewStub,
            ImageFetcher imageFetcher, Callback<Tutorial> onClickListener,
            Callback<Tutorial> onDismissListener) {
        return new VideoIPHCoordinatorImpl(
                viewStub, imageFetcher, onClickListener, onDismissListener);
    }

    /** See {@link VideoPlayerCoordinator}.*/
    public static VideoPlayerCoordinator createVideoPlayerCoordinator(Context context,
            VideoTutorialService videoTutorialService,
            Supplier<Pair<WebContents, ContentView>> webContentsFactory,
            LanguageInfoProvider languageInfoProvider, Callback<Tutorial> tryNowCallback,
            Runnable closeCallback, IntentRequestTracker intentRequestTracker) {
        return new VideoPlayerCoordinatorImpl(context, videoTutorialService, webContentsFactory,
                languageInfoProvider, tryNowCallback, closeCallback, intentRequestTracker);
    }

    /** See {@link TutorialListCoordinator}.*/
    public static TutorialListCoordinator createTutorialListCoordinator(ViewGroup mainView,
            VideoTutorialService videoTutorialService, ImageFetcher imageFetcher,
            Callback<Tutorial> clickCallback) {
        return new TutorialListCoordinatorImpl(
                mainView, videoTutorialService, imageFetcher, clickCallback);
    }

    /** @return The tracker to track Try Now button clicks. */
    public static VideoTutorialTryNowTracker getTryNowTracker() {
        return LazyHolder.TRY_NOW_TRACKER;
    }

    public static void setVideoTutorialServiceForTesting(VideoTutorialService provider) {
        sVideoTutorialServiceForTesting = provider;
    }

    private static final class LazyHolder {
        private static final VideoTutorialTryNowTracker TRY_NOW_TRACKER = new TryNowTrackerImpl();
    }

    @NativeMethods
    interface Natives {
        VideoTutorialService getForProfile(Profile profile);
    }
}
