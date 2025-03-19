// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.components.autofill;

import android.util.Pair;

/** A convenience class for displaying keyed values in a dropdown. */
public class DropdownKeyValue extends Pair<String, String> {
    public DropdownKeyValue(String key, String value) {
        super(key, value);
    }

    /**
     * @return The key identifier.
     */
    public String getKey() {
        return super.first;
    }

    /**
     * @return The human-readable localized display value.
     */
    public String getValue() {
        return super.second;
    }

    @Override
    public String toString() {
        return super.second.toString();
    }
}
