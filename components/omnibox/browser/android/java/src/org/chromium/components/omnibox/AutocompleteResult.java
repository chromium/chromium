// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.components.omnibox;

import android.text.TextUtils;
import android.util.SparseArray;

import androidx.annotation.NonNull;
import androidx.core.util.ObjectsCompat;

import org.chromium.base.annotations.CalledByNative;
import org.chromium.base.annotations.NativeMethods;

import java.util.ArrayList;
import java.util.Arrays;
import java.util.List;

/**
 * AutocompleteResult encompasses and manages autocomplete results.
 */
public class AutocompleteResult {
    /** Describes details of the Suggestions group. */
    public static class GroupDetails {
        // Title of the group, that will be shown to the user.
        public final String title;
        // Default/recommended group collapsed state.
        public final boolean collapsedByDefault;

        public GroupDetails(String title, boolean collapsedByDefault) {
            this.title = title;
            this.collapsedByDefault = collapsedByDefault;
        }

        @Override
        public int hashCode() {
            int hash = title != null ? title.hashCode() : 0;
            hash ^= (collapsedByDefault ? 0x3ff : 0);
            return hash;
        }

        @Override
        public boolean equals(Object otherObj) {
            if (otherObj == this) return true;
            if (!(otherObj instanceof GroupDetails)) return false;

            GroupDetails other = (GroupDetails) otherObj;
            return (collapsedByDefault == other.collapsedByDefault)
                    && TextUtils.equals(title, other.title);
        }
    };

    private final @NonNull SparseArray<GroupDetails> mGroupsDetails;
    private @NonNull List<AutocompleteMatch> mSuggestions;
    private long mNativeAutocompleteResult;

    public AutocompleteResult(
            List<AutocompleteMatch> suggestions, SparseArray<GroupDetails> groupsDetails) {
        mSuggestions = suggestions != null ? suggestions : new ArrayList<>();
        mGroupsDetails = groupsDetails != null ? groupsDetails : new SparseArray<>();
    }

    public AutocompleteResult(long nativeResult, List<AutocompleteMatch> suggestions,
            SparseArray<GroupDetails> groupsDetails) {
        this(suggestions, groupsDetails);
        mNativeAutocompleteResult = nativeResult;
    }

    @CalledByNative
    private static AutocompleteResult build(long nativeAutocompleteResult,
            @NonNull AutocompleteMatch[] suggestions, @NonNull int[] groupIds,
            @NonNull String[] groupNames, @NonNull boolean[] groupCollapsedStates) {
        assert groupIds.length == groupNames.length;
        assert groupIds.length == groupCollapsedStates.length;

        SparseArray<GroupDetails> groupsDetails = new SparseArray<>(groupIds.length);
        for (int index = 0; index < groupIds.length; index++) {
            groupsDetails.put(groupIds[index],
                    new GroupDetails(groupNames[index], groupCollapsedStates[index]));
        }

        AutocompleteResult result = new AutocompleteResult(
                nativeAutocompleteResult, Arrays.asList(suggestions), groupsDetails);
        return result;
    }

    @CalledByNative
    private void updateMatches(@NonNull AutocompleteMatch[] suggestions) {
        mSuggestions = Arrays.asList(suggestions);
    }

    @CalledByNative
    private void destroy() {
        mNativeAutocompleteResult = 0;
    }

    /**
     * @return List of Omnibox Suggestions.
     */
    @NonNull
    public List<AutocompleteMatch> getSuggestionsList() {
        return mSuggestions;
    }

    /**
     * @return Map of Group ID to GroupDetails objects.
     */
    @NonNull
    public SparseArray<GroupDetails> getGroupsDetails() {
        return mGroupsDetails;
    }

    /**
     * Verifies coherency of this AutocompleteResult object with its C++ counterpart.
     * Records histogram data reflecting the outcome.
     * @return Whether Java and C++ AutocompleteResult objects are in sync.
     */
    public boolean verifyCoherency() {
        // May happen with either test data, or AutocompleteResult built from the ZeroSuggestCache.
        // This is a valid case, despite not meeting coherency criteria. Do not record.
        if (mNativeAutocompleteResult == 0) return false;
        long nativeMatches[] = new long[mSuggestions.size()];
        for (int index = 0; index < mSuggestions.size(); index++) {
            nativeMatches[index] = mSuggestions.get(index).getNativeObjectRef();
        }
        return AutocompleteResultJni.get().verifyCoherency(
                mNativeAutocompleteResult, nativeMatches);
    }

    /** Returns a reference to Native AutocompleteResult object. */
    public long getNativeObjectRef() {
        return mNativeAutocompleteResult;
    }

    @Override
    public boolean equals(Object otherObj) {
        if (otherObj == this) return true;
        if (!(otherObj instanceof AutocompleteResult)) return false;

        AutocompleteResult other = (AutocompleteResult) otherObj;
        if (!mSuggestions.equals(other.mSuggestions)) return false;

        final SparseArray<GroupDetails> otherGroupsDetails = other.mGroupsDetails;
        if (mGroupsDetails.size() != otherGroupsDetails.size()) return false;
        for (int index = 0; index < mGroupsDetails.size(); index++) {
            if (mGroupsDetails.keyAt(index) != otherGroupsDetails.keyAt(index)) return false;
            if (!ObjectsCompat.equals(
                        mGroupsDetails.valueAt(index), otherGroupsDetails.valueAt(index))) {
                return false;
            }
        }

        return true;
    }

    @Override
    public int hashCode() {
        int baseHash = 0;
        for (int index = 0; index < mGroupsDetails.size(); index++) {
            baseHash += mGroupsDetails.keyAt(index);
            baseHash ^= mGroupsDetails.valueAt(index).hashCode();
            baseHash = Integer.rotateLeft(baseHash, 10);
        }
        return baseHash ^ mSuggestions.hashCode();
    }

    /**
     * Group native suggestions in specified range by Search vs URL.
     *
     * @param firstIndex Index of the first suggestion for grouping.
     * @param lastIndex Index of the last suggestion for grouping.
     */
    public void groupSuggestionsBySearchVsURL(int firstIndex, int lastIndex) {
        if (mNativeAutocompleteResult != 0) {
            assert verifyCoherency() : "Pre-group verification failed";
            AutocompleteResultJni.get().groupSuggestionsBySearchVsURL(
                    mNativeAutocompleteResult, firstIndex, lastIndex);
            // Verify that the Native AutocompleteResult update has been properly
            // reflected on the Java part.
            assert verifyCoherency() : "Post-group verification failed";
        }
    }

    @NativeMethods
    interface Natives {
        void groupSuggestionsBySearchVsURL(
                long nativeAutocompleteResult, int firstIndex, int lastIndex);
        boolean verifyCoherency(long nativeAutocompleteResult, long[] matches);
    }
}
