// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.omnibox.suggestions.entity;

import org.chromium.build.annotations.NullMarked;
import org.chromium.chrome.browser.omnibox.styles.OmniboxResourceProvider;
import org.chromium.chrome.browser.omnibox.suggestions.basic.SuggestionViewViewBinder;

/** A mechanism binding EntitySuggestion properties to its view. */
@NullMarked
public class EntitySuggestionViewBinder extends SuggestionViewViewBinder {
    public EntitySuggestionViewBinder(OmniboxResourceProvider resourceProvider) {
        super(resourceProvider);
    }
}
