// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.omnibox;

import static org.chromium.chrome.browser.omnibox.OmniboxChipProperties.AVAILABLE_WIDTH;
import static org.chromium.chrome.browser.omnibox.OmniboxChipProperties.CONTENT_DESC;
import static org.chromium.chrome.browser.omnibox.OmniboxChipProperties.ICON;
import static org.chromium.chrome.browser.omnibox.OmniboxChipProperties.ON_CLICK;
import static org.chromium.chrome.browser.omnibox.OmniboxChipProperties.TEXT;

import android.graphics.drawable.Drawable;

import androidx.annotation.Px;

import org.chromium.build.annotations.NullMarked;
import org.chromium.ui.modelutil.PropertyModel;

/** Mediator for the omnibox chip. */
@NullMarked
class OmniboxChipMediator {
    private final PropertyModel mModel;

    /**
     * Creates an OmniboxChipMediator.
     *
     * @param model MVC property model to write changes to.
     */
    OmniboxChipMediator(PropertyModel model) {
        mModel = model;
    }

    void updateChip(String text, Drawable icon, String contentDesc, Runnable onClick) {
        mModel.set(TEXT, text);
        mModel.set(ICON, icon);
        mModel.set(CONTENT_DESC, contentDesc);
        mModel.set(ON_CLICK, onClick);
    }

    void setAvailableWidth(@Px int availableWidth) {
        mModel.set(AVAILABLE_WIDTH, availableWidth);
    }
}
