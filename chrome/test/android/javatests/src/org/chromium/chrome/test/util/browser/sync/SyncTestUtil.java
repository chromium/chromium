// Copyright 2013 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.test.util.browser.sync;

import android.content.Context;
import android.util.Pair;

import org.hamcrest.Matchers;
import org.json.JSONArray;
import org.json.JSONException;
import org.json.JSONObject;

import org.chromium.base.ThreadUtils;
import org.chromium.base.test.util.CallbackHelper;
import org.chromium.base.test.util.Criteria;
import org.chromium.base.test.util.CriteriaHelper;
import org.chromium.chrome.browser.profiles.ProfileManager;
import org.chromium.chrome.browser.sync.SyncServiceFactory;
import org.chromium.components.sync.SyncService;
import org.chromium.components.sync.TransportState;
import org.chromium.components.sync.UserSelectableType;

import java.util.ArrayList;
import java.util.Iterator;
import java.util.List;
import java.util.Set;
import java.util.concurrent.TimeUnit;
import java.util.concurrent.TimeoutException;

/** Utility class for shared sync test functionality. */
public final class SyncTestUtil {
    private static final String TAG = "SyncTestUtil";

    public static final long TIMEOUT_MS = 20000L;
    public static final int INTERVAL_MS = 250;

    private SyncTestUtil() {}

    /**
     * Return the {@link SyncService} for the {@link ProfileManager#getLastUsedRegularProfile()}.
     */
    public static SyncService getSyncServiceForLastUsedProfile() {
        return ThreadUtils.runOnUiThreadBlocking(
                () -> {
                    return SyncServiceFactory.getForProfile(
                            ProfileManager.getLastUsedRegularProfile());
                });
    }

    /**
     * Returns whether the user has sync consent.
     *
     * <p>TODO(crbug.com/40066949): Remove once kSync becomes unreachable or is deleted from the
     * codebase. See ConsentLevel::kSync documentation for details.
     */
    public static boolean hasSyncConsent() {
        return ThreadUtils.runOnUiThreadBlocking(
                () -> getSyncServiceForLastUsedProfile().hasSyncConsent());
    }

    /** Returns whether sync-the-feature is enabled. */
    public static boolean isSyncFeatureEnabled() {
        return ThreadUtils.runOnUiThreadBlocking(
                () -> getSyncServiceForLastUsedProfile().isSyncFeatureEnabled());
    }

    /** Returns whether sync-the-feature is active. */
    public static boolean isSyncFeatureActive() {
        return ThreadUtils.runOnUiThreadBlocking(
                () -> getSyncServiceForLastUsedProfile().isSyncFeatureActive());
    }

    /**
     * Waits for sync-the-feature to become enabled.
     * WARNING: This is does not wait for the feature to be active, see the distinction in
     * components/sync/service/sync_service.h. If the FakeServer isn't running - e.g. because of
     * SyncTestRule - this is all you can hope for. For tests that don't rely on sync data this
     * might just be enough.
     */
    public static void waitForSyncFeatureEnabled() {
        CriteriaHelper.pollUiThread(
                () -> getSyncServiceForLastUsedProfile().isSyncFeatureEnabled(),
                "Timed out waiting for sync to become enabled.",
                TIMEOUT_MS,
                INTERVAL_MS);
    }

    /** Waits for sync-the-feature to become active. */
    public static void waitForSyncFeatureActive() {
        CriteriaHelper.pollUiThread(
                () -> getSyncServiceForLastUsedProfile().isSyncFeatureActive(),
                "Timed out waiting for sync to become active.",
                TIMEOUT_MS,
                INTERVAL_MS);
    }

    /**
     * Waits for hasSyncConsent() to return true.
     *
     * <p>TODO(crbug.com/40066949): Remove once kSync becomes unreachable or is deleted from the
     * codebase. See ConsentLevel::kSync documentation for details.
     */
    public static void waitForSyncConsent() {
        CriteriaHelper.pollUiThread(
                () -> getSyncServiceForLastUsedProfile().hasSyncConsent(),
                "Timed out waiting for sync consent.",
                TIMEOUT_MS,
                INTERVAL_MS);
    }

    /** Waits for sync machinery to become active. */
    public static void waitForSyncTransportActive() {
        CriteriaHelper.pollUiThread(
                () ->
                        getSyncServiceForLastUsedProfile().getTransportState()
                                == TransportState.ACTIVE,
                "Timed out waiting for sync transport state to become active.",
                TIMEOUT_MS,
                INTERVAL_MS);
    }

    /** Waits for sync's engine to be initialized. */
    public static void waitForEngineInitialized() {
        CriteriaHelper.pollUiThread(
                () -> getSyncServiceForLastUsedProfile().isEngineInitialized(),
                "Timed out waiting for sync's engine to initialize.",
                TIMEOUT_MS,
                INTERVAL_MS);
    }

    /** Waits for sync being in the desired TrustedVaultKeyRequired state. */
    public static void waitForTrustedVaultKeyRequired(boolean desiredValue) {
        CriteriaHelper.pollUiThread(
                () -> {
                    Criteria.checkThat(
                            getSyncServiceForLastUsedProfile().isTrustedVaultKeyRequired(),
                            Matchers.is(desiredValue));
                },
                TIMEOUT_MS,
                INTERVAL_MS);
    }

    /** Waits for sync being in the desired value for isTrustedVaultRecoverabilityDegraded(). */
    public static void waitForTrustedVaultRecoverabilityDegraded(boolean desiredValue) {
        CriteriaHelper.pollUiThread(
                () -> {
                    Criteria.checkThat(
                            getSyncServiceForLastUsedProfile()
                                    .isTrustedVaultRecoverabilityDegraded(),
                            Matchers.is(desiredValue));
                },
                TIMEOUT_MS,
                INTERVAL_MS);
    }

    /** Returns whether history sync is active. */
    public static boolean isHistorySyncEnabled() {
        return ThreadUtils.runOnUiThreadBlocking(
                () ->
                        getSyncServiceForLastUsedProfile()
                                .getSelectedTypes()
                                .containsAll(
                                        Set.of(
                                                UserSelectableType.HISTORY,
                                                UserSelectableType.TABS)));
    }

    /** Waits for history and tabs sync to be active. */
    public static void waitForHistorySyncEnabled() {
        CriteriaHelper.pollUiThread(
                () ->
                        getSyncServiceForLastUsedProfile()
                                .getSelectedTypes()
                                .containsAll(
                                        Set.of(
                                                UserSelectableType.HISTORY,
                                                UserSelectableType.TABS)));
    }

    /** Returns whether bookmarks and reading list are active. */
    public static boolean isBookmarksAndReadingListEnabled() {
        return ThreadUtils.runOnUiThreadBlocking(
                () ->
                        getSyncServiceForLastUsedProfile()
                                .getSelectedTypes()
                                .containsAll(
                                        Set.of(
                                                UserSelectableType.BOOKMARKS,
                                                UserSelectableType.READING_LIST)));
    }

    /** Waits for bookmarks and reading list to be active. */
    public static void waitForBookmarksAndReadingListEnabled() {
        CriteriaHelper.pollUiThread(
                () ->
                        getSyncServiceForLastUsedProfile()
                                .getSelectedTypes()
                                .containsAll(
                                        Set.of(
                                                UserSelectableType.BOOKMARKS,
                                                UserSelectableType.READING_LIST)));
    }

    /** Triggers a sync cycle. */
    public static void triggerSync() {
        ThreadUtils.runOnUiThreadBlocking(
                () -> {
                    getSyncServiceForLastUsedProfile().triggerRefresh();
                });
    }

    /**
     * Triggers a sync and tries to wait until it is complete.
     *
     * This method polls until *a* sync cycle completes, but there is no guarantee it is the same
     * one that it triggered. Therefore this method should only be used where it can result in
     * false positives (e.g. checking that something doesn't sync), not false negatives.
     */
    public static void triggerSyncAndWaitForCompletion() {
        final long oldSyncTime = getCurrentSyncTime();
        triggerSync();
        CriteriaHelper.pollInstrumentationThread(
                () -> {
                    Criteria.checkThat(getCurrentSyncTime(), Matchers.greaterThan(oldSyncTime));
                },
                TIMEOUT_MS,
                INTERVAL_MS);
    }

    private static long getCurrentSyncTime() {
        return ThreadUtils.runOnUiThreadBlocking(
                () -> getSyncServiceForLastUsedProfile().getLastSyncedTimeForDebugging());
    }

    /**
     * Retrieves the local Sync data as a JSONArray via SyncService.
     *
     * This method blocks until the data is available or until it times out.
     */
    private static JSONArray getAllNodesAsJsonArray() {
        class NodesCallbackHelper extends CallbackHelper {
            public JSONArray nodes;
        }
        NodesCallbackHelper callbackHelper = new NodesCallbackHelper();
        ThreadUtils.runOnUiThreadBlocking(
                () -> {
                    getSyncServiceForLastUsedProfile()
                            .getAllNodes(
                                    (nodes) -> {
                                        callbackHelper.nodes = nodes;
                                        callbackHelper.notifyCalled();
                                    });
                });

        try {
            callbackHelper.waitForNext(TIMEOUT_MS, TimeUnit.MILLISECONDS);
        } catch (TimeoutException e) {
            assert false : "Timed out waiting for SyncService.getAllNodes()";
        }

        return callbackHelper.nodes;
    }

    /**
     * Extracts datatype-specific information from the given JSONObject. The returned JSONObject
     * contains the same data as a specifics protocol buffer (e.g., ReadingListSpecifics).
     */
    private static JSONObject extractSpecifics(JSONObject node) throws JSONException {
        JSONObject specifics = node.getJSONObject("SPECIFICS");
        // The key name here is type-specific (e.g., "typed_url" for Typed URLs), so we
        // can't hard code a value.
        Iterator<String> keysIterator = specifics.keys();
        String key = null;
        if (!keysIterator.hasNext()) {
            throw new JSONException("Specifics object has 0 keys.");
        }
        key = keysIterator.next();

        if (keysIterator.hasNext()) {
            throw new JSONException("Specifics object has more than 1 key.");
        }

        if (key.equals("bookmark")) {
            JSONObject bookmarkSpecifics = specifics.getJSONObject(key);
            bookmarkSpecifics.put("parent_id", node.getString("PARENT_ID"));
            return bookmarkSpecifics;
        }

        JSONObject specificsWithMetadata = specifics.getJSONObject(key);
        if (node.has("metadata")) {
            specificsWithMetadata.put("metadata", node.getJSONObject("metadata"));
        }
        return specificsWithMetadata;
    }

    /**
     * Converts the given ID to the format stored by the server.
     *
     * <p>See the SyncableId (C++) class for more information about ID encoding. To paraphrase, the
     * client prepends "s" or "c" to the server's ID depending on the commit state of the data. IDs
     * can also be "r" to indicate the root node, but that entity is not supported here.
     *
     * @param clientId the ID to be converted
     * @return the converted ID
     */
    private static String convertToServerId(String clientId) {
        if (clientId == null) {
            throw new IllegalArgumentException("Client entity ID cannot be null.");
        } else if (clientId.isEmpty()) {
            throw new IllegalArgumentException("Client ID cannot be empty.");
        } else if (!clientId.startsWith("s") && !clientId.startsWith("c")) {
            throw new IllegalArgumentException(
                    String.format("Client ID (%s) must start with c or s.", clientId));
        }

        return clientId.substring(1);
    }

    /**
     * Returns the local Sync data present for a single datatype.
     *
     * <p>For each data entity, a Pair is returned. The first piece of data is the entity's server
     * ID. This is useful for activities like deleting an entity on the server. The second piece of
     * data is a JSONObject representing the datatype-specific information for the entity. This data
     * is the same as the data stored in a specifics protocol buffer (e.g., ReadingListSpecifics).
     *
     * @param context the Context used to retreive the correct SyncService
     * @param typeString a String representing a specific datatype.
     *     <p>TODO(pvalenzuela): Replace typeString with the native DataType enum or something else
     *     that will avoid callers needing to specify the native string version.
     * @return a List of Pair<String, JSONObject> representing the local Sync data
     */
    public static List<Pair<String, JSONObject>> getLocalData(Context context, String typeString)
            throws JSONException {
        JSONArray localData = getAllNodesAsJsonArray();
        JSONArray datatypeNodes = new JSONArray();
        for (int i = 0; i < localData.length(); i++) {
            JSONObject datatypeObject = localData.getJSONObject(i);
            if (datatypeObject.getString("type").equals(typeString)) {
                datatypeNodes = datatypeObject.getJSONArray("nodes");
                break;
            }
        }

        List<Pair<String, JSONObject>> localDataForDatatype =
                new ArrayList<Pair<String, JSONObject>>(datatypeNodes.length());
        for (int i = 0; i < datatypeNodes.length(); i++) {
            JSONObject entity = datatypeNodes.getJSONObject(i);
            if (entity.has("UNIQUE_SERVER_TAG")
                    && !entity.getString("UNIQUE_SERVER_TAG").isEmpty()) {
                // Ignore permanent items (e.g., root datatype folders).
                continue;
            }
            String id = convertToServerId(entity.getString("ID"));
            localDataForDatatype.add(Pair.create(id, extractSpecifics(entity)));
        }
        return localDataForDatatype;
    }

    /**
     * Encrypts the profile with the input |passphrase|. It will then block until the sync server is
     * successfully using the passphrase.
     */
    public static void encryptWithPassphrase(final String passphrase) {
        ThreadUtils.runOnUiThreadBlocking(
                () -> getSyncServiceForLastUsedProfile().setEncryptionPassphrase(passphrase));
        // Make sure the new encryption settings make it to the server.
        SyncTestUtil.triggerSyncAndWaitForCompletion();
    }

    /** Decrypts the profile using the input |passphrase|. */
    public static void decryptWithPassphrase(final String passphrase) {
        ThreadUtils.runOnUiThreadBlocking(
                () -> {
                    getSyncServiceForLastUsedProfile().setDecryptionPassphrase(passphrase);
                });
    }
}
