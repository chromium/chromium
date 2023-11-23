// Copyright 2014 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.components.omnibox;

import android.text.TextUtils;

import androidx.annotation.NonNull;
import androidx.annotation.Nullable;
import androidx.annotation.VisibleForTesting;
import androidx.collection.ArraySet;
import androidx.core.util.ObjectsCompat;

import org.jni_zero.CalledByNative;
import org.jni_zero.NativeMethods;

import org.chromium.chrome.browser.omnibox.MatchClassificationStyle;
import org.chromium.components.omnibox.GroupsProto.GroupId;
import org.chromium.components.omnibox.action.OmniboxAction;
import org.chromium.components.query_tiles.QueryTile;
import org.chromium.url.GURL;

import java.util.ArrayList;
import java.util.Arrays;
import java.util.Collections;
import java.util.List;
import java.util.Set;

/** Container class with information about each omnibox suggestion item. */
public class AutocompleteMatch {
    public static final int INVALID_GROUP = GroupId.GROUP_INVALID_VALUE;
    public static final int INVALID_TYPE = -1;

    /** Specifies an individual tile for TILE_NAVSUGGEST suggestions. */
    public static class SuggestTile {
        /** Title of the website the tile points to. */
        public final String title;

        /** URL of the website the tile points to. */
        public final GURL url;

        /** Whether the tile is a Search tile. */
        public final boolean isSearch;

        public SuggestTile(String title, GURL url, boolean isSearch) {
            this.title = title;
            this.url = url;
            this.isSearch = isSearch;
        }
    }

    /**
     * Specifies the style of portions of the suggestion text.
     *
     * <p>ACMatchClassification (as defined in C++) further describes the fields and usage.
     */
    public static class MatchClassification {
        /** The offset into the text where this classification begins. */
        public final int offset;

        /**
         * A bitfield that determines the style of this classification.
         *
         * @see MatchClassificationStyle
         */
        public final int style;

        public MatchClassification(int offset, int style) {
            this.offset = offset;
            this.style = style;
        }

        @Override
        public boolean equals(Object obj) {
            if (!(obj instanceof MatchClassification)) return false;
            MatchClassification other = (MatchClassification) obj;
            return offset == other.offset && style == other.style;
        }
    }

    private final int mType;
    private final @NonNull Set<Integer> mSubtypes;
    private final boolean mIsSearchType;
    private String mDisplayText;
    private final List<MatchClassification> mDisplayTextClassifications;
    private String mDescription;
    private List<MatchClassification> mDescriptionClassifications;
    private SuggestionAnswer mAnswer;
    private final String mFillIntoEdit;
    private GURL mUrl;
    private final GURL mImageUrl;
    private final String mImageDominantColor;
    private final int mRelevance;
    private final int mTransition;
    private final boolean mIsDeletable;
    private String mPostContentType;
    private byte[] mPostData;
    private final int mGroupId;
    private final List<QueryTile> mQueryTiles;
    private byte[] mClipboardImageData;
    private boolean mHasTabMatch;
    private final @Nullable List<SuggestTile> mSuggestTiles;
    private long mNativeMatch;
    private final @NonNull List<OmniboxAction> mActions;

    public AutocompleteMatch(
            int nativeType,
            Set<Integer> subtypes,
            boolean isSearchType,
            int relevance,
            int transition,
            String displayText,
            List<MatchClassification> displayTextClassifications,
            String description,
            List<MatchClassification> descriptionClassifications,
            SuggestionAnswer answer,
            String fillIntoEdit,
            GURL url,
            GURL imageUrl,
            String imageDominantColor,
            boolean isDeletable,
            String postContentType,
            byte[] postData,
            int groupId,
            List<QueryTile> queryTiles,
            byte[] clipboardImageData,
            boolean hasTabMatch,
            List<SuggestTile> suggestTiles,
            @Nullable List<OmniboxAction> actions) {
        if (subtypes == null) {
            subtypes = Collections.emptySet();
        }
        mType = nativeType;
        mSubtypes = subtypes;
        mIsSearchType = isSearchType;
        mRelevance = relevance;
        mTransition = transition;
        mDisplayText = displayText;
        mDisplayTextClassifications = displayTextClassifications;
        mDescription = description;
        mDescriptionClassifications = descriptionClassifications;
        mAnswer = answer;
        mFillIntoEdit = TextUtils.isEmpty(fillIntoEdit) ? displayText : fillIntoEdit;
        assert url != null;
        mUrl = url;
        assert imageUrl != null;
        mImageUrl = imageUrl;
        mImageDominantColor = imageDominantColor;
        mIsDeletable = isDeletable;
        mPostContentType = postContentType;
        mPostData = postData;
        mGroupId = groupId;
        mQueryTiles = queryTiles;
        mClipboardImageData = clipboardImageData;
        mHasTabMatch = hasTabMatch;
        mSuggestTiles = suggestTiles;
        mActions = actions != null ? actions : Arrays.asList();
    }

    @CalledByNative
    private static AutocompleteMatch build(
            long nativeObject,
            int nativeType,
            int[] nativeSubtypes,
            boolean isSearchType,
            int relevance,
            int transition,
            String contents,
            int[] contentClassificationOffsets,
            int[] contentClassificationStyles,
            String description,
            int[] descriptionClassificationOffsets,
            int[] descriptionClassificationStyles,
            SuggestionAnswer answer,
            String fillIntoEdit,
            GURL url,
            GURL imageUrl,
            String imageDominantColor,
            boolean isDeletable,
            String postContentType,
            byte[] postData,
            int groupId,
            List<QueryTile> tiles,
            byte[] clipboardImageData,
            boolean hasTabMatch,
            String[] suggestTileTitles,
            GURL[] suggestTileUrls,
            int[] suggestTileTypes,
            @Nullable OmniboxAction[] actions) {
        assert contentClassificationOffsets.length == contentClassificationStyles.length;
        List<MatchClassification> contentClassifications = new ArrayList<>();
        for (int i = 0; i < contentClassificationOffsets.length; i++) {
            contentClassifications.add(
                    new MatchClassification(
                            contentClassificationOffsets[i], contentClassificationStyles[i]));
        }

        assert suggestTileUrls.length == suggestTileTitles.length;
        assert suggestTileTypes.length == suggestTileTitles.length;
        List<SuggestTile> suggestTiles = new ArrayList<>();
        for (int i = 0; i < suggestTileTitles.length; i++) {
            suggestTiles.add(
                    new SuggestTile(
                            suggestTileTitles[i], suggestTileUrls[i], suggestTileTypes[i] != 0));
        }

        Set<Integer> subtypes = new ArraySet(nativeSubtypes.length);
        for (int i = 0; i < nativeSubtypes.length; i++) {
            subtypes.add(nativeSubtypes[i]);
        }

        AutocompleteMatch match =
                new AutocompleteMatch(
                        nativeType,
                        subtypes,
                        isSearchType,
                        relevance,
                        transition,
                        contents,
                        contentClassifications,
                        description,
                        new ArrayList<>(),
                        answer,
                        fillIntoEdit,
                        url,
                        imageUrl,
                        imageDominantColor,
                        isDeletable,
                        postContentType,
                        postData,
                        groupId,
                        tiles,
                        clipboardImageData,
                        hasTabMatch,
                        suggestTiles,
                        actions == null ? null : Arrays.asList(actions));
        match.updateNativeObjectRef(nativeObject);
        match.setDescription(
                description, descriptionClassificationOffsets, descriptionClassificationStyles);
        return match;
    }

    @CalledByNative
    @VisibleForTesting
    public void updateNativeObjectRef(long nativeMatch) {
        assert nativeMatch != 0 : "Invalid native object.";
        mNativeMatch = nativeMatch;
    }

    /** Returns a reference to Native AutocompleteMatch object. */
    public long getNativeObjectRef() {
        return mNativeMatch;
    }

    /**
     * Update the suggestion with content retrieved from clilpboard.
     *
     * @param contents The main text content for the suggestion.
     * @param url The URL associated with the suggestion.
     * @param postContentType Type of post content data.
     * @param postData Post content data.
     * @param clipboardImageData Clipboard image data content (if any).
     */
    @CalledByNative
    private void updateClipboardContent(
            String contents,
            GURL url,
            @Nullable String postContentType,
            @Nullable byte[] postData,
            @Nullable byte[] clipboardImageData) {
        mDisplayText = contents;
        mUrl = url;
        mPostContentType = postContentType;
        mPostData = postData;
        mClipboardImageData = clipboardImageData;
    }

    @CalledByNative
    private void destroy() {
        mNativeMatch = 0;
    }

    @CalledByNative
    private void setDestinationUrl(GURL url) {
        mUrl = url;
    }

    @CalledByNative
    private void setAnswer(SuggestionAnswer answer) {
        mAnswer = answer;
    }

    @CalledByNative
    private void setDescription(
            String description,
            int[] descriptionClassificationOffsets,
            int[] descriptionClassificationStyles) {
        assert descriptionClassificationOffsets.length == descriptionClassificationStyles.length;
        mDescription = description;
        mDescriptionClassifications.clear();
        for (int i = 0; i < descriptionClassificationOffsets.length; i++) {
            mDescriptionClassifications.add(
                    new MatchClassification(
                            descriptionClassificationOffsets[i],
                            descriptionClassificationStyles[i]));
        }
    }

    @CalledByNative
    private void updateMatchingTab(boolean hasTabMatch) {
        mHasTabMatch = hasTabMatch;
    }

    public @OmniboxSuggestionType int getType() {
        return mType;
    }

    public int getTransition() {
        return mTransition;
    }

    public String getDisplayText() {
        return mDisplayText;
    }

    public List<MatchClassification> getDisplayTextClassifications() {
        return mDisplayTextClassifications;
    }

    public String getDescription() {
        return mDescription;
    }

    public List<MatchClassification> getDescriptionClassifications() {
        return mDescriptionClassifications;
    }

    public SuggestionAnswer getAnswer() {
        return mAnswer;
    }

    public boolean hasAnswer() {
        return mAnswer != null;
    }

    public String getFillIntoEdit() {
        return mFillIntoEdit;
    }

    public GURL getUrl() {
        return mUrl;
    }

    public GURL getImageUrl() {
        return mImageUrl;
    }

    @Nullable
    public String getImageDominantColor() {
        return mImageDominantColor;
    }

    /** @return Whether the suggestion is a search suggestion. */
    public boolean isSearchSuggestion() {
        return mIsSearchType;
    }

    public boolean isDeletable() {
        return mIsDeletable;
    }

    public String getPostContentType() {
        return mPostContentType;
    }

    public List<QueryTile> getQueryTiles() {
        return mQueryTiles;
    }

    public byte[] getPostData() {
        return mPostData;
    }

    public boolean hasTabMatch() {
        return mHasTabMatch;
    }

    @NonNull
    public List<OmniboxAction> getActions() {
        return mActions;
    }

    /**
     * @return The image data for the image clipbaord suggestion. This data has already been
     *     validated in C++ and is safe to use in the browser process.
     */
    @Nullable
    public byte[] getClipboardImageData() {
        return mClipboardImageData;
    }

    /** @return The relevance score of this suggestion. */
    public int getRelevance() {
        return mRelevance;
    }

    /** @return Set of suggestion subtypes. */
    public @NonNull Set<Integer> getSubtypes() {
        return mSubtypes;
    }

    @Override
    public int hashCode() {
        final int displayTextHash = mDisplayText != null ? mDisplayText.hashCode() : 0;
        final int fillIntoEditHash = mFillIntoEdit != null ? mFillIntoEdit.hashCode() : 0;
        int hash =
                37 * mType
                        + 2017 * displayTextHash
                        + 1901 * fillIntoEditHash
                        + (mIsDeletable ? 1 : 0);
        if (mAnswer != null) hash = hash + mAnswer.hashCode();
        return hash;
    }

    @Override
    public boolean equals(Object obj) {
        if (!(obj instanceof AutocompleteMatch)) {
            return false;
        }

        AutocompleteMatch suggestion = (AutocompleteMatch) obj;
        return mType == suggestion.mType
                && mNativeMatch == suggestion.mNativeMatch
                && ObjectsCompat.equals(mSubtypes, suggestion.mSubtypes)
                && TextUtils.equals(mFillIntoEdit, suggestion.mFillIntoEdit)
                && TextUtils.equals(mDisplayText, suggestion.mDisplayText)
                && ObjectsCompat.equals(
                        mDisplayTextClassifications, suggestion.mDisplayTextClassifications)
                && TextUtils.equals(mDescription, suggestion.mDescription)
                && ObjectsCompat.equals(
                        mDescriptionClassifications, suggestion.mDescriptionClassifications)
                && mIsDeletable == suggestion.mIsDeletable
                && mRelevance == suggestion.mRelevance
                && ObjectsCompat.equals(mAnswer, suggestion.mAnswer)
                && TextUtils.equals(mPostContentType, suggestion.mPostContentType)
                && Arrays.equals(mPostData, suggestion.mPostData)
                && mGroupId == suggestion.mGroupId
                && ObjectsCompat.equals(mQueryTiles, suggestion.mQueryTiles);
    }

    /**
     * @return ID of the group this suggestion is associated with, or null, if the suggestion is not
     *     associated with any group, or INVALID_GROUP if suggestion is not associated with any
     *     group.
     */
    public int getGroupId() {
        return mGroupId;
    }

    /** @return List of tiles for TILE_NAVSUGGEST suggestion. */
    public @Nullable List<SuggestTile> getSuggestTiles() {
        return mSuggestTiles;
    }

    /**
     * Retrieve the clipboard information and update this instance of AutocompleteMatch. Will
     * terminate immediately if the native counterpart of the AutocompleteMatch object does not
     * exist. The callback is guaranteed to be executed at all times.
     *
     * @param callback The callback to run when update completes.
     */
    public void updateWithClipboardContent(Runnable callback) {
        if (mNativeMatch == 0) {
            callback.run();
            return;
        }

        AutocompleteMatchJni.get().updateWithClipboardContent(mNativeMatch, callback);
    }

    @Override
    public String toString() {
        List<String> pieces =
                Arrays.asList(
                        "mType=" + mType,
                        "mSubtypes=" + mSubtypes.toString(),
                        "mIsSearchType=" + mIsSearchType,
                        "mDisplayText=" + mDisplayText,
                        "mDescription=" + mDescription,
                        "mFillIntoEdit=" + mFillIntoEdit,
                        "mUrl=" + mUrl,
                        "mImageUrl=" + mImageUrl,
                        "mImageDominatColor=" + mImageDominantColor,
                        "mRelevance=" + mRelevance,
                        "mTransition=" + mTransition,
                        "mIsDeletable=" + mIsDeletable,
                        "mPostContentType=" + mPostContentType,
                        "mPostData=" + Arrays.toString(mPostData),
                        "mGroupId=" + mGroupId,
                        "mDisplayTextClassifications=" + mDisplayTextClassifications,
                        "mDescriptionClassifications=" + mDescriptionClassifications,
                        "mAnswer=" + mAnswer);
        return pieces.toString();
    }

    @NativeMethods
    interface Natives {
        void updateWithClipboardContent(long nativeAutocompleteMatch, Runnable callback);
    }
}
