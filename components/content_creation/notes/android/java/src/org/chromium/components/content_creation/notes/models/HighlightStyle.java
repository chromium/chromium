// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.components.content_creation.notes.models;

/**
 * Enum with values corresponding to the C++ HighlightStyle enum class.
 */
public enum HighlightStyle {
    NONE,
    FULL,
    HALF;

    public static HighlightStyle fromInteger(int x) {
        switch (x) {
            case 0:
                return NONE;
            case 1:
                return FULL;
            case 2:
                return HALF;
        }
        return NONE;
    }
}