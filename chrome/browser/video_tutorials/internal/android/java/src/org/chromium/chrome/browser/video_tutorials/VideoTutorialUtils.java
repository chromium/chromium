// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.video_tutorials;

import android.content.ActivityNotFoundException;
import android.content.Context;
import android.content.Intent;

import org.chromium.base.Callback;
import org.chromium.base.Log;

import java.util.List;
import java.util.Locale;

/**
 * Handles various feature utility functions associated with video tutorials UI.
 */
public class VideoTutorialUtils {
    private static final String TAG = "VideoTutorialShare";

    /**
     * Creates and launches an Intent that allows sharing a video tutorial.
     */
    public static void launchShareIntent(Context context, Tutorial tutorial) {
        Intent intent = new Intent();
        intent.setType("video/*");
        intent.addFlags(Intent.FLAG_ACTIVITY_NEW_TASK);
        intent.setAction(Intent.ACTION_SEND);
        intent.putExtra(Intent.EXTRA_TEXT, tutorial.shareUrl);
        startShareIntent(context, intent);
    }

    private static void startShareIntent(Context context, Intent intent) {
        try {
            context.startActivity(Intent.createChooser(
                    intent, context.getString(R.string.share_link_chooser_title)));
        } catch (ActivityNotFoundException e) {
            Log.e(TAG, "Cannot find activity for sharing");
        } catch (Exception e) {
            Log.e(TAG, "Cannot start activity for sharing, exception: " + e);
        }
    }

    /**
     * Converts a duration string in ms to a human-readable form.
     * @param videoLengthSeconds The video length in seconds.
     * @return The video length in human-readable form.
     */
    public static String getVideoLengthString(int videoLengthSeconds) {
        int hours = videoLengthSeconds / 3600;
        int minutes = (videoLengthSeconds / 60) % 60;
        int seconds = videoLengthSeconds % 60;

        if (hours > 0) {
            return String.format(Locale.US, "%d:%02d:%02d", hours, minutes, seconds);
        } else {
            return String.format(Locale.US, "%d:%02d", minutes, seconds);
        }
    }

    /**
     * Finds the next video tutorial to be presented to the user after the user has completed one.
     */
    public static void getNextTutorial(VideoTutorialService videoTutorialService, Tutorial tutorial,
            Callback<Tutorial> callback) {
        videoTutorialService.getTutorials(tutorials -> {
            Tutorial nextTutorial = VideoTutorialUtils.getNextTutorial(tutorials, tutorial);
            callback.onResult(nextTutorial);
        });
    }

    /** @return Whether or not to show the Try Now button on the video player. */
    public static boolean shouldShowTryNow(@FeatureType int featureType) {
        switch (featureType) {
            case FeatureType.SEARCH:
            case FeatureType.VOICE_SEARCH:
                return true;
            case FeatureType.CHROME_INTRO:
            case FeatureType.DOWNLOAD:
            default:
                return false;
        }
    }

    private static Tutorial getNextTutorial(List<Tutorial> tutorials, Tutorial currentTutorial) {
        int currentIndex = 0;
        for (int i = 0; i < tutorials.size(); i++) {
            if (tutorials.get(i).featureType == currentTutorial.featureType) {
                currentIndex = i;
                break;
            }
        }

        return currentIndex < tutorials.size() - 1 ? tutorials.get(currentIndex + 1) : null;
    }
}
