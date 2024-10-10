// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.components.tab_group_sync.messaging;

import androidx.annotation.NonNull;

import org.chromium.base.Callback;
import org.chromium.components.tab_group_sync.messaging.EitherId.EitherGroupId;
import org.chromium.components.tab_group_sync.messaging.EitherId.EitherTabId;

import java.util.List;
import java.util.Optional;

/**
 * Java shim for a MessagingBackendService. See
 * //components/saved_tab_groups/messaging/messaging_backend_service.h. Used for accessing and
 * listening to messages related to collaboration and groups that need to be reflected in the UI.
 */
public interface MessagingBackendService {
    /** An observer to be notified of persistent indicators that need to be shown in the UI. */
    interface PersistentMessageObserver {
        /**
         * Invoked once when the service is initialized. This is invoked only once and is
         * immediately invoked if the service was initialized before the observer was added. The
         * initialization state can also be inspected using {@link
         * MessagingBackendService#isInitialized()}.
         */
        default void onMessagingBackendServiceInitialized() {}

        /** Invoked when the frontend needs to display a specific persistent message. */
        void displayPersistentMessage(PersistentMessage message);

        /** Invoked when the frontend needs to hide a specific persistent message. */
        void hidePersistentMessage(PersistentMessage message);
    }

    /**
     * A delegate for showing instant (one-off) messages for the current platform. This needs to be
     * provided to the {@link MessagingBackendService} through {@link #SetInstantMessageDelegate}.
     */
    interface InstantMessageDelegate {
        /**
         * Invoked when the frontend needs to display an instant message. When a decision has been
         * made whether it can be displayed or not, invoke `successCallback` with `true` if it was
         * displayed, and `false` otherwise. This enables the backend to either: * Success: Clear
         * the message from internal storage. * Failure: Prepare the message to be redelivered at a
         * later time.
         *
         * <p>Note on memory safety: The successCallback is backed by an object in C++, and must NOT
         * be given to the garbage collector without invoking it first.
         */
        void displayInstantaneousMessage(InstantMessage message, Callback<Boolean> successCallback);
    }

    /** Sets the delegate for instant (one-off) messages. */
    void setInstantMessageDelegate(InstantMessageDelegate delegate);

    /** Adds an observer for being notified of {@link PersistentMessage} changes. */
    void addPersistentMessageObserver(PersistentMessageObserver observer);

    /** Removes an observer for being notified of {@link PersistentMessage} changes. */
    void removePersistentMessageObserver(PersistentMessageObserver observer);

    /** Returns whether the service has been initialized. */
    boolean isInitialized();

    /**
     * Queries for all {@link PersistentMessage}s associated with a tab ID.
     *
     * <p>Will return an empty result if the service has not yet been initialized. Use {@link
     * #isInitialized()} to check initialization state, or listen for broadcasts of {@link
     * PersistentMessageObserver#onMessagingBackendServiceInitialized}.
     *
     * @param tabId The ID of the tab to scope messages to.
     * @param type The type of message to query to. Pass Optional.empty() to return all message
     *     types.
     */
    @NonNull
    List<PersistentMessage> getMessagesForTab(
            EitherTabId tabId, Optional</* @PersistentNotificationType */ Integer> type);

    /**
     * Queries for all {@link PersistentMessage}s associated with a group ID. Will return an empty
     * result if the service has not yet been initialized.
     *
     * <p>Will return an empty result if the service has not yet been initialized. Use {@link
     * #isInitialized()} to check initialization state, or listen for broadcasts of {@link
     * PersistentMessageObserver#onMessagingBackendServiceInitialized}.
     *
     * @param groupId The ID of the group to scope messages to.
     * @param type The message type to query for. Pass Optional.empty() to return all message types.
     */
    @NonNull
    List<PersistentMessage> getMessagesForGroup(
            EitherGroupId groupId, Optional</* @PersistentNotificationType */ Integer> type);

    /**
     * Queries for all {@link PersistentMessage}s.
     *
     * <p>Will return an empty result if the service has not yet been initialized. Use {@link
     * #isInitialized()} to check initialization state, or listen for broadcasts of {@link
     * PersistentMessageObserver#onMessagingBackendServiceInitialized}.
     *
     * @param type The message type to query for. Pass Optional.empty() to return all message types.
     */
    @NonNull
    List<PersistentMessage> getMessages(Optional</* @PersistentNotificationType */ Integer> type);

    /**
     * Queries for all {@link ActivityLogItem}s.
     *
     * <p>Will return an empty result if the service has not yet been initialized. Use {@link
     * #isInitialized()} to check initialization state.
     *
     * @param params The query params (e.g. collaboration ID).
     */
    @NonNull
    List<ActivityLogItem> getActivityLog(ActivityLogQueryParams params);
}
