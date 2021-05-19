// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.components.content_creation.notes.models;

/**
 * Enum with values corresponding to the C++ TextAlignment enum class.
 */
public enum TextAlignment {
    INVALID,
    START,
    CENTER,
    END;
    public static TextAlignment fromInteger(int x) {
        switch (x) {
            case 1:
                return START;
            case 2:
                return CENTER;
            case 3:
                return END;
        }
        return INVALID;
    }
}