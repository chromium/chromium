// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.ui.plus_addresses;

import org.chromium.ui.modelutil.MVCListAdapter.ModelList;
import org.chromium.ui.modelutil.PropertyKey;
import org.chromium.ui.modelutil.PropertyModel;
import org.chromium.ui.modelutil.PropertyModel.ReadableObjectPropertyKey;
import org.chromium.ui.modelutil.PropertyModel.WritableObjectPropertyKey;

class AllPlusAddressesBottomSheetProperties {
    static final PropertyModel.WritableBooleanPropertyKey VISIBLE =
            new PropertyModel.WritableBooleanPropertyKey("visible");
    static final WritableObjectPropertyKey<String> TITLE = new WritableObjectPropertyKey<>("title");
    static final ReadableObjectPropertyKey<ModelList> PLUS_PROFILES =
            new ReadableObjectPropertyKey<>("plus_profiles");

    static final PropertyKey[] ALL_KEYS = {
        VISIBLE, TITLE, PLUS_PROFILES,
    };

    static PropertyModel createDefaultModel() {
        return new PropertyModel.Builder(ALL_KEYS)
                .with(VISIBLE, false)
                .with(TITLE, "")
                .with(PLUS_PROFILES, new ModelList())
                .build();
    }

    static class PlusProfileProperties {
        static final PropertyModel.ReadableObjectPropertyKey<PlusProfile> PLUS_PROFILE =
                new PropertyModel.ReadableObjectPropertyKey<>("plus_profile");

        static final PropertyKey[] ALL_KEYS = {PLUS_PROFILE};

        static PropertyModel createPlusProfileModel(PlusProfile profile) {
            return new PropertyModel.Builder(PlusProfileProperties.ALL_KEYS)
                    .with(PLUS_PROFILE, profile)
                    .build();
        }

        private PlusProfileProperties() {}
    }

    @interface ItemType {
        int PLUS_PROFILE = 0;
    }

    private AllPlusAddressesBottomSheetProperties() {}
}
