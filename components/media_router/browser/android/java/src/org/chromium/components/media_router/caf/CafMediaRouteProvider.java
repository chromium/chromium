// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.components.media_router.caf;

import static org.chromium.components.media_router.caf.CastUtils.isSameOrigin;

import androidx.annotation.NonNull;
import androidx.annotation.Nullable;
import androidx.annotation.VisibleForTesting;
import androidx.mediarouter.media.MediaRouter;

import com.google.android.gms.cast.framework.CastSession;

import org.chromium.base.Log;
import org.chromium.components.media_router.BrowserMediaRouter;
import org.chromium.components.media_router.ClientRecord;
import org.chromium.components.media_router.MediaRoute;
import org.chromium.components.media_router.MediaRouteManager;
import org.chromium.components.media_router.MediaRouteProvider;
import org.chromium.components.media_router.MediaSink;
import org.chromium.components.media_router.MediaSource;

import java.util.HashMap;
import java.util.Map;

/** A {@link MediaRouteProvider} implementation for Cast devices and applications, using Cast v3 API. */
public class CafMediaRouteProvider extends CafBaseMediaRouteProvider {
    private static final String TAG = "CafMRP";

    private static final String AUTO_JOIN_PRESENTATION_ID = "auto-join";
    private static final String PRESENTATION_ID_SESSION_ID_PREFIX = "cast-session_";

    @VisibleForTesting ClientRecord mLastRemovedRouteRecord;
    // The records for clients, which must match mRoutes. This is used for the saving last record
    // for autojoin.
    private final Map<String, ClientRecord> mClientIdToRecords =
            new HashMap<String, ClientRecord>();
    @VisibleForTesting CafMessageHandler mMessageHandler;
    // The session controller which is always attached to the current CastSession.
    private final CastSessionController mSessionController;

    public static CafMediaRouteProvider create(MediaRouteManager manager) {
        return new CafMediaRouteProvider(BrowserMediaRouter.getAndroidMediaRouter(), manager);
    }

    public Map<String, ClientRecord> getClientIdToRecords() {
        return mClientIdToRecords;
    }

    @Override
    public void joinRoute(
            String sourceId, String presentationId, String origin, int tabId, int nativeRequestId) {
        CastMediaSource source = CastMediaSource.from(sourceId);
        if (source == null || source.getClientId() == null) {
            mManager.onJoinRouteRequestError("Unsupported presentation URL", nativeRequestId);
            return;
        }

        if (!hasSession()) {
            mManager.onJoinRouteRequestError("No presentation", nativeRequestId);
            return;
        }

        if (!canJoinExistingSession(presentationId, origin, tabId, source)) {
            mManager.onJoinRouteRequestError("No matching route", nativeRequestId);
            return;
        }

        MediaRoute route =
                new MediaRoute(sessionController().getSink().getId(), sourceId, presentationId);
        addRoute(route, origin, tabId, nativeRequestId, /* wasLaunched= */ false);
    }

    // TODO(zqzhang): the clientRecord/route management is not clean and the logic seems to be
    // problematic.
    @Override
    public void closeRoute(String routeId) {
        boolean isRouteInRecord = mRoutes.containsKey(routeId);

        super.closeRoute(routeId);

        if (!isRouteInRecord) return;

        ClientRecord client = getClientRecordByRouteId(routeId);
        if (client != null) {
            MediaSink sink = sessionController().getSink();
            if (sink != null) {
                mMessageHandler.sendReceiverActionToClient(routeId, sink, client.clientId, "stop");
            }
        }
    }

    @Override
    public void sendStringMessage(String routeId, String message) {
        Log.d(TAG, "Received message from client: %s", message);

        if (!mRoutes.containsKey(routeId)) {
            return;
        }

        mMessageHandler.handleMessageFromClient(message);
    }

    @Override
    protected MediaSource getSourceFromId(String sourceId) {
        return CastMediaSource.from(sourceId);
    }

    @Override
    public BaseSessionController sessionController() {
        return mSessionController;
    }

    public void sendMessageToClient(String clientId, String message) {
        ClientRecord clientRecord = mClientIdToRecords.get(clientId);
        if (clientRecord == null) return;

        if (!clientRecord.isConnected) {
            Log.d(TAG, "Queueing message to client %s: %s", clientId, message);
            clientRecord.pendingMessages.add(message);
            return;
        }

        // Flush pending messages, if any, before sending the current message to ensure that the
        // messages are delivered in order.
        flushPendingMessagesToClient(clientRecord);
        Log.d(TAG, "Sending message to client %s: %s", clientId, message);
        mManager.onMessage(clientRecord.routeId, message);
    }

    /** Flushes all pending messages in record to a client. */
    public void flushPendingMessagesToClient(ClientRecord clientRecord) {
        for (String message : clientRecord.pendingMessages) {
            Log.d(TAG, "Deqeueing message for client %s: %s", clientRecord.clientId, message);
            mManager.onMessage(clientRecord.routeId, message);
        }
        clientRecord.pendingMessages.clear();
    }

    @NonNull
    public CafMessageHandler getMessageHandler() {
        return mMessageHandler;
    }

    @Override
    protected void handleSessionStart(CastSession session, String sessionId) {
        super.handleSessionStart(session, sessionId);

        for (ClientRecord clientRecord : mClientIdToRecords.values()) {
            // Should be exactly one instance of MediaRoute/ClientRecord at this moment.
            mMessageHandler.sendReceiverActionToClient(
                    clientRecord.routeId,
                    sessionController().getSink(),
                    clientRecord.clientId,
                    "cast");
        }

        mMessageHandler.onSessionStarted();
        sessionController().getSession().getRemoteMediaClient().requestStatus();
    }

    @Override
    protected void addRoute(
            MediaRoute route, String origin, int tabId, int nativeRequestId, boolean wasLaunched) {
        super.addRoute(route, origin, tabId, nativeRequestId, wasLaunched);
        CastMediaSource source = CastMediaSource.from(route.getSourceId());
        final String clientId = source.getClientId();

        if (clientId == null || mClientIdToRecords.containsKey(clientId)) return;

        mClientIdToRecords.put(
                clientId,
                new ClientRecord(
                        route.id,
                        clientId,
                        source.getApplicationId(),
                        source.getAutoJoinPolicy(),
                        origin,
                        tabId));
    }

    @Override
    protected void removeRouteFromRecord(String routeId) {
        ClientRecord record = getClientRecordByRouteId(routeId);
        if (record != null) {
            mLastRemovedRouteRecord = mClientIdToRecords.remove(record.clientId);
        }
        super.removeRouteFromRecord(routeId);
    }

    @Nullable
    private ClientRecord getClientRecordByRouteId(String routeId) {
        for (ClientRecord record : mClientIdToRecords.values()) {
            if (record.routeId.equals(routeId)) return record;
        }
        return null;
    }

    private CafMediaRouteProvider(MediaRouter androidMediaRouter, MediaRouteManager manager) {
        super(androidMediaRouter, manager);
        mSessionController = new CastSessionController(this);
        mMessageHandler = new CafMessageHandler(this, mSessionController);
    }

    @VisibleForTesting
    boolean canJoinExistingSession(
            String presentationId, String origin, int tabId, CastMediaSource source) {
        if (AUTO_JOIN_PRESENTATION_ID.equals(presentationId)) {
            return canAutoJoin(source, origin, tabId);
        }
        if (presentationId.startsWith(PRESENTATION_ID_SESSION_ID_PREFIX)) {
            String sessionId = presentationId.substring(PRESENTATION_ID_SESSION_ID_PREFIX.length());
            return sessionId != null && sessionId.equals(sessionController().getSessionId());
        }
        for (MediaRoute route : mRoutes.values()) {
            if (route.presentationId.equals(presentationId)) return true;
        }
        return false;
    }

    private boolean canAutoJoin(CastMediaSource source, String origin, int tabId) {
        if (source.getAutoJoinPolicy().equals(CastMediaSource.AUTOJOIN_PAGE_SCOPED)) return false;

        CastMediaSource currentSource = (CastMediaSource) sessionController().getSource();
        if (!currentSource.getApplicationId().equals(source.getApplicationId())) return false;

        if (mClientIdToRecords.isEmpty() && mLastRemovedRouteRecord != null) {
            return isSameOrigin(origin, mLastRemovedRouteRecord.origin)
                    && tabId == mLastRemovedRouteRecord.tabId;
        }

        if (mClientIdToRecords.isEmpty()) return false;

        ClientRecord client = mClientIdToRecords.values().iterator().next();

        if (source.getAutoJoinPolicy().equals(CastMediaSource.AUTOJOIN_ORIGIN_SCOPED)) {
            return isSameOrigin(origin, client.origin);
        }
        if (source.getAutoJoinPolicy().equals(CastMediaSource.AUTOJOIN_TAB_AND_ORIGIN_SCOPED)) {
            return isSameOrigin(origin, client.origin) && tabId == client.tabId;
        }

        return false;
    }
}
