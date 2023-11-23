// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.components.media_router.caf.remoting;

import com.google.android.gms.cast.framework.CastSession;

import org.chromium.base.Log;
import org.chromium.components.media_router.CastSessionUtil;
import org.chromium.components.media_router.MediaSource;
import org.chromium.components.media_router.caf.BaseNotificationController;
import org.chromium.components.media_router.caf.BaseSessionController;
import org.chromium.components.media_router.caf.CafBaseMediaRouteProvider;

import java.lang.ref.WeakReference;

/** Wrapper for {@link CastSession} for remoting. */
public class RemotingSessionController extends BaseSessionController {
    private static final String TAG = "RmtSessionCtrl";

    private static WeakReference<RemotingSessionController> sInstance;

    public static RemotingSessionController getInstance() {
        return sInstance != null ? sInstance.get() : null;
    }

    private FlingingControllerAdapter mFlingingControllerAdapter;
    private RemotingNotificationController mNotificationController;

    RemotingSessionController(CafBaseMediaRouteProvider provider) {
        super(provider);
        mNotificationController = new RemotingNotificationController(this);
        sInstance = new WeakReference<>(this);
    }

    /**
     * Called when media source needs to be updated.
     *
     * @param source The new media source.
     */
    public void updateMediaSource(MediaSource source) {
        mFlingingControllerAdapter.updateMediaUrl(((RemotingMediaSource) source).getMediaUrl());
        getRouteCreationInfo().setMediaSource(source);
        getProvider().updateRouteMediaSource(getRouteCreationInfo().routeId, source.getSourceId());
    }

    @Override
    public void attachToCastSession(CastSession session) {
        super.attachToCastSession(session);

        try {
            getSession()
                    .setMessageReceivedCallbacks(
                            CastSessionUtil.MEDIA_NAMESPACE, this::onMessageReceived);
        } catch (Exception e) {
            Log.e(
                    TAG,
                    "Failed to register namespace listener for %s",
                    CastSessionUtil.MEDIA_NAMESPACE,
                    e);
        }
    }

    @Override
    public void onSessionStarted() {
        super.onSessionStarted();
        RemotingMediaSource source = (RemotingMediaSource) getSource();
        if (source != null) {
            mFlingingControllerAdapter = new FlingingControllerAdapter(this, source.getMediaUrl());
        } else {
            throw new AssertionError("Remoting Session started with an invalid source.");
        }
    }

    @Override
    protected void onStatusUpdated() {
        mFlingingControllerAdapter.onStatusUpdated();
        super.onStatusUpdated();
    }

    @Override
    public FlingingControllerAdapter getFlingingController() {
        return mFlingingControllerAdapter;
    }

    @Override
    public BaseNotificationController getNotificationController() {
        return mNotificationController;
    }
}
