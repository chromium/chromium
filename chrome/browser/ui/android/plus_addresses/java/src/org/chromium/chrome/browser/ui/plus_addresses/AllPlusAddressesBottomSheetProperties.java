// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.ui.plus_addresses;

import org.chromium.base.Callback;
import org.chromium.ui.modelutil.MVCListAdapter.ModelList;
import org.chromium.ui.modelutil.PropertyKey;
import org.chromium.ui.modelutil.PropertyModel;
import org.chromium.ui.modelutil.PropertyModel.ReadableObjectPropertyKey;
import org.chromium.ui.modelutil.PropertyModel.WritableObjectPropertyKey;

class AllPlusAddressesBottomSheetProperties {
    static final PropertyModel.WritableBooleanPropertyKey VISIBLE =
            new PropertyModel.WritableBooleanPropertyKey("visible");
    static final WritableObjectPropertyKey<String> TITLE = new WritableObjectPropertyKey<>("title");
    static final WritableObjectPropertyKey<String> WARNING =
            new WritableObjectPropertyKey<>("warning");
    static final WritableObjectPropertyKey<String> QUERY_HINT =
            new WritableObjectPropertyKey<>("query_hint");
    static final PropertyModel.WritableObjectPropertyKey<Callback<String>> ON_QUERY_TEXT_CHANGE =
            new PropertyModel.WritableObjectPropertyKey<>("on_query_text_change");
    static final ReadableObjectPropertyKey<ModelList> PLUS_PROFILES =
            new ReadableObjectPropertyKey<>("plus_profiles");
    static final PropertyModel.WritableObjectPropertyKey<Runnable> ON_DISMISSED =
            new PropertyModel.WritableObjectPropertyKey<>("on_dismissed");

    static final PropertyKey[] ALL_KEYS = {
        VISIBLE, TITLE, WARNING, QUERY_HINT, ON_QUERY_TEXT_CHANGE, PLUS_PROFILES, ON_DISMISSED,
    };

    static PropertyModel createDefaultModel() {
        return new PropertyModel.Builder(ALL_KEYS)
                .with(VISIBLE, false)
                .with(TITLE, "")
                .with(WARNING, "")
                .with(QUERY_HINT, "")
                .with(PLUS_PROFILES, new ModelList())
                .with(ON_DISMISSED, () -> {})
                .build();
    }

    static class PlusProfileProperties {
        static final PropertyModel.ReadableObjectPropertyKey<PlusProfile> PLUS_PROFILE =
                new PropertyModel.ReadableObjectPropertyKey<>("plus_profile");
        static final PropertyModel.WritableObjectPropertyKey<Callback<String>>
                ON_PLUS_ADDRESS_SELECTED =
                        new PropertyModel.WritableObjectPropertyKey<>("on_plus_address_selected");

        static final PropertyKey[] ALL_KEYS = {PLUS_PROFILE, ON_PLUS_ADDRESS_SELECTED};

        static PropertyModel createPlusProfileModel(
                PlusProfile profile, Callback<String> callback) {
            return new PropertyModel.Builder(PlusProfileProperties.ALL_KEYS)
                    .with(PLUS_PROFILE, profile)
                    .with(ON_PLUS_ADDRESS_SELECTED, callback)
                    .build();
        }

        private PlusProfileProperties() {}
    }

    @interface ItemType {
        int PLUS_PROFILE = 0;
    }

    private AllPlusAddressesBottomSheetProperties() {}
}
