// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.components.data_sharing;

import org.chromium.base.Callback;
import org.chromium.base.UserDataHost;

import java.util.List;

/**
 * DataSharingService is the core class for managing data sharing. It represents a native
 * DataSharingService object in Java.
 */
public interface DataSharingService {
    /** Result that contains group data and an outcome of the action that was requested. */
    public static class GroupDataOrFailureOutcome {
        /**
         * The group data requested.
         *
         * <p>Can be null if the action failed. Please check `actionFailure` for more info.
         */
        public final GroupData groupData;

        /** Result of the action, UNKNOWN if the action was successful. */
        public final @PeopleGroupActionFailure int actionFailure;

        GroupDataOrFailureOutcome(GroupData groupData, int actionFailure) {
            this.groupData = groupData;
            this.actionFailure = actionFailure;
        }
    }

    /** Result that contains a set of groups and an outcome of the action that was requested. */
    public static class GroupsDataSetOrFailureOutcome {
        /**
         * The list of groups requested.
         *
         * <p>The list if null if the request failed. Group IDs cannot be repeated in the list.
         */
        public final List<GroupData> groupDataSet;

        /** Result of the action */
        public final @PeopleGroupActionFailure int actionFailure;

        GroupsDataSetOrFailureOutcome(List<GroupData> groupDataSet, int actionFailure) {
            this.groupDataSet = groupDataSet;
            this.actionFailure = actionFailure;
        }
    }

    /** Observer to listen to the updates on any of the groups. */
    interface Observer {
        /** A group was updated where the current user continues to be a member of. */
        void onGroupChanged(GroupData groupData);

        /** The user either created a new group or has been invited to the existing one. */
        void onGroupAdded(GroupData groupData);

        /** Either group has been deleted or user has been removed from the group. */
        void onGroupRemoved(String groupId);
    }

    /**
     * Add an observer to the service.
     *
     * <p>An observer should not be added to the same list more than once.
     *
     * @param observer The observer to add.
     */
    void addObserver(Observer observer);

    /**
     * Remove an observer from the service, if it is in the list.
     *
     * @param observer The observer to remove.
     */
    void removeObserver(Observer observer);

    /**
     * Refresh and read all the group data the user is part of.
     *
     * <p>Refresh data if necessary. The result is ordered by group ID.
     *
     * @param callback On success passes to the `callback` a set of all groups known to the client.
     */
    void readAllGroups(Callback<GroupsDataSetOrFailureOutcome> callback);

    /**
     * Refresh and read the requested group data.
     *
     * <p>Refresh data if necessary.
     *
     * @param groupId The group ID to read data from.
     * @param callback The GroupData is returned by callback.
     */
    void readGroup(String groupId, Callback<GroupDataOrFailureOutcome> callback);

    /**
     * Attempt to create a new group.
     *
     * @param groupName The name of the group to be created.
     * @param callback Return a created group data on success.
     */
    void createGroup(String groupName, Callback<GroupDataOrFailureOutcome> callback);

    /**
     * Attempt to delete a group.
     *
     * @param groupID The group ID to delete.
     * @param callback The deletion result as PeopleGroupActionOutcome.
     */
    void deleteGroup(String groupId, Callback</*PeopleGroupActionOutcome*/ Integer> callback);

    /**
     * Attempt to invite a new user to the group.
     *
     * @param groupID The group ID to add to.
     * @param inviteeEmail The email of the member to add.
     * @param callback The invite result as PeopleGroupActionOutcome.
     */
    void inviteMember(
            String groupId,
            String inviteeEmail,
            Callback</*PeopleGroupActionOutcome*/ Integer> callback);

    /**
     * Attempts to remove a user from the group.
     *
     * @param groupID The group ID to remove from.
     * @param membeEmail The email of the member to remove.
     * @param callback The removal result as PeopleGroupActionOutcome.
     */
    void removeMember(
            String groupId,
            String memberEmail,
            Callback</*PeopleGroupActionOutcome*/ Integer> callback);

    /**
     * Whether the service is an empty implementation. This is here because the Chromium build
     * disables RTTI, and we need to be able to verify that we are using an empty service from the
     * Chrome embedder.
     *
     * @return Whether the service implementation is empty.
     */
    boolean isEmptyService();

    /** Returns the network loader for sending out network calls to backend services. */
    DataSharingNetworkLoader getNetworkLoader();

    /**
     * @return {@link UserDataHost} that manages {@link UserData} objects attached to.
     */
    UserDataHost getUserDataHost();
}
