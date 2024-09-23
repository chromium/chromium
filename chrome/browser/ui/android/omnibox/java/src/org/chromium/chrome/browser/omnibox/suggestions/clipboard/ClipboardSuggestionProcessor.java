// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.omnibox.suggestions.clipboard;

import android.content.Context;
import android.graphics.Bitmap;
import android.graphics.BitmapFactory;

import androidx.annotation.NonNull;
import androidx.annotation.VisibleForTesting;

import org.chromium.base.metrics.RecordUserAction;
import org.chromium.chrome.browser.omnibox.R;
import org.chromium.chrome.browser.omnibox.styles.OmniboxDrawableState;
import org.chromium.chrome.browser.omnibox.styles.OmniboxImageSupplier;
import org.chromium.chrome.browser.omnibox.styles.OmniboxResourceProvider;
import org.chromium.chrome.browser.omnibox.styles.SuggestionSpannable;
import org.chromium.chrome.browser.omnibox.suggestions.SuggestionHost;
import org.chromium.chrome.browser.omnibox.suggestions.base.BaseSuggestionViewProcessor;
import org.chromium.chrome.browser.omnibox.suggestions.base.BaseSuggestionViewProperties.Action;
import org.chromium.chrome.browser.omnibox.suggestions.basic.SuggestionViewProperties;
import org.chromium.components.omnibox.AutocompleteMatch;
import org.chromium.components.omnibox.OmniboxSuggestionType;
import org.chromium.components.omnibox.suggestions.OmniboxSuggestionUiType;
import org.chromium.ui.modelutil.PropertyModel;

import java.util.Arrays;
import java.util.Optional;

/** A class that handles model and view creation for the clipboard suggestions. */
public class ClipboardSuggestionProcessor extends BaseSuggestionViewProcessor {
    /**
     * @param context An Android context.
     * @param suggestionHost A handle to the object using the suggestions.
     * @param imageSupplier Supplier used to retrieve suggestion icons and images.
     */
    public ClipboardSuggestionProcessor(
            @NonNull Context context,
            @NonNull SuggestionHost suggestionHost,
            @NonNull Optional<OmniboxImageSupplier> imageSupplier) {
        super(context, suggestionHost, imageSupplier);
    }

    @Override
    public boolean doesProcessSuggestion(@NonNull AutocompleteMatch suggestion, int position) {
        return suggestion.getType() == OmniboxSuggestionType.CLIPBOARD_URL
                || suggestion.getType() == OmniboxSuggestionType.CLIPBOARD_TEXT
                || suggestion.getType() == OmniboxSuggestionType.CLIPBOARD_IMAGE;
    }

    @Override
    public int getViewTypeId() {
        return OmniboxSuggestionUiType.CLIPBOARD_SUGGESTION;
    }

    @Override
    public @NonNull PropertyModel createModel() {
        return new PropertyModel(SuggestionViewProperties.ALL_KEYS);
    }

    @Override
    public void populateModel(
            @NonNull AutocompleteMatch suggestion, @NonNull PropertyModel model, int position) {
        super.populateModel(suggestion, model, position);

        model.set(SuggestionViewProperties.IS_SEARCH_SUGGESTION, suggestion.isSearchSuggestion());
        model.set(
                SuggestionViewProperties.TEXT_LINE_1_TEXT,
                new SuggestionSpannable(suggestion.getDescription()));

        setupContentField(suggestion, model, /* showContent= */ false);
    }

    @Override
    protected boolean allowOmniboxActions() {
        return false;
    }

    /**
     * Set the content related properties for the suggestion.
     *
     * @param suggestion The current suggestion.
     * @param model Model representing current suggestion.
     * @param showContent Whether the contents should be shown.
     */
    private void setupContentField(
            @NonNull AutocompleteMatch suggestion,
            @NonNull PropertyModel model,
            boolean showContent) {
        String displayText = showContent ? suggestion.getDisplayText() : "";
        model.set(SuggestionViewProperties.TEXT_LINE_2_TEXT, new SuggestionSpannable(displayText));

        updateSuggestionIcon(suggestion, model, showContent);
        updateActionButton(suggestion, model, showContent);
    }

    /**
     * Update the icon for the current suggestion.
     *
     * <ul>
     *   <li>If CLIPBOARD_SUGGESTION_CONTENT_HIDDEN is enabled, the content of the clipboard
     *       suggestion will not be shown by default until users clicked reveal button.
     *   <li>If CLIPBOARD_SUGGESTION_CONTENT_HIDDEN is not enabled, the content of the clipboard
     *       suggestion will be shown if it is available.
     * </ul>
     *
     * @param suggestion The current suggestion.
     * @param model Model representing current suggestion.
     * @param showContent Whether the contents should be shown.
     */
    private void updateSuggestionIcon(
            @NonNull AutocompleteMatch suggestion,
            @NonNull PropertyModel model,
            boolean showContent) {
        if (!showContent) {
            setOmniboxDrawableState(model, getFallbackIcon(suggestion));
            return;
        }

        // Show thumbnail for image suggestion if thumbnail available.
        if (suggestion.getType() == OmniboxSuggestionType.CLIPBOARD_IMAGE) {
            byte[] imageData = suggestion.getClipboardImageData();
            if (imageData != null && imageData.length > 0) {
                Bitmap bitmap = BitmapFactory.decodeByteArray(imageData, 0, imageData.length);

                if (bitmap != null) {
                    // TODO(crbug.com/40133944): This is short term solution, resize need to be
                    // handled somewhere else.
                    if (bitmap.getWidth() > 0
                            && bitmap.getHeight() > 0
                            && (bitmap.getWidth() > getDecorationImageSize()
                                    || bitmap.getHeight() > getDecorationImageSize())) {
                        float max = Math.max(bitmap.getWidth(), bitmap.getHeight());
                        float scale = ((float) getDecorationImageSize()) / max;
                        float width = bitmap.getWidth();
                        float height = bitmap.getHeight();
                        bitmap =
                                Bitmap.createScaledBitmap(
                                        bitmap,
                                        (int) Math.round(scale * width),
                                        (int) Math.round(scale * height),
                                        true);
                    }
                    setOmniboxDrawableState(model, OmniboxDrawableState.forImage(mContext, bitmap));
                }
            }
        } else if (!suggestion.isSearchSuggestion()) {
            fetchSuggestionFavicon(model, suggestion.getUrl());
        }
    }

    /**
     * Update the action button for the current suggestion.
     *
     * @param suggestion The current suggestion.
     * @param model Model representing current suggestion.
     * @param showContent Whether the contents should be shown.
     */
    private void updateActionButton(
            @NonNull AutocompleteMatch suggestion,
            @NonNull PropertyModel model,
            boolean showContent) {
        int icon =
                showContent ? R.drawable.ic_visibility_off_black : R.drawable.ic_visibility_black;
        String iconString =
                OmniboxResourceProvider.getString(
                        mContext,
                        showContent
                                ? R.string.accessibility_omnibox_conceal_clipboard_contents
                                : R.string.accessibility_omnibox_reveal_clipboard_contents);
        String announcementString =
                OmniboxResourceProvider.getString(
                        mContext,
                        showContent
                                ? R.string.accessibility_omnibox_conceal_button_announcement
                                : R.string.accessibility_omnibox_reveal_button_announcement);
        Runnable action =
                showContent
                        ? () -> concealButtonClickHandler(suggestion, model)
                        : () -> revealButtonClickHandler(suggestion, model);
        setActionButtons(
                model,
                Arrays.asList(
                        new Action(
                                OmniboxDrawableState.forSmallIcon(mContext, icon, true),
                                iconString,
                                announcementString,
                                action)));
    }

    @Override
    protected void onSuggestionClicked(@NonNull AutocompleteMatch suggestion, int position) {
        if (!suggestion.getUrl().isEmpty()) {
            super.onSuggestionClicked(suggestion, position);
            return;
        }

        // Retrieve suggestion content before propagating the Click event.
        suggestion.updateWithClipboardContent(
                () -> {
                    super.onSuggestionClicked(suggestion, position);
                });
    }

    /**
     * Handle the click event for the reveal button.
     *
     * @param suggestion Selected suggestion.
     * @param model Model representing current suggestion.
     */
    // TODO(crbug.com/40177279): Make revealButtonClickHandler and concealButtonClickHandler
    // private.
    @VisibleForTesting(otherwise = VisibleForTesting.PRIVATE)
    public void revealButtonClickHandler(
            @NonNull AutocompleteMatch suggestion, @NonNull PropertyModel model) {
        RecordUserAction.record("Omnibox.ClipboardSuggestion.Reveal");
        if (suggestion.getUrl().isEmpty()) {
            suggestion.updateWithClipboardContent(
                    () -> setupContentField(suggestion, model, /* showContent= */ true));
            return;
        }
        setupContentField(suggestion, model, /* showContent= */ true);
    }

    /**
     * Handle the click event for the conceal button.
     *
     * @param suggestion Selected suggestion.
     * @param model Model representing current suggestion.
     */
    @VisibleForTesting(otherwise = VisibleForTesting.PRIVATE)
    public void concealButtonClickHandler(
            @NonNull AutocompleteMatch suggestion, @NonNull PropertyModel model) {
        RecordUserAction.record("Omnibox.ClipboardSuggestion.Conceal");
        setupContentField(suggestion, model, /* showContent= */ false);
    }
}
