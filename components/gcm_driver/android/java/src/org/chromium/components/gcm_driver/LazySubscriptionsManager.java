// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.components.gcm_driver;

import android.content.Context;
import android.content.SharedPreferences;

import androidx.annotation.VisibleForTesting;

import org.json.JSONArray;
import org.json.JSONException;
import org.json.JSONObject;

import org.chromium.base.ContextUtils;
import org.chromium.base.Log;
import org.chromium.base.StrictModeContext;

import java.util.ArrayList;
import java.util.Collections;
import java.util.HashSet;
import java.util.List;
import java.util.Set;

/**
 * This class is responsible for managing lazy subscriptions. It provides API to change and query
 * whether a subscription is lazy, and toto persist and retrieve persisted messages.
 */
public class LazySubscriptionsManager {
    private static final String TAG = "LazySubscriptions";
    private static final String FCM_LAZY_SUBSCRIPTIONS = "fcm_lazy_subscriptions";
    static final String LEGACY_HAS_PERSISTED_MESSAGES_KEY = "has_persisted_messages";
    private static final String SUBSCRIPTIONS_WITH_PERSISTED_MESSAGES_KEY =
            "subscriptions_with_persisted_messages";
    private static final String PREF_PACKAGE =
            "org.chromium.components.gcm_driver.lazy_subscriptions";

    // The max number of most recent messages queued per lazy subscription until
    // Chrome is foregrounded.
    @VisibleForTesting public static final int MESSAGES_QUEUE_SIZE = 3;

    // Private constructor because all methods in this class are static, and it
    // shouldn't be instantiated.
    private LazySubscriptionsManager() {}

    /**
     * A one time migration from the deprecated "has persisted messages" boolean
     * flag to a set of subscription ids that have persisted messages. If the
     * global flag is set, it add all lazy subscription ids have persisted
     * messages and then clears the global flag.
     */
    public static void migrateHasPersistedMessagesPref() {
        SharedPreferences sharedPrefs = ContextUtils.getAppSharedPreferences();
        boolean hasPersistedMessages =
                sharedPrefs.getBoolean(LEGACY_HAS_PERSISTED_MESSAGES_KEY, false);
        if (!hasPersistedMessages) {
            return;
        }
        Set<String> lazySubscriptionIds = getLazySubscriptionIds();
        sharedPrefs
                .edit()
                .putStringSet(SUBSCRIPTIONS_WITH_PERSISTED_MESSAGES_KEY, lazySubscriptionIds)
                .apply();
        sharedPrefs.edit().remove(LEGACY_HAS_PERSISTED_MESSAGES_KEY).apply();
    }

    /**
     * Adds/Removes the |subscriptionId| to indicate whether there are any persisted messages to
     * read for this |subscriptionId|. This information could be read using
     * hasPersistedMessagesForSubscription().
     */
    public static void storeHasPersistedMessagesForSubscription(
            final String subscriptionId, boolean hasPersistedMessages) {
        // Stores the information in the default preferences instead of special
        // one for the GCM messages. The reason is the default preferences file
        // is used in many places in Chrome and should be already cached in
        // memory by the time this method is called. Therefore, it should
        // provide a cheap way that (most probably) doesn't require disk access
        // to read that flag.
        SharedPreferences sharedPrefs = ContextUtils.getAppSharedPreferences();
        Set<String> subscriptionsWithPersistedMessages =
                new HashSet<>(
                        sharedPrefs.getStringSet(
                                SUBSCRIPTIONS_WITH_PERSISTED_MESSAGES_KEY, Collections.emptySet()));
        if (subscriptionsWithPersistedMessages.contains(subscriptionId) == hasPersistedMessages) {
            // Correct information are already stored, nothing to do.
            return;
        }
        if (hasPersistedMessages) {
            subscriptionsWithPersistedMessages.add(subscriptionId);
        } else {
            subscriptionsWithPersistedMessages.remove(subscriptionId);
        }
        sharedPrefs
                .edit()
                .putStringSet(
                        SUBSCRIPTIONS_WITH_PERSISTED_MESSAGES_KEY,
                        subscriptionsWithPersistedMessages)
                .apply();
    }

    /**
     * Whether some messages are persisted for |subscriptionIdPrefix| and should be replayed next
     * time Chrome is running. It should be cheaper to call than actually reading the stored
     * messages. Call this method to decide whether there is a need to read any persisted messages
     * for that subscription.
     *
     * @return whether some messages are persisted for that subscription.
     */
    public static Set<String> getSubscriptionIdsWithPersistedMessages(
            final String subscriptionIdPrefix) {
        SharedPreferences sharedPrefs = ContextUtils.getAppSharedPreferences();
        Set<String> subscriptionsWithPersistedMessages =
                new HashSet<>(
                        sharedPrefs.getStringSet(
                                SUBSCRIPTIONS_WITH_PERSISTED_MESSAGES_KEY, Collections.emptySet()));
        Set<String> subscriptionsWithPersistedMessagesWithPrefix = new HashSet<String>();
        for (String subscriptionWithPersistedMessages : subscriptionsWithPersistedMessages) {
            if (subscriptionWithPersistedMessages.startsWith(subscriptionIdPrefix)) {
                subscriptionsWithPersistedMessagesWithPrefix.add(subscriptionWithPersistedMessages);
            }
        }
        return subscriptionsWithPersistedMessagesWithPrefix;
    }

    /**
     * Given an appId and a senderId, this methods builds a unique identifier for a subscription.
     * Currently implementation concatenates both senderId and appId.
     *
     * @return The unique identifier for the subscription.
     */
    public static String buildSubscriptionUniqueId(final String appId, final String senderId) {
        return appId + senderId;
    }

    /** Stores the information about lazy subscriptions in SharedPreferences. */
    public static void storeLazinessInformation(final String subscriptionId, boolean isLazy) {
        boolean isAlreadyLazy = isSubscriptionLazy(subscriptionId);
        if (isAlreadyLazy == isLazy) {
            return;
        }
        if (isAlreadyLazy) {
            // Switching from lazy to unlazy.
            // Delete any queued messages.
            deletePersistedMessagesForSubscriptionId(subscriptionId);
        }
        Context context = ContextUtils.getApplicationContext();
        SharedPreferences sharedPrefs =
                context.getSharedPreferences(PREF_PACKAGE, Context.MODE_PRIVATE);
        Set<String> lazyIds =
                new HashSet<>(
                        sharedPrefs.getStringSet(FCM_LAZY_SUBSCRIPTIONS, Collections.emptySet()));
        if (isAlreadyLazy) {
            lazyIds.remove(subscriptionId);
        } else { // Switching from unlazy to lazy.
            lazyIds.add(subscriptionId);
        }
        sharedPrefs.edit().putStringSet(FCM_LAZY_SUBSCRIPTIONS, lazyIds).apply();
    }

    /** Returns whether the subscription with the |appId| and |senderId| is lazy. */
    public static boolean isSubscriptionLazy(final String subscriptionId) {
        try (StrictModeContext ignored = StrictModeContext.allowDiskReads()) {
            Context context = ContextUtils.getApplicationContext();
            SharedPreferences sharedPrefs =
                    context.getSharedPreferences(PREF_PACKAGE, Context.MODE_PRIVATE);
            Set<String> lazyIds =
                    new HashSet<>(
                            sharedPrefs.getStringSet(
                                    FCM_LAZY_SUBSCRIPTIONS, Collections.emptySet()));
            return lazyIds.contains(subscriptionId);
        }
    }

    /**
     * Returns the ids of all lazy subscriptions.
     * @return Set of subscriptions ids.
     */
    public static Set<String> getLazySubscriptionIds() {
        try (StrictModeContext ignored = StrictModeContext.allowDiskReads()) {
            Context context = ContextUtils.getApplicationContext();
            SharedPreferences sharedPrefs =
                    context.getSharedPreferences(PREF_PACKAGE, Context.MODE_PRIVATE);
            return new HashSet<>(
                    sharedPrefs.getStringSet(FCM_LAZY_SUBSCRIPTIONS, Collections.emptySet()));
        }
    }

    /**
     * Stores |message| on disk. Stored Messages for a subscription id will be
     * returned by readMessages(). Only the most recent |MESSAGES_QUEUE_SIZE|
     * messages with distinct collapse keys are kept.
     * @param subscriptionId id of the subscription.
     * @param message The message to be persisted.
     */
    public static void persistMessage(String subscriptionId, GCMMessage message) {
        // Messages are stored as a JSONArray in SharedPreferences. The key is
        // |subscriptionId|. The value is a string representing a JSONArray that
        // contains messages serialized as a JSONObject.

        // Load the persisted messages for this subscription.
        Context context = ContextUtils.getApplicationContext();
        SharedPreferences sharedPrefs =
                context.getSharedPreferences(PREF_PACKAGE, Context.MODE_PRIVATE);
        // Default is an empty queue if no messages are queued for this subscription.
        String queueString = sharedPrefs.getString(subscriptionId, "[]");
        try {
            JSONArray queueJSON = new JSONArray(queueString);
            if (message.getCollapseKey() != null) {
                queueJSON = filterMessageBasedOnCollapseKey(queueJSON, message.getCollapseKey());
            }

            // If the queue is full remove the oldest message.
            if (queueJSON.length() == MESSAGES_QUEUE_SIZE) {
                Log.w(
                        TAG,
                        "Dropping GCM Message due queue size limit. Sender id:"
                                + GCMMessage.peekSenderId(queueJSON.getJSONObject(0)));
                JSONArray newQueue = new JSONArray();
                // Copy all messages except the first one.
                for (int i = 1; i < MESSAGES_QUEUE_SIZE; i++) {
                    newQueue.put(queueJSON.get(i));
                }
                queueJSON = newQueue;
            }
            // Add the new message to the end.
            queueJSON.put(message.toJSON());
            sharedPrefs.edit().putString(subscriptionId, queueJSON.toString()).apply();
            storeHasPersistedMessagesForSubscription(
                    subscriptionId, /* hasPersistedMessages= */ true);
        } catch (JSONException e) {
            Log.e(
                    TAG,
                    "Error when parsing the persisted message queue for subscriber:"
                            + subscriptionId
                            + ":"
                            + e.getMessage());
        }
    }

    /**
     *  Reads messages stored using persistMessage() for |subscriptionId|. No
     *  more than |MESSAGES_QUEUE_SIZE| are returned.
     *  @param subscriptionId The subscription id of the stored messages.
     *  @return The messages stored. Returns an empty list in case of failure.
     */
    public static GCMMessage[] readMessages(String subscriptionId) {
        Context context = ContextUtils.getApplicationContext();
        SharedPreferences sharedPrefs =
                context.getSharedPreferences(PREF_PACKAGE, Context.MODE_PRIVATE);

        // Default is an empty queue if no messages are queued for this subscription.
        String queueString = sharedPrefs.getString(subscriptionId, "[]");
        try {
            JSONArray queueJSON = new JSONArray(queueString);
            List<GCMMessage> messages = new ArrayList<>();
            for (int i = 0; i < queueJSON.length(); i++) {
                try {
                    GCMMessage persistedMessage =
                            GCMMessage.createFromJSON(queueJSON.getJSONObject(i));
                    if (persistedMessage == null) {
                        Log.e(
                                TAG,
                                "Persisted GCM Message is invalid. Sender id:"
                                        + GCMMessage.peekSenderId(queueJSON.getJSONObject(i)));
                        continue;
                    }
                    messages.add(persistedMessage);
                } catch (JSONException e) {
                    Log.e(
                            TAG,
                            "Error when creating a GCMMessage from a JSONObject:" + e.getMessage());
                }
            }
            return messages.toArray(new GCMMessage[messages.size()]);
        } catch (JSONException e) {
            Log.e(
                    TAG,
                    "Error when parsing the persisted message queue for subscriber:"
                            + subscriptionId);
        }
        return new GCMMessage[0];
    }

    /** Deletes all persisted messages for the given subscription id. */
    public static void deletePersistedMessagesForSubscriptionId(String subscriptionId) {
        Context context = ContextUtils.getApplicationContext();
        SharedPreferences sharedPrefs =
                context.getSharedPreferences(PREF_PACKAGE, Context.MODE_PRIVATE);
        sharedPrefs.edit().remove(subscriptionId).apply();
        LazySubscriptionsManager.storeHasPersistedMessagesForSubscription(
                subscriptionId, /* hasPersistedMessages= */ false);
    }

    /**
     * Filters out any messages in |messagesJSON| with the given collpase key. It returns the
     * filtered list.
     */
    private static JSONArray filterMessageBasedOnCollapseKey(JSONArray messages, String collapseKey)
            throws JSONException {
        JSONArray filteredMessages = new JSONArray();
        for (int i = 0; i < messages.length(); i++) {
            JSONObject message = messages.getJSONObject(i);
            if (GCMMessage.peekCollapseKey(message).equals(collapseKey)) {
                Log.i(
                        TAG,
                        "Dropping GCM Message due to collapse key collision. Sender id:"
                                + GCMMessage.peekSenderId(message));
                continue;
            }
            filteredMessages.put(message);
        }
        return filteredMessages;
    }
}
