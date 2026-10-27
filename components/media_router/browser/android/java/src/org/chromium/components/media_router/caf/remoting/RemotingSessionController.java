// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.components.media_router.caf.remoting;

import static org.chromium.build.NullUtil.assumeNonNull;

import com.google.android.gms.cast.framework.CastSession;

import org.chromium.base.Log;
import org.chromium.build.annotations.NullMarked;
import org.chromium.build.annotations.Nullable;
import org.chromium.components.media_router.CastSessionUtil;
import org.chromium.components.media_router.MediaSource;
import org.chromium.components.media_router.caf.BaseSessionController;
import org.chromium.components.media_router.caf.CafBaseMediaRouteProvider;

/** Wrapper for {@link CastSession} for remoting. */
@NullMarked
public class RemotingSessionController extends BaseSessionController {
    private static final String TAG = "RmtSessionCtrl";

    private @Nullable FlingingControllerAdapter mFlingingControllerAdapter;

    RemotingSessionController(CafBaseMediaRouteProvider provider) {
        super(provider);
        new RemotingNotificationController(this);
    }

    /**
     * Called when media source needs to be updated.
     *
     * @param source The new media source.
     */
    public void updateMediaSource(MediaSource source) {
        assumeNonNull(mFlingingControllerAdapter);
        mFlingingControllerAdapter.updateMediaUrl(((RemotingMediaSource) source).getMediaUrl());
        assumeNonNull(getRouteCreationInfo()).setMediaSource(source);
        getProvider().updateRouteMediaSource(getRouteCreationInfo().routeId, source.getSourceId());
    }

    @Override
    public void attachToCastSession(CastSession session) {
        super.attachToCastSession(session);

        try {
            assumeNonNull(getSession())
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
        assumeNonNull(mFlingingControllerAdapter);
        mFlingingControllerAdapter.onStatusUpdated();
        super.onStatusUpdated();
    }

    @Override
    public @Nullable FlingingControllerAdapter getFlingingController() {
        return mFlingingControllerAdapter;
    }
}
