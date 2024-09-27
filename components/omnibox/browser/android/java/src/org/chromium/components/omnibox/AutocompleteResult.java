// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.components.omnibox;

import androidx.annotation.IntDef;
import androidx.annotation.NonNull;
import androidx.annotation.Nullable;
import androidx.annotation.VisibleForTesting;

import com.google.protobuf.InvalidProtocolBufferException;

import org.jni_zero.CalledByNative;
import org.jni_zero.NativeMethods;

import org.chromium.build.annotations.MockedInTests;
import org.chromium.components.omnibox.GroupsProto.GroupsInfo;

import java.lang.annotation.Retention;
import java.lang.annotation.RetentionPolicy;
import java.util.ArrayList;
import java.util.Collections;
import java.util.List;

/** AutocompleteResult encompasses and manages autocomplete results. */
@MockedInTests
public class AutocompleteResult {
    @IntDef({
        VerificationPoint.INVALID,
        VerificationPoint.SELECT_MATCH,
        VerificationPoint.UPDATE_MATCH,
        VerificationPoint.DELETE_MATCH,
        VerificationPoint.GROUP_BY_SEARCH_VS_URL_BEFORE,
        VerificationPoint.GROUP_BY_SEARCH_VS_URL_AFTER,
        VerificationPoint.ON_TOUCH_MATCH,
        VerificationPoint.GET_MATCHING_TAB
    })
    @Retention(RetentionPolicy.SOURCE)
    // When updating this enum, please update corresponding enum in autocomplete_result_android.cc.
    public @interface VerificationPoint {
        int INVALID = 0;
        int SELECT_MATCH = 1;
        int UPDATE_MATCH = 2;
        int DELETE_MATCH = 3;
        int GROUP_BY_SEARCH_VS_URL_BEFORE = 4;
        int GROUP_BY_SEARCH_VS_URL_AFTER = 5;
        int ON_TOUCH_MATCH = 6;
        int GET_MATCHING_TAB = 7;
    }

    /** A special value indicating that action has no particular index associated. */
    public static final int NO_SUGGESTION_INDEX = -1;

    private final @NonNull GroupsInfo mGroupsInfo;
    private final @NonNull List<AutocompleteMatch> mSuggestions;
    private final boolean mIsFromCachedResult;
    private long mNativeAutocompleteResult;

    /**
     * Create AutocompleteResult object that is associated with an (optional) Native
     * AutocompleteResult object.
     *
     * @param nativeResult Opaque pointer to Native AutocompleteResult object (or 0 if this object
     *     is built from local cache)
     * @param suggestions List of AutocompleteMatch objects.
     * @param groupsInfo Additional information about the AutocompleteMatch groups.
     */
    @VisibleForTesting(otherwise = VisibleForTesting.PRIVATE)
    public AutocompleteResult(
            long nativeResult,
            @Nullable List<AutocompleteMatch> suggestions,
            @Nullable GroupsInfo groupsInfo) {
        // Consider all locally constructed AutocompleteResult objects as coming from Cache.
        // These results do not have a native counterpart, meaning there's no corresponding C++
        // structure describing the same AutocompleteResult.
        // Note that the mNativeResult might change at any point during the lifecycle of this object
        // to reflect relocation or destruction of the native object, so we cache this information
        // separately.
        mIsFromCachedResult = nativeResult == 0;
        mNativeAutocompleteResult = nativeResult;
        mSuggestions = suggestions != null ? suggestions : new ArrayList<>();
        mGroupsInfo = groupsInfo != null ? groupsInfo : GroupsInfo.newBuilder().build();
    }

    /**
     * Create AutocompleteResult object from cached information.
     *
     * <p>Newly created AutocompleteResult object is not associated with any Native
     * AutocompleteResult counterpart.
     *
     * @param suggestions List of AutocompleteMatch objects.
     * @param groupsInfo Additional information about the AutocompleteMatch groups.
     * @return AutocompleteResult object encompassing supplied information.
     */
    public static AutocompleteResult fromCache(
            @Nullable List<AutocompleteMatch> suggestions, @Nullable GroupsInfo groupsInfo) {
        return new AutocompleteResult(0, suggestions, groupsInfo);
    }

    /**
     * Create AutocompleteResult object from native object.
     *
     * <p>Newly created AutocompleteResult object is associated with its Native counterpart.
     *
     * @param nativeAutocompleteResult Corresponding Native object.
     * @param suggestions Array of encompassed, associated AutocompleteMatch objects. These
     *     suggestions must be exact same and in same order as the ones held by Native
     *     AutocompleteResult content.
     * @param groupIds An array of known group identifiers (used for matching group headers).
     * @param groupNames An array of group names for each of the identifiers. The length and the
     *     content of this array must match the length and IDs of the |groupIds|.
     * @param groupCollapsedStates An array of group default collapsed states. The length and the
     *     content of this array must match the length and IDs of the |groupIds|.
     * @return AutocompleteResult object encompassing supplied information.
     */
    @VisibleForTesting(otherwise = VisibleForTesting.PRIVATE)
    @CalledByNative
    static AutocompleteResult fromNative(
            long nativeAutocompleteResult,
            @NonNull AutocompleteMatch[] suggestions,
            @NonNull byte[] groupDefinitions) {
        GroupsInfo groupsInfo = null;

        try {
            groupsInfo = GroupsInfo.parseFrom(groupDefinitions);
        } catch (InvalidProtocolBufferException e) {
        }

        AutocompleteResult result =
                new AutocompleteResult(nativeAutocompleteResult, null, groupsInfo);
        result.updateMatches(suggestions);
        return result;
    }

    private void updateMatches(@NonNull AutocompleteMatch[] suggestions) {
        mSuggestions.clear();
        Collections.addAll(mSuggestions, suggestions);
    }

    @VisibleForTesting(otherwise = VisibleForTesting.PRIVATE)
    @CalledByNative
    void notifyNativeDestroyed() {
        mNativeAutocompleteResult = 0;
    }

    /** @return List of Omnibox Suggestions. */
    @NonNull
    public List<AutocompleteMatch> getSuggestionsList() {
        return mSuggestions;
    }

    /** @return GroupsInfo structure, describing everything that's known about Suggestion Groups. */
    @NonNull
    public GroupsInfo getGroupsInfo() {
        return mGroupsInfo;
    }

    public boolean isFromCachedResult() {
        return mIsFromCachedResult;
    }

    /**
     * Verifies coherency of this AutocompleteResult object with its C++ counterpart. Records
     * histogram data reflecting the outcome.
     *
     * @param suggestionIndex The index of suggestion the code intends to operate on, or
     *     NO_SUGGESTION_INDEX if there is no specific suggestion.
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
        return AutocompleteResultJni.get()
                .verifyCoherency(mNativeAutocompleteResult, nativeMatches, suggestionIndex, origin);
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
        return mGroupsInfo.equals(other.mGroupsInfo);
    }

    @Override
    public int hashCode() {
        return mGroupsInfo.hashCode() ^ mSuggestions.hashCode();
    }

    /**
     * This is a counterpart of native AutocompleteResult#default_match.
     *
     * @return The default match if it exists, or nullptr otherwise.
     */
    @Nullable
    public AutocompleteMatch getDefaultMatch() {
        if (mSuggestions.size() > 0 && mSuggestions.get(0).allowedToBeDefaultMatch()) {
            return mSuggestions.get(0);
        }

        return null;
    }

    @NativeMethods
    interface Natives {
        boolean verifyCoherency(
                long nativeAutocompleteResult, long[] matches, int suggestionIndex, int origin);
    }
}
