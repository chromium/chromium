// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.test.util.browser.suggestions;

import static org.chromium.chrome.test.util.browser.suggestions.ContentSuggestionsTestUtils.createDummySuggestions;

import android.graphics.Bitmap;
import android.graphics.BitmapFactory;

import org.chromium.base.Callback;
import org.chromium.base.ObserverList;
import org.chromium.base.task.PostTask;
import org.chromium.base.test.util.UrlUtils;
import org.chromium.chrome.browser.ntp.cards.SuggestionsCategoryInfo;
import org.chromium.chrome.browser.ntp.snippets.CategoryInt;
import org.chromium.chrome.browser.ntp.snippets.CategoryStatus;
import org.chromium.chrome.browser.ntp.snippets.SnippetArticle;
import org.chromium.chrome.browser.ntp.snippets.SnippetsBridge;
import org.chromium.chrome.browser.ntp.snippets.SuggestionsSource;
import org.chromium.content_public.browser.UiThreadTaskTraits;

import java.util.ArrayList;
import java.util.Collections;
import java.util.HashMap;
import java.util.LinkedHashMap;
import java.util.List;
import java.util.Map;

/**
 * A fake Suggestions source for use in unit and instrumentation tests.
 */
public class FakeSuggestionsSource implements SuggestionsSource {
    private ObserverList<Observer> mObserverList = new ObserverList<>();
    private final List<Integer> mCategories = new ArrayList<>();
    private final Map<Integer, List<SnippetArticle>> mSuggestions = new HashMap<>();
    private final Map<Integer, Integer> mCategoryStatus = new LinkedHashMap<>();
    private final Map<Integer, SuggestionsCategoryInfo> mCategoryInfo = new HashMap<>();

    // Maps within-category ids to their fake bitmaps.
    private final Map<String, Bitmap> mThumbnails = new HashMap<>();
    private final Map<String, Bitmap> mFavicons = new HashMap<>();

    private Bitmap mDefaultFavicon;

    private final List<Integer> mDismissedCategories = new ArrayList<>();
    private final Map<Integer, List<SnippetArticle>> mDismissedCategorySuggestions =
            new HashMap<>();
    private final Map<Integer, Integer> mDismissedCategoryStatus = new LinkedHashMap<>();
    private final Map<Integer, SuggestionsCategoryInfo> mDismissedCategoryInfo = new HashMap<>();

    private boolean mRemoteSuggestionsEnabled = true;

    /**
     * Sets the status to be returned for a given category.
     */
    public void setStatusForCategory(@CategoryInt int category, @CategoryStatus int status) {
        mCategoryStatus.put(category, status);
        if (status == CategoryStatus.NOT_PROVIDED) {
            mCategories.remove(Integer.valueOf(category));
        } else if (!mCategories.contains(category)) {
            mCategories.add(category);
        }
        for (Observer observer : mObserverList) observer.onCategoryStatusChanged(category, status);
    }

    /**
     * Sets the suggestions to be returned for a given category.
     */
    public void setSuggestionsForCategory(
            @CategoryInt int category, List<SnippetArticle> suggestions) {
        // Copy the suggestions list so that it can't be modified anymore.
        mSuggestions.put(category, new ArrayList<>(suggestions));
        for (Observer observer : mObserverList) observer.onNewSuggestions(category);
    }

    /**
     * Creates and sets the suggestions to be returned for a given category.
     * @return The suggestions created.
     * @see ContentSuggestionsTestUtils#createDummySuggestions(int, int)
     * @see #setSuggestionsForCategory(int, List)
     */
    public List<SnippetArticle> createAndSetSuggestions(int count, @CategoryInt int category) {
        List<SnippetArticle> suggestions = createDummySuggestions(count, category);
        setSuggestionsForCategory(category, suggestions);
        return suggestions;
    }

    /**
     * Creates and sets the suggestions to be returned for a given category.
     * @return The suggestions created.
     * @see ContentSuggestionsTestUtils#createDummySuggestions(int, int, String)
     * @see #setSuggestionsForCategory(int, List)
     */
    public List<SnippetArticle> createAndSetSuggestions(
            int count, @CategoryInt int category, String suffix) {
        List<SnippetArticle> suggestions = createDummySuggestions(count, category, suffix);
        setSuggestionsForCategory(category, suggestions);
        return suggestions;
    }

    /**
     * Sets the metadata to be returned for a given category.
     */
    public void setInfoForCategory(@CategoryInt int category, SuggestionsCategoryInfo info) {
        mCategoryInfo.put(category, info);
    }

    /**
     * Sets the bitmap to be returned when the thumbnail is requested for a suggestion with that
     * (within-category) id.
     * Note: Does not check for categories, try to not have overlapping ids across categories while
     * creating test data.
     */
    public void setThumbnailForId(String id, Bitmap bitmap) {
        mThumbnails.put(id, bitmap);
    }

    /**
     * Sets the bitmap to be returned when the thumbnail is requested for a suggestion with that
     * (within-category) id.
     * Note: Does not check for categories, try to not have overlapping ids across categories while
     * creating test data.
     * @param id Id of the suggestion ({@link SnippetArticle#mIdWithinCategory}).
     * @param thumbnailPath Path to the file within the test resources directory.
     */
    public void setThumbnailForId(String id, String thumbnailPath) {
        setThumbnailForId(id, BitmapFactory.decodeFile(UrlUtils.getTestFilePath(thumbnailPath)));
    }

    /**
     * Sets the bitmap to be returned when the favicon is requested for a suggestion with that
     * (within-category) id.
     */
    public void setFaviconForId(String id, Bitmap bitmap) {
        mFavicons.put(id, bitmap);
    }

    /**
     * Sets a default favicon to be returned for suggestions that don't have a specific favicon
     * defined.
     * @param bitmap The favicon bitmap to be returned by default.
     */
    public void setDefaultFavicon(Bitmap bitmap) {
        mDefaultFavicon = bitmap;
    }

    /**
     * Removes the given suggestion from the source and notifies any observer that it has been
     * invalidated.
     */
    public void fireSuggestionInvalidated(@CategoryInt int category, String idWithinCategory) {
        for (SnippetArticle suggestion : mSuggestions.get(category)) {
            if (suggestion.mIdWithinCategory.equals(idWithinCategory)) {
                mSuggestions.get(category).remove(suggestion);
                break;
            }
        }
        for (Observer observer : mObserverList) {
            observer.onSuggestionInvalidated(category, idWithinCategory);
        }
    }

    /**
     * Notifies the observer that a full refresh is required.
     */
    public void fireFullRefreshRequired() {
        for (Observer observer : mObserverList) observer.onFullRefreshRequired();
    }

    /**
     * Notifies the observer that the suggestions visibility has changed for the specified category.
     */
    public void fireOnSuggestionsVisibilityChanged(@CategoryInt int category) {
        for (Observer observer : mObserverList) observer.onSuggestionsVisibilityChanged(category);
    }

    /**
     * Removes a category from the fake source without notifying anyone.
     */
    public void silentlyRemoveCategory(int category) {
        mSuggestions.remove(category);
        mCategoryStatus.remove(category);
        mCategoryInfo.remove(category);
        mCategories.remove(Integer.valueOf(category));
    }

    /**
     * Clears the list of observers.
     */
    public void removeObservers() {
        mObserverList.clear();
    }

    @Override
    public void fetchRemoteSuggestions() {}

    @Override
    public boolean areRemoteSuggestionsEnabled() {
        return mRemoteSuggestionsEnabled;
    }

    public void setRemoteSuggestionsEnabled(boolean enabled) {
        mRemoteSuggestionsEnabled = enabled;
    }

    @Override
    public void dismissSuggestion(SnippetArticle suggestion) {
        for (List<SnippetArticle> suggestions : mSuggestions.values()) {
            suggestions.remove(suggestion);
        }
    }

    @Override
    public void dismissCategory(@CategoryInt int category) {
        mDismissedCategorySuggestions.put(category, mSuggestions.get(category));
        mDismissedCategoryStatus.put(category, mCategoryStatus.get(category));
        mDismissedCategoryInfo.put(category, mCategoryInfo.get(category));
        mDismissedCategories.add(category);
        silentlyRemoveCategory(category);
    }

    @Override
    public void restoreDismissedCategories() {
        for (int category : mDismissedCategories) {
            mSuggestions.put(category, mDismissedCategorySuggestions.remove(category));
            mCategoryStatus.put(category, mDismissedCategoryStatus.remove(category));
            mCategoryInfo.put(category, mDismissedCategoryInfo.remove(category));
            mCategories.add(category);
        }
        mDismissedCategories.clear();
    }

    @Override
    public void fetchSuggestionImage(
            final SnippetArticle suggestion, final Callback<Bitmap> callback) {
        if (mThumbnails.containsKey(suggestion.mIdWithinCategory)) {
            PostTask.postTask(UiThreadTaskTraits.DEFAULT,
                    () -> callback.onResult(mThumbnails.get(suggestion.mIdWithinCategory)));
        }
    }

    @Override
    public void fetchSuggestionFavicon(final SnippetArticle suggestion, int minimumSizePx,
            int desiredSizePx, final Callback<Bitmap> callback) {
        final Bitmap favicon = getFaviconForId(suggestion.mIdWithinCategory);
        if (favicon != null)
            PostTask.postTask(UiThreadTaskTraits.DEFAULT, () -> callback.onResult(favicon));
    }

    private Bitmap getFaviconForId(String id) {
        if (mFavicons.containsKey(id)) return mFavicons.get(id);

        return mDefaultFavicon;
    }

    @Override
    public void fetchSuggestions(@CategoryInt int category, String[] displayedSuggestionIds,
            Callback<List<SnippetArticle>> successCallback, Runnable failureRunnable) {
    }

    @Override
    public void addObserver(Observer observer) {
        mObserverList.addObserver(observer);
    }

    @Override
    public void removeObserver(Observer observer) {
        mObserverList.removeObserver(observer);
    }

    @Override
    public void destroy() {}

    @Override
    public int[] getCategories() {
        int[] result = new int[mCategories.size()];
        int index = 0;
        for (int id : mCategories) result[index++] = id;
        return result;
    }

    @CategoryStatus
    @Override
    public int getCategoryStatus(@CategoryInt int category) {
        return mCategoryStatus.get(category);
    }

    @Override
    public SuggestionsCategoryInfo getCategoryInfo(int category) {
        return mCategoryInfo.get(category);
    }

    @Override
    public List<SnippetArticle> getSuggestionsForCategory(int category) {
        if (!SnippetsBridge.isCategoryStatusAvailable(mCategoryStatus.get(category))) {
            return Collections.emptyList();
        }
        List<SnippetArticle> result = mSuggestions.get(category);
        return result == null ? Collections.<SnippetArticle>emptyList() : new ArrayList<>(result);
    }
}
