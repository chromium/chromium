// Copyright 2014 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.components.media_router.cast_emulator.remote;

import android.app.PendingIntent;
import android.content.Context;
import android.content.Intent;
import android.content.IntentFilter;
import android.content.IntentFilter.MalformedMimeTypeException;
import android.content.pm.ApplicationInfo;
import android.content.pm.PackageManager;
import android.content.pm.PackageManager.NameNotFoundException;
import android.media.AudioManager;
import android.media.MediaRouter;
import android.net.Uri;
import android.os.Bundle;

import androidx.mediarouter.media.MediaControlIntent;
import androidx.mediarouter.media.MediaRouteDescriptor;
import androidx.mediarouter.media.MediaRouteProvider;
import androidx.mediarouter.media.MediaRouteProviderDescriptor;
import androidx.mediarouter.media.MediaRouter.ControlRequestCallback;
import androidx.mediarouter.media.MediaSessionStatus;

import com.google.android.gms.cast.CastMediaControlIntent;

import org.chromium.base.Log;
import org.chromium.components.media_router.cast_emulator.RoutePublisher;

import java.util.ArrayList;
import java.util.List;

/**
 * Media route publisher for testing remote playback from Chrome.
 *
 * @see TestMediaRouteProviderService
 */
public final class RemotePlaybackRoutePublisher implements RoutePublisher {
    private static final String MANIFEST_CAST_KEY =
            "com.google.android.apps.chrome.tests.support.CAST_ID";

    private static final String TAG = "CastEmulator";

    private static final String VARIABLE_VOLUME_SESSION_ROUTE_ID = "variable_session";
    private static final int VOLUME_MAX = 10;

    private int mVolume = 5;

    private final MediaRouteProvider mProvider;
    private final List<String> mControlCategories = new ArrayList<String>();

    public RemotePlaybackRoutePublisher(MediaRouteProvider provider) {
        mProvider = provider;

        mControlCategories.add(CastMediaControlIntent.categoryForRemotePlayback(getCastId()));
        mControlCategories.add(CastMediaControlIntent.categoryForRemotePlayback());
    }

    @Override
    public boolean supportsRoute(String routeId) {
        return routeId.equals(VARIABLE_VOLUME_SESSION_ROUTE_ID);
    }

    @Override
    public MediaRouteProvider.RouteController onCreateRouteController(String routeId) {
        return new TestMediaController(routeId);
    }

    @Override
    public boolean supportsControlCategory(String controlCategory) {
        return mControlCategories.contains(controlCategory);
    }

    @Override
    public void publishRoutes() {
        Log.i(TAG, "publishing routes");

        String castId = getCastId();

        IntentFilter f1 = new IntentFilter();
        f1.addCategory(MediaControlIntent.CATEGORY_REMOTE_PLAYBACK);
        f1.addCategory(CastMediaControlIntent.categoryForRemotePlayback(castId));
        f1.addAction(MediaControlIntent.ACTION_PLAY);
        f1.addDataScheme("http");
        f1.addDataScheme("https");
        addDataTypeUnchecked(f1, "video/*");

        IntentFilter f2 = new IntentFilter();
        f2.addCategory(MediaControlIntent.CATEGORY_REMOTE_PLAYBACK);
        f2.addCategory(CastMediaControlIntent.categoryForRemotePlayback());
        f2.addAction(MediaControlIntent.ACTION_SEEK);
        f2.addAction(MediaControlIntent.ACTION_GET_STATUS);
        f2.addAction(MediaControlIntent.ACTION_PAUSE);
        f2.addAction(MediaControlIntent.ACTION_RESUME);
        f2.addAction(MediaControlIntent.ACTION_STOP);
        f2.addAction(MediaControlIntent.ACTION_START_SESSION);
        f2.addAction(MediaControlIntent.ACTION_GET_SESSION_STATUS);
        f2.addAction(MediaControlIntent.ACTION_END_SESSION);
        f2.addAction(CastMediaControlIntent.ACTION_SYNC_STATUS);

        ArrayList<IntentFilter> controlFilters = new ArrayList<IntentFilter>();
        controlFilters.add(f1);
        controlFilters.add(f2);

        MediaRouteDescriptor testRouteDescriptor =
                new MediaRouteDescriptor.Builder(
                                VARIABLE_VOLUME_SESSION_ROUTE_ID, "Cast Test Route")
                        .setDescription("Cast Test Route")
                        .addControlFilters(controlFilters)
                        .setPlaybackStream(AudioManager.STREAM_MUSIC)
                        .setPlaybackType(MediaRouter.RouteInfo.PLAYBACK_TYPE_REMOTE)
                        .setVolumeHandling(MediaRouter.RouteInfo.PLAYBACK_VOLUME_VARIABLE)
                        .setVolumeMax(VOLUME_MAX)
                        .setVolume(mVolume)
                        .build();

        MediaRouteProviderDescriptor providerDescriptor =
                new MediaRouteProviderDescriptor.Builder().addRoute(testRouteDescriptor).build();
        mProvider.setDescriptor(providerDescriptor);
    }

    private String getCastId() {
        String castId = CastMediaControlIntent.DEFAULT_MEDIA_RECEIVER_APPLICATION_ID;
        try {
            // Downstream cast uses a different, private, castId; so read this from
            // the manifest.
            ApplicationInfo ai;
            ai =
                    mProvider
                            .getContext()
                            .getPackageManager()
                            .getApplicationInfo(
                                    mProvider.getContext().getPackageName(),
                                    PackageManager.GET_META_DATA);
            Bundle bundle = ai.metaData;
            if (bundle != null) {
                castId = bundle.getString(MANIFEST_CAST_KEY, castId);
            }
        } catch (NameNotFoundException e) {
            // Should never happen, do nothing - use default
        }
        return castId;
    }

    private final class TestMediaController extends MediaRouteProvider.RouteController {
        private final String mRouteId;
        private final LocalSessionManager mSessionManager =
                new LocalSessionManager(mProvider.getContext());
        private PendingIntent mSessionReceiver;
        private Bundle mMetadata;

        public TestMediaController(String routeId) {
            mRouteId = routeId;
            mSessionManager.setCallback(
                    new LocalSessionManager.Callback() {
                        @Override
                        public void onItemChanged(MediaItem item) {
                            handleStatusChange(item);
                        }
                    });
            setVolumeInternal(mVolume);
            Log.v(TAG, "%s: Controller created", mRouteId);
        }

        @Override
        public void onRelease() {
            Log.v(TAG, "%s: Controller released", mRouteId);
        }

        @Override
        public void onSelect() {
            Log.v(TAG, "%s: Selected", mRouteId);
        }

        @Override
        public void onUnselect() {
            Log.v(TAG, "%s: Unselected", mRouteId);
        }

        @Override
        public void onSetVolume(int volume) {
            Log.v(TAG, "%s: Set volume to %d", mRouteId, volume);
            setVolumeInternal(volume);
        }

        @Override
        public void onUpdateVolume(int delta) {
            Log.v(TAG, "%s: Update volume by %d", mRouteId, delta);
            setVolumeInternal(mVolume + delta);
        }

        @Override
        public boolean onControlRequest(Intent intent, ControlRequestCallback callback) {
            Log.v(TAG, "%s: Received control request %s", mRouteId, intent);
            String action = intent.getAction();
            if (intent.hasCategory(MediaControlIntent.CATEGORY_REMOTE_PLAYBACK)
                    || intent.hasCategory(CastMediaControlIntent.categoryForRemotePlayback())) {
                boolean success = false;
                if (action.equals(MediaControlIntent.ACTION_PLAY)) {
                    success = handlePlay(intent, callback);
                } else if (action.equals(MediaControlIntent.ACTION_SEEK)) {
                    success = handleSeek(intent, callback);
                } else if (action.equals(MediaControlIntent.ACTION_GET_STATUS)) {
                    success = handleGetStatus(intent, callback);
                } else if (action.equals(MediaControlIntent.ACTION_PAUSE)) {
                    success = handlePause(intent, callback);
                } else if (action.equals(MediaControlIntent.ACTION_RESUME)) {
                    success = handleResume(intent, callback);
                } else if (action.equals(MediaControlIntent.ACTION_STOP)) {
                    success = handleStop(intent, callback);
                } else if (action.equals(MediaControlIntent.ACTION_START_SESSION)) {
                    success = handleStartSession(intent, callback);
                } else if (action.equals(MediaControlIntent.ACTION_GET_SESSION_STATUS)) {
                    success = handleGetSessionStatus(intent, callback);
                } else if (action.equals(MediaControlIntent.ACTION_END_SESSION)) {
                    success = handleEndSession(intent, callback);
                } else if (action.equals(CastMediaControlIntent.ACTION_SYNC_STATUS)) {
                    success = handleSyncStatus(intent, callback);
                }
                Log.v(TAG, mSessionManager.getSessionStatusString());
                return success;
            }

            return false;
        }

        private boolean handleSyncStatus(Intent intent, ControlRequestCallback callback) {
            String sid = intent.getStringExtra(MediaControlIntent.EXTRA_SESSION_ID);
            Log.v(TAG, "%s: Received syncStatus request, sid=%s", mRouteId, sid);

            MediaItem item = mSessionManager.getCurrentItem();
            if (callback != null) {
                Bundle result = new Bundle();
                if (item != null) {
                    String iid = item.getItemId();
                    result.putString(MediaControlIntent.EXTRA_ITEM_ID, iid);
                    result.putBundle(
                            MediaControlIntent.EXTRA_ITEM_STATUS, item.getStatus().asBundle());
                    if (mMetadata != null) {
                        result.putBundle(MediaControlIntent.EXTRA_ITEM_METADATA, mMetadata);
                    }
                }
                callback.onResult(result);
            }
            return true;
        }

        private void setVolumeInternal(int volume) {
            if (volume >= 0 && volume <= VOLUME_MAX) {
                mVolume = volume;
                Log.v(TAG, "%s: New volume is %d", mRouteId, mVolume);
                AudioManager audioManager =
                        (AudioManager)
                                mProvider.getContext().getSystemService(Context.AUDIO_SERVICE);
                audioManager.setStreamVolume(AudioManager.STREAM_MUSIC, volume, 0);
                publishRoutes();
            }
        }

        private boolean handlePlay(Intent intent, ControlRequestCallback callback) {
            String sid = intent.getStringExtra(MediaControlIntent.EXTRA_SESSION_ID);
            if (sid != null && !sid.equals(mSessionManager.getSessionId())) {
                Log.v(TAG, "handlePlay fails because of bad sid=%s", sid);
                return false;
            }
            if (mSessionManager.hasSession()) {
                mSessionManager.stop();
            }

            Uri uri = intent.getData();
            if (uri == null) {
                Log.v(TAG, "handlePlay fails because of null uri");
                return false;
            }

            mMetadata = intent.getBundleExtra(MediaControlIntent.EXTRA_ITEM_METADATA);
            PendingIntent receiver =
                    (PendingIntent)
                            intent.getParcelableExtra(
                                    MediaControlIntent.EXTRA_ITEM_STATUS_UPDATE_RECEIVER);
            long position = intent.getLongExtra(MediaControlIntent.EXTRA_ITEM_CONTENT_POSITION, 0);

            Log.v(
                    TAG,
                    "%s: Received play request {%s}",
                    mRouteId,
                    getMediaControlIntentDebugString(intent));
            // Add the video to the session manager.
            MediaItem item = mSessionManager.add(uri, intent.getType(), receiver, position);
            // And start it playing.
            mSessionManager.resume();
            if (callback != null) {
                Bundle result = new Bundle();
                result.putString(MediaControlIntent.EXTRA_SESSION_ID, item.getSessionId());
                result.putString(MediaControlIntent.EXTRA_ITEM_ID, item.getItemId());
                result.putBundle(MediaControlIntent.EXTRA_ITEM_STATUS, item.getStatus().asBundle());
                callback.onResult(result);
            }
            return true;
        }

        private boolean handleSeek(Intent intent, ControlRequestCallback callback) {
            String sid = intent.getStringExtra(MediaControlIntent.EXTRA_SESSION_ID);
            if (sid == null || !sid.equals(mSessionManager.getSessionId())) {
                return false;
            }

            String iid = intent.getStringExtra(MediaControlIntent.EXTRA_ITEM_ID);
            long pos = intent.getLongExtra(MediaControlIntent.EXTRA_ITEM_CONTENT_POSITION, 0);
            Log.v(TAG, "%s: Received seek request, pos=%d", mRouteId, pos);
            MediaItem item = mSessionManager.seek(iid, pos);
            if (callback != null) {
                Bundle result = new Bundle();
                result.putBundle(MediaControlIntent.EXTRA_ITEM_STATUS, item.getStatus().asBundle());
                callback.onResult(result);
            }
            return true;
        }

        private boolean handleGetStatus(Intent intent, ControlRequestCallback callback) {
            String sid = intent.getStringExtra(MediaControlIntent.EXTRA_SESSION_ID);
            String iid = intent.getStringExtra(MediaControlIntent.EXTRA_ITEM_ID);
            Log.v(TAG, "%s: Received getStatus request, sid=%s, iid=%s", mRouteId, sid, iid);
            MediaItem item = mSessionManager.getStatus(iid);
            if (callback != null) {
                if (item != null) {
                    Bundle result = new Bundle();
                    result.putBundle(
                            MediaControlIntent.EXTRA_ITEM_STATUS, item.getStatus().asBundle());
                    callback.onResult(result);
                } else {
                    callback.onError("Failed to get status, sid=" + sid + ", iid=" + iid, null);
                }
            }
            return (item != null);
        }

        private boolean handlePause(Intent intent, ControlRequestCallback callback) {
            String sid = intent.getStringExtra(MediaControlIntent.EXTRA_SESSION_ID);
            boolean success = (sid != null) && sid.equals(mSessionManager.getSessionId());
            mSessionManager.pause();
            if (callback != null) {
                if (success) {
                    callback.onResult(new Bundle());
                    handleSessionStatusChange(sid);
                } else {
                    callback.onError("Failed to pause, sid=" + sid, null);
                }
            }
            return success;
        }

        private boolean handleResume(Intent intent, ControlRequestCallback callback) {
            String sid = intent.getStringExtra(MediaControlIntent.EXTRA_SESSION_ID);
            boolean success = (sid != null) && sid.equals(mSessionManager.getSessionId());
            mSessionManager.resume();
            if (callback != null) {
                if (success) {
                    callback.onResult(new Bundle());
                    handleSessionStatusChange(sid);
                } else {
                    callback.onError("Failed to resume, sid=" + sid, null);
                }
            }
            return success;
        }

        private boolean handleStop(Intent intent, ControlRequestCallback callback) {
            String sid = intent.getStringExtra(MediaControlIntent.EXTRA_SESSION_ID);
            boolean success = (sid != null) && sid.equals(mSessionManager.getSessionId());
            mSessionManager.stop();
            if (callback != null) {
                if (success) {
                    callback.onResult(new Bundle());
                    handleSessionStatusChange(sid);
                } else {
                    callback.onError("Failed to stop, sid=" + sid, null);
                }
            }
            return success;
        }

        private boolean handleStartSession(Intent intent, ControlRequestCallback callback) {
            boolean relaunch =
                    intent.getBooleanExtra(
                            CastMediaControlIntent.EXTRA_CAST_RELAUNCH_APPLICATION, true);
            String sid = mSessionManager.startSession(relaunch);
            Log.v(TAG, "StartSession returns sessionId %s", sid);
            if (callback != null) {
                if (sid != null) {
                    Bundle result = new Bundle();
                    result.putString(MediaControlIntent.EXTRA_SESSION_ID, sid);
                    result.putBundle(
                            MediaControlIntent.EXTRA_SESSION_STATUS,
                            mSessionManager.getSessionStatus(sid).asBundle());
                    Log.v(TAG, "StartSession sends result of $s", result);
                    callback.onResult(result);
                    mSessionReceiver =
                            (PendingIntent)
                                    intent.getParcelableExtra(
                                            MediaControlIntent
                                                    .EXTRA_SESSION_STATUS_UPDATE_RECEIVER);
                    handleSessionStatusChange(sid);
                } else {
                    callback.onError("Failed to start session.", null);
                }
            }
            return (sid != null);
        }

        private boolean handleGetSessionStatus(Intent intent, ControlRequestCallback callback) {
            String sid = intent.getStringExtra(MediaControlIntent.EXTRA_SESSION_ID);

            MediaSessionStatus sessionStatus = mSessionManager.getSessionStatus(sid);
            if (callback != null) {
                if (sessionStatus != null) {
                    Bundle result = new Bundle();
                    result.putBundle(
                            MediaControlIntent.EXTRA_SESSION_STATUS,
                            mSessionManager.getSessionStatus(sid).asBundle());
                    callback.onResult(result);
                } else {
                    callback.onError("Failed to get session status, sid=" + sid, null);
                }
            }
            return (sessionStatus != null);
        }

        private boolean handleEndSession(Intent intent, ControlRequestCallback callback) {
            String sid = intent.getStringExtra(MediaControlIntent.EXTRA_SESSION_ID);
            boolean success =
                    (sid != null)
                            && sid.equals(mSessionManager.getSessionId())
                            && mSessionManager.endSession();
            if (callback != null) {
                if (success) {
                    Bundle result = new Bundle();
                    MediaSessionStatus sessionStatus =
                            new MediaSessionStatus.Builder(MediaSessionStatus.SESSION_STATE_ENDED)
                                    .build();
                    result.putBundle(
                            MediaControlIntent.EXTRA_SESSION_STATUS, sessionStatus.asBundle());
                    callback.onResult(result);
                    handleSessionStatusChange(sid);
                    mSessionReceiver = null;
                } else {
                    callback.onError("Failed to end session, sid=" + sid, null);
                }
            }
            return success;
        }

        private void handleStatusChange(MediaItem item) {
            if (item == null) {
                item = mSessionManager.getCurrentItem();
            }
            if (item != null) {
                PendingIntent receiver = item.getUpdateReceiver();
                if (receiver != null) {
                    Intent intent = new Intent();
                    intent.putExtra(MediaControlIntent.EXTRA_SESSION_ID, item.getSessionId());
                    intent.putExtra(MediaControlIntent.EXTRA_ITEM_ID, item.getItemId());
                    intent.putExtra(
                            MediaControlIntent.EXTRA_ITEM_STATUS, item.getStatus().asBundle());
                    try {
                        receiver.send(mProvider.getContext(), 0, intent);
                        Log.v(TAG, "%s: Sending status update from provider", mRouteId);
                    } catch (PendingIntent.CanceledException e) {
                        Log.v(TAG, "%s: Failed to send status update!", mRouteId);
                    }
                }
            }
        }

        private void handleSessionStatusChange(String sid) {
            if (mSessionReceiver != null) {
                Intent intent = new Intent();
                intent.putExtra(MediaControlIntent.EXTRA_SESSION_ID, sid);
                intent.putExtra(
                        MediaControlIntent.EXTRA_SESSION_STATUS,
                        mSessionManager.getSessionStatus(sid).asBundle());
                try {
                    mSessionReceiver.send(mProvider.getContext(), 0, intent);
                    Log.v(TAG, "%s: Sending session status update from provider", mRouteId);
                } catch (PendingIntent.CanceledException e) {
                    Log.v(TAG, "%s: Failed to send session status update!", mRouteId);
                }
            }
        }
    }

    private String getMediaControlIntentDebugString(Intent intent) {
        return "uri="
                + intent.getData()
                + ", mime="
                + intent.getType()
                + ", sid="
                + intent.getStringExtra(MediaControlIntent.EXTRA_SESSION_ID)
                + ", pos="
                + intent.getLongExtra(MediaControlIntent.EXTRA_ITEM_CONTENT_POSITION, 0)
                + ", metadata="
                + intent.getBundleExtra(MediaControlIntent.EXTRA_ITEM_METADATA)
                + ", headers="
                + intent.getBundleExtra(MediaControlIntent.EXTRA_ITEM_HTTP_HEADERS);
    }

    private static void addDataTypeUnchecked(IntentFilter filter, String type) {
        try {
            filter.addDataType(type);
        } catch (MalformedMimeTypeException ex) {
            throw new RuntimeException(ex);
        }
    }
}
