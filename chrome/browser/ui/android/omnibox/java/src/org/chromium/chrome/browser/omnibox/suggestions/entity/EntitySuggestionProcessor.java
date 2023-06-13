// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.omnibox.suggestions.entity;

import android.content.Context;
import android.graphics.Bitmap;
import android.graphics.Color;
import android.text.TextUtils;

import androidx.annotation.VisibleForTesting;

import org.chromium.base.ThreadUtils;
import org.chromium.base.supplier.Supplier;
import org.chromium.chrome.browser.omnibox.OmniboxFeatures;
import org.chromium.chrome.browser.omnibox.R;
import org.chromium.chrome.browser.omnibox.suggestions.SuggestionHost;
import org.chromium.chrome.browser.omnibox.suggestions.base.BaseSuggestionViewProcessor;
import org.chromium.chrome.browser.omnibox.suggestions.base.SuggestionDrawableState;
import org.chromium.components.image_fetcher.ImageFetcher;
import org.chromium.components.omnibox.AutocompleteMatch;
import org.chromium.components.omnibox.OmniboxSuggestionType;
import org.chromium.components.omnibox.suggestions.OmniboxSuggestionUiType;
import org.chromium.ui.modelutil.PropertyModel;
import org.chromium.url.GURL;

import java.util.ArrayList;
import java.util.HashMap;
import java.util.List;
import java.util.Map;

/** A class that handles model and view creation for the Entity suggestions. */
public class EntitySuggestionProcessor extends BaseSuggestionViewProcessor {
    private final Map<GURL, List<PropertyModel>> mPendingImageRequests;
    private final Supplier<ImageFetcher> mImageFetcherSupplier;

    /**
     * @param context An Android context.
     * @param suggestionHost A handle to the object using the suggestions.
     */
    public EntitySuggestionProcessor(Context context, SuggestionHost suggestionHost,
            Supplier<ImageFetcher> imageFetcherSupplier) {
        super(context, suggestionHost, null);
        mPendingImageRequests = new HashMap<>();
        mImageFetcherSupplier = imageFetcherSupplier;
    }

    @Override
    public boolean doesProcessSuggestion(AutocompleteMatch suggestion, int position) {
        return suggestion.getType() == OmniboxSuggestionType.SEARCH_SUGGEST_ENTITY;
    }

    @Override
    public int getViewTypeId() {
        return OmniboxSuggestionUiType.ENTITY_SUGGESTION;
    }

    @Override
    public PropertyModel createModel() {
        return new PropertyModel(EntitySuggestionViewProperties.ALL_KEYS);
    }

    private void fetchEntityImage(AutocompleteMatch suggestion, PropertyModel model) {
        ThreadUtils.assertOnUiThread();
        final GURL url = suggestion.getImageUrl();
        if (url.isEmpty()) return;

        // Ensure an image fetcher is available prior to requesting images.
        ImageFetcher imageFetcher = mImageFetcherSupplier.get();
        if (imageFetcher == null) return;

        // Do not make duplicate answer image requests for the same URL (to avoid generating
        // duplicate bitmaps for the same image).
        if (mPendingImageRequests.containsKey(url)) {
            mPendingImageRequests.get(url).add(model);
            return;
        }

        List<PropertyModel> models = new ArrayList<>();
        models.add(model);
        mPendingImageRequests.put(url, models);

        ImageFetcher.Params params = ImageFetcher.Params.create(
                url.getSpec(), ImageFetcher.ENTITY_SUGGESTIONS_UMA_CLIENT_NAME);
        imageFetcher.fetchImage(
                params, (Bitmap bitmap) -> {
                    ThreadUtils.assertOnUiThread();

                    final List<PropertyModel> pendingModels = mPendingImageRequests.remove(url);
                    if (pendingModels == null || bitmap == null) {
                        return;
                    }

                    for (int i = 0; i < pendingModels.size(); i++) {
                        PropertyModel pendingModel = pendingModels.get(i);
                        setSuggestionDrawableState(pendingModel,
                                SuggestionDrawableState.Builder.forBitmap(mContext, bitmap)
                                        .setUseRoundedCorners(true)
                                        .setLarge(true)
                                        .build());
                    }
                });
    }

    @VisibleForTesting
    public void applyImageDominantColor(String colorSpec, PropertyModel model) {
        if (TextUtils.isEmpty(colorSpec)) {
            return;
        }

        int color;
        try {
            color = Color.parseColor(colorSpec);
        } catch (IllegalArgumentException e) {
            // The supplied color information could not be parsed.
            return;
        }

        setSuggestionDrawableState(model,
                SuggestionDrawableState.Builder.forColor(color)
                        .setLarge(true)
                        .setUseRoundedCorners(true)
                        .build());
    }

    @Override
    public void populateModel(AutocompleteMatch suggestion, PropertyModel model, int position) {
        super.populateModel(suggestion, model, position);
        setSuggestionDrawableState(model,
                SuggestionDrawableState.Builder
                        .forDrawableRes(mContext, R.drawable.ic_suggestion_magnifier)
                        .setAllowTint(true)
                        .build());

        if (!OmniboxFeatures.isLowMemoryDevice()) {
            applyImageDominantColor(suggestion.getImageDominantColor(), model);
            fetchEntityImage(suggestion, model);
        }

        model.set(EntitySuggestionViewProperties.SUBJECT_TEXT, suggestion.getDisplayText());
        model.set(EntitySuggestionViewProperties.DESCRIPTION_TEXT, suggestion.getDescription());
        setTabSwitchOrRefineAction(model, suggestion, position);
    }
}
