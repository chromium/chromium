// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.components.content_creation.notes.models;

import android.view.Gravity;

/** Enum with values corresponding to the C++ TextAlignment enum class. */
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

    public static int toGravity(TextAlignment alignment) {
        switch (alignment) {
                // Invalid will default to start.
            case INVALID:
            case START:
                return Gravity.START | Gravity.CENTER_VERTICAL;
            case CENTER:
                return Gravity.CENTER;
            case END:
                return Gravity.END | Gravity.CENTER_VERTICAL;
        }

        return Gravity.START;
    }
}
