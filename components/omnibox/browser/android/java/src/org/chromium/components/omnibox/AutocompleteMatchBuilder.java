// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.components.omnibox;

import androidx.annotation.NonNull;
import androidx.collection.ArraySet;

import org.chromium.chrome.browser.omnibox.MatchClassificationStyle;
import org.chromium.components.omnibox.AnswerTypeProto.AnswerType;
import org.chromium.components.omnibox.action.OmniboxAction;
import org.chromium.url.GURL;
import org.chromium.url.JUnitTestGURLs;

import java.util.ArrayList;
import java.util.List;
import java.util.Set;

/**
 * Utility class for all omnibox suggestions related tests that aids constructing of Omnibox
 * Suggestions.
 */
public class AutocompleteMatchBuilder {
    // Fields below directly represent fields used in AutocompleteMatch.java.
    private @OmniboxSuggestionType int mType;
    private Set<Integer> mSubtypes;
    private boolean mIsSearchType;
    private String mDisplayText;
    private List<AutocompleteMatch.MatchClassification> mDisplayTextClassifications;
    private String mDescription;
    private List<AutocompleteMatch.MatchClassification> mDescriptionClassifications;
    private SuggestionAnswer mAnswer;
    private byte[] mSerializedAnswerTemplate;
    private AnswerType mAnswerType;
    private String mFillIntoEdit;
    private GURL mUrl;
    private GURL mImageUrl;
    private String mImageDominantColor;
    private int mRelevance;
    private int mTransition;
    private boolean mIsDeletable;
    private String mPostContentType;
    private byte[] mPostData;
    private int mGroupId;
    private byte[] mClipboardImageData;
    private boolean mHasTabMatch;
    private List<OmniboxAction> mActions;
    private boolean mAllowedToBeDefaultMatch;
    private String mInlineAutocompletion;
    private String mAdditionalText;

    /**
     * Create a suggestion builder for a search suggestion.
     *
     * @return Omnibox suggestion builder that can be further refined by the user.
     */
    public static AutocompleteMatchBuilder searchWithType(@OmniboxSuggestionType int type) {
        return new AutocompleteMatchBuilder(type)
                .setIsSearch(true)
                .setDisplayText("Placeholder Suggestion")
                .setDescription("Placeholder Description")
                .setAnswerType(AnswerType.ANSWER_TYPE_UNSPECIFIED)
                .setUrl(JUnitTestGURLs.SEARCH_URL);
    }

    public AutocompleteMatchBuilder(@OmniboxSuggestionType int type) {
        reset();
        mType = type;
    }

    public AutocompleteMatchBuilder() {
        this(AutocompleteMatch.INVALID_TYPE);
    }

    /** Reset the Builder to its default state. */
    public void reset() {
        mType = AutocompleteMatch.INVALID_TYPE;
        mSubtypes = new ArraySet<>();
        mIsSearchType = false;
        mDisplayText = null;
        mDisplayTextClassifications = new ArrayList<>();
        mDescription = null;
        mDescriptionClassifications = new ArrayList<>();
        mAnswer = null;
        mSerializedAnswerTemplate = null;
        mAnswerType = AnswerType.ANSWER_TYPE_UNSPECIFIED;
        mFillIntoEdit = null;
        mUrl = GURL.emptyGURL();
        mImageUrl = GURL.emptyGURL();
        mImageDominantColor = null;
        mRelevance = 0;
        mTransition = 0;
        mIsDeletable = false;
        mPostContentType = null;
        mPostData = null;
        mGroupId = AutocompleteMatch.INVALID_GROUP;
        mClipboardImageData = null;
        mHasTabMatch = false;
        mActions = null;
        mAllowedToBeDefaultMatch = false;
        mInlineAutocompletion = null;
        mAdditionalText = null;

        mDisplayTextClassifications.add(
                new AutocompleteMatch.MatchClassification(0, MatchClassificationStyle.NONE));
        mDescriptionClassifications.add(
                new AutocompleteMatch.MatchClassification(0, MatchClassificationStyle.NONE));
    }

    /**
     * Construct AutocompleteMatch from user set parameters. Default/fallback values for not
     * explicitly initialized fields are supplied by the builder.
     *
     * @return New AutocompleteMatch.
     */
    public AutocompleteMatch build() {
        return new AutocompleteMatch(
                mType,
                mSubtypes,
                mIsSearchType,
                mRelevance,
                mTransition,
                mDisplayText,
                mDisplayTextClassifications,
                mDescription,
                mDescriptionClassifications,
                mAnswer,
                mSerializedAnswerTemplate,
                mAnswerType.getNumber(),
                mFillIntoEdit,
                mUrl,
                mImageUrl,
                mImageDominantColor,
                mIsDeletable,
                mPostContentType,
                mPostData,
                mGroupId,
                mClipboardImageData,
                mHasTabMatch,
                mActions,
                mAllowedToBeDefaultMatch,
                mInlineAutocompletion,
                mAdditionalText);
    }

    /**
     * @param text Display text to be used with the built suggestion.
     * @return Omnibox suggestion builder.
     */
    public AutocompleteMatchBuilder setDisplayText(String text) {
        mDisplayText = text;
        return this;
    }

    /**
     * @param text Description text to be used with the built suggestion.
     * @return Omnibox suggestion builder.
     */
    public AutocompleteMatchBuilder setDescription(String text) {
        mDescription = text;
        return this;
    }

    /**
     * @param text The text to replace the Omnibox content with.
     * @return Omnibox suggestion builder.
     */
    public AutocompleteMatchBuilder setFillIntoEdit(String text) {
        mFillIntoEdit = text;
        return this;
    }

    /**
     * @param id Group Id for newly built suggestion.
     * @return Omnibox suggestion builder.
     */
    public AutocompleteMatchBuilder setGroupId(int id) {
        mGroupId = id;
        return this;
    }

    /**
     * @param type Post content type to set for this suggestion.
     * @return Omnibox suggestion builder.
     */
    public AutocompleteMatchBuilder setPostContentType(String type) {
        mPostContentType = type;
        return this;
    }

    /**
     * @param data Post data to set for this suggestion.
     * @return Omnibox suggestion builder.
     */
    public AutocompleteMatchBuilder setPostData(byte[] data) {
        mPostData = data;
        return this;
    }

    /**
     * @param url URL for the built suggestion.
     * @return Omnibox suggestion builder.
     */
    public AutocompleteMatchBuilder setUrl(GURL url) {
        mUrl = url;
        return this;
    }

    /**
     * @param url Image URL for the built suggestion.
     * @return Omnibox suggestion builder.
     */
    public AutocompleteMatchBuilder setImageUrl(GURL url) {
        mImageUrl = url;
        return this;
    }

    /**
     * @param color Image dominant color to set for built suggestion.
     * @return Omnibox suggestion builder.
     */
    public AutocompleteMatchBuilder setImageDominantColor(String color) {
        mImageDominantColor = color;
        return this;
    }

    /**
     * @param isSearch Whether built suggestion is a search suggestion.
     * @return Omnibox suggestion builder.
     */
    public AutocompleteMatchBuilder setIsSearch(boolean isSearch) {
        mIsSearchType = isSearch;
        return this;
    }

    /**
     * @param answer The answer in the Omnibox suggestion.
     * @return Omnibox suggestion builder.
     */
    public AutocompleteMatchBuilder setAnswer(SuggestionAnswer answer) {
        mAnswer = answer;
        return this;
    }

    /**
     * @param answer The type of answer in the Omnibox suggestion.
     * @return Omnibox suggestion builder.
     */
    public AutocompleteMatchBuilder setAnswerType(AnswerType answerType) {
        mAnswerType = answerType;
        return this;
    }

    /**
     * @param clipboardImageData Image data to set for this suggestion.
     * @return Omnibox suggestion builder.
     */
    public AutocompleteMatchBuilder setClipboardImageData(byte[] clipboardImageData) {
        mClipboardImageData = clipboardImageData;
        return this;
    }

    /**
     * @param hasTabMatch Whether built suggestion has tab match.
     * @return Omnibox suggestion builder.
     */
    public AutocompleteMatchBuilder setHasTabMatch(boolean hasTabMatch) {
        mHasTabMatch = hasTabMatch;
        return this;
    }

    /**
     * @param relevance Relevance score for newly constructed suggestion.
     * @return Omnibox suggestion builder.
     */
    public AutocompleteMatchBuilder setRelevance(int relevance) {
        mRelevance = relevance;
        return this;
    }

    /**
     * @param type Suggestion type.
     * @return Omnibox suggestion builder.
     */
    public AutocompleteMatchBuilder setType(@OmniboxSuggestionType int type) {
        mType = type;
        return this;
    }

    /**
     * @param subtype Suggestion subtype.
     * @return Omnibox suggestion builder.
     */
    public AutocompleteMatchBuilder addSubtype(int subtype) {
        mSubtypes.add(subtype);
        return this;
    }

    /**
     * @param actions List of actions to add to the AutocompleteMatch.
     * @return Omnibox suggestion builder.
     */
    public AutocompleteMatchBuilder setActions(@NonNull List<OmniboxAction> actions) {
        mActions = actions;
        return this;
    }

    /**
     * @param isDeletable Whether the match should be made deletable.
     * @return Omnibox suggestion builder.
     */
    public AutocompleteMatchBuilder setDeletable(boolean isDeletable) {
        mIsDeletable = isDeletable;
        return this;
    }

    /**
     * @param allowedToBeDefaultMatch Whether the match is allowed to be the default match.
     * @return Omnibox suggestion builder.
     */
    public AutocompleteMatchBuilder setAllowedToBeDefaultMatch(boolean allowedToBeDefaultMatch) {
        mAllowedToBeDefaultMatch = allowedToBeDefaultMatch;
        return this;
    }

    /**
     * @param inlineAutocompletion The inline autocompletion to display after the user's input in
     *     the omnibox.
     * @return Omnibox suggestion builder.
     */
    public AutocompleteMatchBuilder setInlineAutocompletion(String inlineAutocompletion) {
        mInlineAutocompletion = inlineAutocompletion;
        return this;
    }

    /**
     * @param additionalText This string is displayed adjacent to the omnibox if this match is the
     *     default.
     * @return Omnibox suggestion builder.
     */
    public AutocompleteMatchBuilder setAdditionalText(String additionalText) {
        mAdditionalText = additionalText;
        return this;
    }

    /**
     * @param serializedAnswerTemplate Serialized RichAnswerTemplate proto.
     * @return Omnibox suggestion builder.
     */
    public AutocompleteMatchBuilder setSerializedAnswerTemplate(byte[] serializedAnswerTemplate) {
        mSerializedAnswerTemplate = serializedAnswerTemplate;
        return this;
    }
}
