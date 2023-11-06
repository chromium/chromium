// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.omnibox;

import android.content.Context;
import android.os.Build;
import android.util.AttributeSet;
import android.view.ViewStructure;

/**
 * Sub-class of UrlBar that contains newer Android APIs to avoid verification errors.
 *
 * <p>Only super calls to new Android APIs belong here - if it is a normal call to a new Android
 * API, use ApiHelperForX. See crbug.com/999165 for more description of what verification errors are
 * and why they are expensive.
 */
public class UrlBarApi26 extends UrlBar {
    public UrlBarApi26(Context context, AttributeSet attrs) {
        super(context, attrs);
    }

    @Override
    public void onProvideAutofillStructure(ViewStructure structure, int autofillFlags) {
        // https://crbug.com/996402: Prevent breaking autofill services on newer versions of
        // Android.
        mRequestingAutofillStructure = true;
        super.onProvideAutofillStructure(structure, autofillFlags);
        mRequestingAutofillStructure = false;
    }

    @Override
    public int getAutofillType() {
        // https://crbug.com/1103555: Prevent augmented autofill service from taking over the
        // session by disabling both standard and augmented autofill on versions of Android
        // where both are supported.
        if (Build.VERSION.SDK_INT >= Build.VERSION_CODES.Q) {
            return AUTOFILL_TYPE_NONE;
        } else {
            return super.getAutofillType();
        }
    }
}
