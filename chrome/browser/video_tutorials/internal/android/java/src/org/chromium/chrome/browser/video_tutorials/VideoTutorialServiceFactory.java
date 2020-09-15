// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.video_tutorials;

import org.chromium.base.annotations.NativeMethods;
import org.chromium.chrome.browser.profiles.Profile;

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

    /** For testing only. */
    public static void setVideoTutorialServiceForTesting(VideoTutorialService provider) {
        sVideoTutorialServiceForTesting = provider;
    }

    @NativeMethods
    interface Natives {
        VideoTutorialService getForProfile(Profile profile);
    }
}