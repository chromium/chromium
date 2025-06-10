// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.net;

final class ScopedSystemProperty implements AutoCloseable {
    private final String mKey;
    private final String mValue;
    private final String mOriginalValue;

    public ScopedSystemProperty(String key, String value) {
        mKey = key;
        mValue = value;
        mOriginalValue = System.getProperty(key);
        if (value != null) {
            System.setProperty(key, value);
        } else {
            System.clearProperty(mKey);
        }
    }

    @Override
    public void close() {
        var value = System.getProperty(mKey);
        if ((value == null) != (mValue == null) || (value != null && !value.equals(mValue))) {
            throw new IllegalStateException(
                    "Expected value of system property `"
                            + mKey
                            + "` to be "
                            + (mValue == null ? "(null)" : "`" + mValue + "`")
                            + ", got "
                            + (value == null ? "(null)" : "`" + value + "`")
                            + " instead");
        }

        if (mOriginalValue != null) {
            System.setProperty(mKey, mOriginalValue);
        } else {
            System.clearProperty(mKey);
        }
    }
}
