// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.ui.android.webid;

import androidx.annotation.IntDef;

import org.chromium.ui.modelutil.PropertyKey;
import org.chromium.ui.modelutil.PropertyModel;

import java.lang.annotation.Retention;
import java.lang.annotation.RetentionPolicy;

/**
 * Properties defined here reflect the state of the AccountSelection-components.
 */
class AccountSelectionProperties {
    /**
     * Properties defined here reflect the state of the header in the AccountSelection
     * sheet.
     */
    static class HeaderProperties {
        static final PropertyModel.ReadableBooleanPropertyKey SINGLE_ACCOUNT =
                new PropertyModel.ReadableBooleanPropertyKey("single_account");
        static final PropertyModel.ReadableObjectPropertyKey<String> FORMATTED_URL =
                new PropertyModel.ReadableObjectPropertyKey<>("formatted_url");

        static final PropertyKey[] ALL_KEYS = {SINGLE_ACCOUNT, FORMATTED_URL};

        private HeaderProperties() {}
    }

    @IntDef({ItemType.HEADER})
    @Retention(RetentionPolicy.SOURCE)
    @interface ItemType {
        /**
         * The header at the top of the accounts sheet.
         */
        int HEADER = 1;
    }

    private AccountSelectionProperties() {}
}
