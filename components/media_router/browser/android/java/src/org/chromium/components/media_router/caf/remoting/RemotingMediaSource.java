// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.components.media_router.caf.remoting;

import android.content.Context;
import android.content.pm.ApplicationInfo;
import android.content.pm.PackageManager;
import android.net.Uri;
import android.os.Bundle;
import android.util.Base64;

import androidx.annotation.Nullable;
import androidx.mediarouter.media.MediaRouteSelector;

import com.google.android.gms.cast.CastMediaControlIntent;

import org.chromium.base.ContextUtils;
import org.chromium.base.Log;
import org.chromium.components.media_router.MediaSource;

import java.io.UnsupportedEncodingException;

/** Abstracts parsing the Cast application id and other parameters from the source id. */
public class RemotingMediaSource implements MediaSource {
    private static final String TAG = "MediaRemoting";

    // Needs to be in sync with
    // third_party/blink/public/platform/modules/remoteplayback/remote_playback_source.h.
    private static final String SOURCE_PREFIX = "remote-playback:";
    private static final String ENCODED_SOURCE_KEY = "source";

    // Needs to be in sync with AndroidManifest meta-data key (used both by Clank and WebLayer
    // clients).
    private static final String REMOTE_PLAYBACK_APP_ID_KEY =
            "org.chromium.content.browser.REMOTE_PLAYBACK_APP_ID";

    /** The Cast application id. */
    private static String sApplicationId;

    /** The original source URL that the {@link MediaSource} object was created from. */
    private final String mSourceId;

    /** The URL to fling to the Cast device. */
    private final String mMediaUrl;

    /**
     * Initializes the media source from the source id.
     * @param sourceId a URL containing encoded info about the media element's source.
     * @return an initialized media source if the id is valid, null otherwise.
     */
    @Nullable
    public static RemotingMediaSource from(String sourceId) {
        assert sourceId != null;
        // The sourceId for RemotingMediaSource is not a hierarchical URI, which can't be parsed to
        // get query parameters. By removing the scheme from the URI, we can get an Relative URI
        // reference (in the format of <relative or absolute path>?<query>), which is always
        // hierarchical, and use it for query parameter parsing.
        if (!sourceId.startsWith(SOURCE_PREFIX)) return null;
        Uri sourceUri = Uri.parse(sourceId.substring(SOURCE_PREFIX.length()));
        if (!sourceUri.getPath().equals("media-element")) return null;

        String mediaUrl;
        try {
            String encodedSource = sourceUri.getQueryParameter(ENCODED_SOURCE_KEY);
            mediaUrl = new String(Base64.decode(encodedSource, Base64.URL_SAFE), "UTF-8");
        } catch (UnsupportedOperationException
                | NullPointerException
                | IllegalArgumentException
                | UnsupportedEncodingException e) {
            Log.e(TAG, "Couldn't parse the source id.", e);
            return null;
        }

        return new RemotingMediaSource(sourceId, mediaUrl);
    }

    /**
     * Returns a new {@link MediaRouteSelector} to use for Cast device filtering for this
     * particular media source or null if the application id is invalid.
     *
     * @return an initialized route selector or null.
     */
    @Override
    public MediaRouteSelector buildRouteSelector() {
        return new MediaRouteSelector.Builder()
                .addControlCategory(CastMediaControlIntent.categoryForCast(getApplicationId()))
                .build();
    }

    /**
     * Lazily loads a custom App ID from the AndroidManifest, which can be overriden
     * downstream. This app ID will never change, so we can store it in a static field.
     * If there is no custom app ID defined, or if there is an error retreiving the app ID,
     * we fallback to the default media receiver app ID.
     *
     * @return a custom app ID or the default media receiver app ID.
     */
    private static String applicationId() {
        if (sApplicationId == null) {
            String customAppId = null;

            try {
                Context context = ContextUtils.getApplicationContext();
                ApplicationInfo ai =
                        context.getPackageManager()
                                .getApplicationInfo(
                                        context.getPackageName(), PackageManager.GET_META_DATA);
                Bundle bundle = ai.metaData;
                customAppId = bundle.getString(REMOTE_PLAYBACK_APP_ID_KEY);
            } catch (Exception e) {
                // Should never happen, implies a corrupt AndroidManifest.
            }

            sApplicationId =
                    (customAppId != null && !customAppId.isEmpty())
                            ? customAppId
                            : CastMediaControlIntent.DEFAULT_MEDIA_RECEIVER_APPLICATION_ID;
        }

        return sApplicationId;
    }

    /** @return the Cast application id corresponding to the source. Can be overridden downstream. */
    @Override
    public String getApplicationId() {
        return applicationId();
    }

    /** @return the id identifying the media source */
    @Override
    public String getSourceId() {
        return mSourceId;
    }

    /** @return the media URL to fling to the Cast device. */
    public String getMediaUrl() {
        return mMediaUrl;
    }

    private RemotingMediaSource(String sourceId, String mediaUrl) {
        mSourceId = sourceId;
        mMediaUrl = mediaUrl;
    }
}
