// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.omnibox.suggestions.base;

import android.content.Context;

import androidx.annotation.IntDef;
import androidx.annotation.NonNull;
import androidx.annotation.Nullable;
import androidx.annotation.StringRes;

import org.chromium.chrome.browser.omnibox.suggestions.SuggestionCommonProperties;
import org.chromium.ui.modelutil.PropertyKey;
import org.chromium.ui.modelutil.PropertyModel;
import org.chromium.ui.modelutil.PropertyModel.WritableIntPropertyKey;
import org.chromium.ui.modelutil.PropertyModel.WritableObjectPropertyKey;

import java.lang.annotation.Retention;
import java.lang.annotation.RetentionPolicy;
import java.util.List;

/** The base set of properties for most omnibox suggestions. */
public class BaseSuggestionViewProperties {
    /** Describes density of the suggestions. */
    @IntDef({Density.DEFAULT, Density.COMPACT})
    @Retention(RetentionPolicy.SOURCE)
    public @interface Density {
        int DEFAULT = 0;
        int COMPACT = 1;
    }

    /**
     * Describes the content and behavior of the interactive Action Icon.
     */
    public static final class Action {
        public final SuggestionDrawableState icon;
        public final Runnable callback;
        public final @NonNull String accessibilityDescription;
        public final @Nullable String onClickAnnouncement;

        /**
         * Create a new action for suggestion.
         *
         * @param icon SuggestionDrawableState describing the icon to show.
         * @param description Content description for the action view.
         * @param onClickAnnouncement action announcement for the action view when the action view
         *         is clicked.
         * @param callback Callback to invoke when user interacts with the icon.
         */
        public Action(@NonNull SuggestionDrawableState icon, @NonNull String description,
                @Nullable String onClickAnnouncement, @NonNull Runnable callback) {
            this.icon = icon;
            this.accessibilityDescription = description;
            this.onClickAnnouncement = onClickAnnouncement;
            this.callback = callback;
        }

        /**
         * Create a new action for suggestion.
         *
         * @param icon SuggestionDrawableState describing the icon to show.
         * @param description Content description for the action view.
         * @param callback Callback to invoke when user interacts with the icon.
         */
        public Action(SuggestionDrawableState icon, String description, Runnable callback) {
            this(icon, description, null, callback);
        }

        /**
         * Create a new action for suggestion, using Accessibility description from a resource.
         *
         * @param context Current context
         * @param icon SuggestionDrawableState describing the icon to show.
         * @param descriptionRes Resource to use as a content description for the action view.
         * @param callback Callback to invoke when user interacts with the icon.
         */
        public Action(Context context, SuggestionDrawableState icon, @StringRes int descriptionRes,
                Runnable callback) {
            this(icon, context.getResources().getString(descriptionRes), callback);
        }
    }

    /** SuggestionDrawableState to show as a suggestion icon. */
    public static final WritableObjectPropertyKey<SuggestionDrawableState> ICON =
            new WritableObjectPropertyKey<>();

    /** Action Button descriptors. */
    public static final WritableObjectPropertyKey<List<Action>> ACTION_BUTTONS =
            new WritableObjectPropertyKey();

    /** Callback invoked when the Suggestion view is highlighted. */
    public static final WritableObjectPropertyKey<Runnable> ON_FOCUS_VIA_SELECTION =
            new WritableObjectPropertyKey<>();

    /** Specifies how densely suggestions should be packed. */
    public static final WritableIntPropertyKey DENSITY = new WritableIntPropertyKey();

    /** Callback invoked when user clicks the suggestion. */
    public static final WritableObjectPropertyKey<Runnable> ON_CLICK =
            new WritableObjectPropertyKey<>();

    /** Callback invoked when user long-clicks the suggestion. */
    public static final WritableObjectPropertyKey<Runnable> ON_LONG_CLICK =
            new WritableObjectPropertyKey<>();

    public static final PropertyKey[] ALL_UNIQUE_KEYS = new PropertyKey[] {
            ICON, ACTION_BUTTONS, ON_FOCUS_VIA_SELECTION, DENSITY, ON_CLICK, ON_LONG_CLICK};

    public static final PropertyKey[] ALL_KEYS = PropertyModel.concatKeys(
            PropertyModel.concatKeys(ALL_UNIQUE_KEYS, ActionChipsProperties.ALL_UNIQUE_KEYS),
            SuggestionCommonProperties.ALL_KEYS);
}
