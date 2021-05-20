// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.components.content_creation.notes.models;

/**
 * Enum with values corresponding to the C++ LinearGradientDirection enum class.
 */
public enum LinearGradientDirection {
    INVALID,
    TOP_TO_BOTTOM,
    TOP_RIGHT_TO_BOTTOM_LEFT,
    RIGHT_TO_LEFT,
    BOTTOM_RIGHT_TO_TOP_LEFT;

    public static LinearGradientDirection fromInteger(int x) {
        switch (x) {
            case 1:
                return TOP_TO_BOTTOM;
            case 2:
                return TOP_RIGHT_TO_BOTTOM_LEFT;
            case 3:
                return RIGHT_TO_LEFT;
            case 4:
                return BOTTOM_RIGHT_TO_TOP_LEFT;
        }

        return INVALID;
    }
}