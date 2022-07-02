// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.components.omnibox;

import android.text.TextUtils;
import android.util.SparseArray;

import androidx.annotation.IntDef;
import androidx.annotation.NonNull;
import androidx.annotation.Nullable;
import androidx.annotation.VisibleForTesting;
import androidx.core.util.ObjectsCompat;

import org.chromium.base.annotations.CalledByNative;
import org.chromium.base.annotations.MockedInTests;
import org.chromium.base.annotations.NativeMethods;

import java.lang.annotation.Retention;
import java.lang.annotation.RetentionPolicy;
import java.util.ArrayList;
import java.util.Collections;
import java.util.List;

/**
 * AutocompleteResult encompasses and manages autocomplete results.
 */
@MockedInTests
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

    @IntDef({VerificationPoint.INVALID, VerificationPoint.SELECT_MATCH,
            VerificationPoint.UPDATE_MATCH, VerificationPoint.DELETE_MATCH,
            VerificationPoint.GROUP_BY_SEARCH_VS_URL_BEFORE,
            VerificationPoint.GROUP_BY_SEARCH_VS_URL_AFTER})
    @Retention(RetentionPolicy.SOURCE)
    public @interface VerificationPoint {
        int INVALID = 0;
        int SELECT_MATCH = 1;
        int UPDATE_MATCH = 2;
        int DELETE_MATCH = 3;
        int GROUP_BY_SEARCH_VS_URL_BEFORE = 4;
        int GROUP_BY_SEARCH_VS_URL_AFTER = 5;
    }

    /** An empty, initialized AutocompleteResult object. */
    public static final AutocompleteResult EMPTY_RESULT =
            new AutocompleteResult(0, Collections.emptyList(), null);
    /** A special value indicating that action has no particular index associated. */
    public static final int NO_SUGGESTION_INDEX = -1;

    private final @NonNull SparseArray<GroupDetails> mGroupsDetails;
    private final @NonNull List<AutocompleteMatch> mSuggestions;
    private final boolean mIsFromCachedResult;
    private long mNativeAutocompleteResult;

    /**
     * Create AutocompleteResult object that is associated with an (optional) Native
     * AutocompleteResult object.
     *
     * @param nativeResult Opaque pointer to Native AutocompleteResult object (or 0 if this object
     *         is built from local cache)
     * @param suggestions List of AutocompleteMatch objects.
     * @param groupsDetails Additional information about the AutocompleteMatch groups.
     */
    @VisibleForTesting(otherwise = VisibleForTesting.PRIVATE)
    AutocompleteResult(long nativeResult, @Nullable List<AutocompleteMatch> suggestions,
            @Nullable SparseArray<GroupDetails> groupsDetails) {
        // Consider all locally constructed AutocompleteResult objects as coming from Cache.
        // These results do not have a native counterpart, meaning there's no corresponding C++
        // structure describing the same AutocompleteResult.
        // Note that the mNativeResult might change at any point during the lifecycle of this object
        // to reflect relocation or destruction of the native object, so we cache this information
        // separately.
        mIsFromCachedResult = nativeResult == 0;
        mNativeAutocompleteResult = nativeResult;
        mSuggestions = suggestions != null ? suggestions : new ArrayList<>();
        mGroupsDetails = groupsDetails != null ? groupsDetails : new SparseArray<>();
    }

    /**
     * Create AutocompleteResult object from cached information.
     *
     * Newly created AutocompleteResult object is not associated with any Native AutocompleteResult
     * counterpart.
     *
     * @param suggestions List of AutocompleteMatch objects.
     * @param groupsDetails Additional information about the AutocompleteMatch groups.
     * @return AutocompleteResult object encompassing supplied information.
     */
    public static AutocompleteResult fromCache(@Nullable List<AutocompleteMatch> suggestions,
            @Nullable SparseArray<GroupDetails> groupsDetails) {
        return new AutocompleteResult(0, suggestions, groupsDetails);
    }

    /**
     * Create AutocompleteResult object from native object.
     *
     * Newly created AutocompleteResult object is associated with its Native counterpart.
     *
     * @param nativeAutocompleteResult Corresponding Native object.
     * @param suggestions Array of encompassed, associated AutocompleteMatch objects.
     *         These suggestions must be exact same and in same order as the ones held by
     *         Native AutocompleteResult content.
     * @param groupIds An array of known group identifiers (used for matching group headers).
     * @param groupNames An array of group names for each of the identifiers. The length and
     *         the content of this array must match the length and IDs of the |groupIds|.
     * @param groupCollapsedStates An array of group default collapsed states. The length and
     *         the content of this array must match the length and IDs of the |groupIds|.
     * @return AutocompleteResult object encompassing supplied information.
     */
    @VisibleForTesting(otherwise = VisibleForTesting.PRIVATE)
    @CalledByNative
    static AutocompleteResult fromNative(long nativeAutocompleteResult,
            @NonNull AutocompleteMatch[] suggestions, @NonNull int[] groupIds,
            @NonNull String[] groupNames, @NonNull boolean[] groupCollapsedStates) {
        assert groupIds.length == groupNames.length;
        assert groupIds.length == groupCollapsedStates.length;

        SparseArray<GroupDetails> groupsDetails = new SparseArray<>(groupIds.length);
        for (int index = 0; index < groupIds.length; index++) {
            groupsDetails.put(groupIds[index],
                    new GroupDetails(groupNames[index], groupCollapsedStates[index]));
        }

        AutocompleteResult result =
                new AutocompleteResult(nativeAutocompleteResult, null, groupsDetails);
        result.updateMatches(suggestions);
        return result;
    }

    @CalledByNative
    private void updateMatches(@NonNull AutocompleteMatch[] suggestions) {
        mSuggestions.clear();
        Collections.addAll(mSuggestions, suggestions);
    }

    @VisibleForTesting(otherwise = VisibleForTesting.PRIVATE)
    @CalledByNative
    void notifyNativeDestroyed() {
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

    public boolean isFromCachedResult() {
        return mIsFromCachedResult;
    }

    /**
     * Verifies coherency of this AutocompleteResult object with its C++ counterpart.
     * Records histogram data reflecting the outcome.
     *
     * @param suggestionIndex The index of suggestion the code intends to operate on,
     *         or NO_SUGGESTION_INDEX if there is no specific suggestion.
     * @param origin Used to track the source of the mismatch, should it occur.
     * @return Whether Java and C++ AutocompleteResult objects are in sync.
     */
    public boolean verifyCoherency(int suggestionIndex, @VerificationPoint int origin) {
        // May happen with either test data, or AutocompleteResult built from the ZeroSuggestCache.
        // This is a valid case, despite not meeting coherency criteria. Do not record.
        if (mNativeAutocompleteResult == 0) return false;
        long nativeMatches[] = new long[mSuggestions.size()];
        for (int index = 0; index < mSuggestions.size(); index++) {
            nativeMatches[index] = mSuggestions.get(index).getNativeObjectRef();
        }
        return AutocompleteResultJni.get().verifyCoherency(
                mNativeAutocompleteResult, nativeMatches, suggestionIndex, origin);
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
            if (!verifyCoherency(
                        NO_SUGGESTION_INDEX, VerificationPoint.GROUP_BY_SEARCH_VS_URL_BEFORE)) {
                // This may trigger if the Native (C++) object got updated and we haven't had a
                // chance to reflect this update here. When this happens, do not rearrange the
                // order of suggestions and wait for a corresponding update.
                // Need to identify whether this issue is anything larger than just background
                // update.
                assert false : "Pre-group verification failed. Please report.";
                return;
            }
            AutocompleteResultJni.get().groupSuggestionsBySearchVsURL(
                    mNativeAutocompleteResult, firstIndex, lastIndex);
            // Verify that the Native AutocompleteResult update has been properly
            // reflected on the Java part.
            assert verifyCoherency(
                    NO_SUGGESTION_INDEX, VerificationPoint.GROUP_BY_SEARCH_VS_URL_AFTER)
                : "Post-group verification failed";
        }
    }

    @NativeMethods
    interface Natives {
        void groupSuggestionsBySearchVsURL(
                long nativeAutocompleteResult, int firstIndex, int lastIndex);
        boolean verifyCoherency(
                long nativeAutocompleteResult, long[] matches, long suggestionIndex, int origin);
    }
}
