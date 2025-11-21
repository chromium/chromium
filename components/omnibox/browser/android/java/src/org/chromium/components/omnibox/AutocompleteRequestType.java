// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.components.omnibox;

import androidx.annotation.IntDef;

import org.chromium.build.annotations.NullMarked;

import java.lang.annotation.ElementType;
import java.lang.annotation.Retention;
import java.lang.annotation.RetentionPolicy;
import java.lang.annotation.Target;

/** The type of fulfillment for the autocomplete request. */
// LINT.IfChange(AutocompleteRequestType)
@IntDef({
    AutocompleteRequestType.SEARCH,
    AutocompleteRequestType.SEARCH_PREFETCH,
    AutocompleteRequestType.AI_MODE,
    AutocompleteRequestType.IMAGE_GENERATION
})
@Retention(RetentionPolicy.SOURCE)
@Target({ElementType.TYPE_USE})
@NullMarked
public @interface AutocompleteRequestType {
    /** Standard search fulfillment. */
    int SEARCH = 0;

    /** Prefetch suggestions for the search autocomplete request type. */
    int SEARCH_PREFETCH = 1;

    /** AI-powered fulfillment. */
    int AI_MODE = 2;

    /** Image generation. */
    int IMAGE_GENERATION = 3;

    int COUNT = 4;
    /* Note: account for new types in {@link FuseboxCoordinator#isConventionalFulfillmentType}. */
}

// LINT.ThenChange(//tools/metrics/histograms/metadata/omnibox/enums.xml:AutocompleteRequestType)
