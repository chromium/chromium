// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.components.sync.notifier;

import android.accounts.Account;
import android.annotation.SuppressLint;
import android.content.SharedPreferences;
import android.util.Base64;

import androidx.annotation.Nullable;
import androidx.annotation.VisibleForTesting;

import com.google.ipc.invalidation.external.client.types.ObjectId;

import org.chromium.base.ApiCompatibilityUtils;
import org.chromium.base.ContextUtils;
import org.chromium.base.Log;

import java.util.Collection;
import java.util.Collections;
import java.util.HashSet;
import java.util.Set;

/**
 * Class to manage the preferences used by the invalidation client.
 * <p>
 * This class provides methods to read and write the preferences used by the invalidation client.
 * <p>
 * To read a preference, call the appropriate {@code get...} method.
 * <p>
 * To write a preference, first call {@link #edit} to obtain a {@link EditContext}. Then, make
 * one or more calls to a {@code set...} method, providing the same edit context to each call.
 * Finally, call {@link #commit(EditContext)} to save the changes to stable storage.
 *
 * @author dsmyers@google.com (Daniel Myers)
 */
public class InvalidationPreferences {
    /**
     * Wrapper around a {@link android.content.SharedPreferences.Editor} for the preferences.
     * Used to avoid exposing raw preference objects to users of this class.
     */
    @SuppressLint("CommitPrefEdits")
    public static class EditContext {
        private final SharedPreferences.Editor mEditor;

        EditContext() {
            mEditor = ContextUtils.getAppSharedPreferences().edit();
        }
    }

    /**
     * Internal class to wrap constants for preference keys.
     */
    @VisibleForTesting
    public static class PrefKeys {
        /**
         * Shared preference key to store the invalidation types that we want to register
         * for.
         */
        @VisibleForTesting
        public static final String SYNC_TANGO_TYPES = "sync_tango_types";

        /**
         * Shared preference key to store tango object ids for additional objects that we want to
         * register for.
         */
        @VisibleForTesting
        public static final String TANGO_OBJECT_IDS = "tango_object_ids";

        /** Shared preference key to store the name of the account in use. */
        @VisibleForTesting
        public static final String SYNC_ACCT_NAME = "sync_acct_name";

        /** Shared preference key to store the type of account in use. */
        static final String SYNC_ACCT_TYPE = "sync_acct_type";

        /** Shared preference key to store internal notification client library state. */
        static final String SYNC_TANGO_INTERNAL_STATE = "sync_tango_internal_state";
    }

    private static final String TAG = "InvalidationPrefs";

    // Only one commit call can be in progress at a time.
    private static final Object sCommitLock = new Object();

    /** Returns a new {@link EditContext} to modify the preferences managed by this class. */
    public EditContext edit() {
        return new EditContext();
    }

    /**
     * Applies the changes accumulated in {@code editContext}. Returns whether they were
     * successfully written.
     * <p>
     * NOTE: this method performs blocking I/O and must not be called from the UI thread.
     */
    public boolean commit(EditContext editContext) {
        synchronized (sCommitLock) {
            if (!editContext.mEditor.commit()) {
                Log.w(TAG, "Failed to commit invalidation preferences");
                return false;
            }
            return true;
        }
    }

    /** Returns the saved sync types, or {@code null} if none exist. */
    @Nullable
    public Set<String> getSavedSyncedTypes() {
        SharedPreferences preferences = ContextUtils.getAppSharedPreferences();
        Set<String> syncedTypes = preferences.getStringSet(PrefKeys.SYNC_TANGO_TYPES, null);
        // Wrap with unmodifiableSet to ensure it's never modified. See crbug.com/568369.
        return syncedTypes == null ? null : Collections.unmodifiableSet(syncedTypes);
    }

    /** Sets the saved sync types to {@code syncTypes} in {@code editContext}. */
    public void setSyncTypes(EditContext editContext, Collection<String> syncTypes) {
        if (syncTypes == null) throw new NullPointerException("syncTypes is null.");
        Set<String> selectedTypesSet = new HashSet<String>(syncTypes);
        editContext.mEditor.putStringSet(PrefKeys.SYNC_TANGO_TYPES, selectedTypesSet);
    }

    /** Returns the saved non-sync object ids, or {@code null} if none exist. */
    @Nullable
    public Set<ObjectId> getSavedObjectIds() {
        SharedPreferences preferences = ContextUtils.getAppSharedPreferences();
        Set<String> objectIdStrings = preferences.getStringSet(PrefKeys.TANGO_OBJECT_IDS, null);
        if (objectIdStrings == null) {
            return null;
        }
        Set<ObjectId> objectIds = new HashSet<ObjectId>(objectIdStrings.size());
        for (String objectIdString : objectIdStrings) {
            ObjectId objectId = getObjectId(objectIdString);
            if (objectId != null) {
                objectIds.add(objectId);
            }
        }
        return objectIds;
    }

    /** Sets the saved non-sync object ids */
    public void setObjectIds(EditContext editContext, Collection<ObjectId> objectIds) {
        if (objectIds == null) throw new NullPointerException("objectIds is null.");
        Set<String> objectIdStrings = new HashSet<String>(objectIds.size());
        for (ObjectId objectId : objectIds) {
            objectIdStrings.add(getObjectIdString(objectId));
        }
        editContext.mEditor.putStringSet(PrefKeys.TANGO_OBJECT_IDS, objectIdStrings);
    }

    /** Returns the saved account, or {@code null} if none exists. */
    @Nullable
    public Account getSavedSyncedAccount() {
        SharedPreferences preferences = ContextUtils.getAppSharedPreferences();
        String accountName = preferences.getString(PrefKeys.SYNC_ACCT_NAME, null);
        String accountType = preferences.getString(PrefKeys.SYNC_ACCT_TYPE, null);
        if (accountName == null || accountType == null) {
            return null;
        }
        return new Account(accountName, accountType);
    }

    /** Sets the saved account to {@code account} in {@code editContext}. */
    public void setAccount(EditContext editContext, Account account) {
        editContext.mEditor.putString(PrefKeys.SYNC_ACCT_NAME, account.name);
        editContext.mEditor.putString(PrefKeys.SYNC_ACCT_TYPE, account.type);
    }

    /** Returns the notification client internal state. */
    @Nullable
    public byte[] getInternalNotificationClientState() {
        SharedPreferences preferences = ContextUtils.getAppSharedPreferences();
        String base64State = preferences.getString(PrefKeys.SYNC_TANGO_INTERNAL_STATE, null);
        if (base64State == null) {
            return null;
        }
        try {
            return Base64.decode(base64State, Base64.DEFAULT);
        } catch (java.lang.IllegalArgumentException e) {
            return null;
        }
    }

    /** Sets the notification client internal state to {@code state}. */
    public void setInternalNotificationClientState(EditContext editContext, byte[] state) {
        editContext.mEditor.putString(
                PrefKeys.SYNC_TANGO_INTERNAL_STATE, Base64.encodeToString(state, Base64.DEFAULT));
    }

    /** Converts the given object id to a string for storage in preferences. */
    private String getObjectIdString(ObjectId objectId) {
        return objectId.getSource() + ":" + new String(objectId.getName());
    }

    /**
     * Converts the given object id string stored in preferences to an object id.
     * Returns null if the string does not represent a valid object id.
     */
    private ObjectId getObjectId(String objectIdString) {
        int separatorPos = objectIdString.indexOf(':');
        // Ensure that the separator is surrounded by at least one character on each side.
        if (separatorPos < 1 || separatorPos == objectIdString.length() - 1) {
            return null;
        }
        int objectSource;
        try {
            objectSource = Integer.parseInt(objectIdString.substring(0, separatorPos));
        } catch (NumberFormatException e) {
            return null;
        }
        byte[] objectName =
                ApiCompatibilityUtils.getBytesUtf8(objectIdString.substring(separatorPos + 1));
        return ObjectId.newInstance(objectSource, objectName);
    }
}
