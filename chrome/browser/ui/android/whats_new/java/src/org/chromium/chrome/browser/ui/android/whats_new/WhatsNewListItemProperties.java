// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.ui.android.whats_new;

import android.content.Context;
import android.view.View.OnClickListener;

import org.chromium.base.Callback;
import org.chromium.build.annotations.NullMarked;
import org.chromium.chrome.browser.ui.android.whats_new.features.WhatsNewFeature;
import org.chromium.ui.modelutil.PropertyKey;
import org.chromium.ui.modelutil.PropertyModel;
import org.chromium.ui.modelutil.PropertyModel.ReadableIntPropertyKey;
import org.chromium.ui.modelutil.PropertyModel.ReadableObjectPropertyKey;
import org.chromium.ui.modelutil.PropertyModel.WritableIntPropertyKey;
import org.chromium.ui.modelutil.PropertyModel.WritableObjectPropertyKey;

/** Properties for the a feature item in the What's New page. */
@NullMarked
public class WhatsNewListItemProperties {
    public static final int DEFAULT_ITEM_TYPE = 0;

    /* PropertyKey for feature title in What's New feature list item */
    static final ReadableObjectPropertyKey<String> TITLE_ID = new ReadableObjectPropertyKey<>();
    /* PropertyKey for feature description in What's New feature list item */
    static final ReadableObjectPropertyKey<String> DESCRIPTION_ID =
            new ReadableObjectPropertyKey<>();
    /* PropertyKey for feature image icon in What's New feature list item */
    static final ReadableIntPropertyKey ICON_IMAGE_RES_ID = new WritableIntPropertyKey();
    /* PropertyKey for click listener of the What's New feature list item */
    static final ReadableObjectPropertyKey<OnClickListener> ON_CLICK =
            new WritableObjectPropertyKey<>();

    static final PropertyKey[] ALL_KEYS = {TITLE_ID, DESCRIPTION_ID, ICON_IMAGE_RES_ID, ON_CLICK};

    static PropertyModel buildModelForFeature(
            Context context, WhatsNewFeature featureItem, Callback<WhatsNewFeature> onClick) {
        return new PropertyModel.Builder(WhatsNewListItemProperties.ALL_KEYS)
                .with(WhatsNewListItemProperties.TITLE_ID, featureItem.getTitle(context))
                .with(
                        WhatsNewListItemProperties.DESCRIPTION_ID,
                        featureItem.getDescription(context))
                .with(WhatsNewListItemProperties.ICON_IMAGE_RES_ID, featureItem.getIconResId())
                .with(WhatsNewListItemProperties.ON_CLICK, (v) -> onClick.onResult(featureItem))
                .build();
    }
}
