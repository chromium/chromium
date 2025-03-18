// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.components.autofill;

import org.jni_zero.CalledByNative;
import org.jni_zero.JNINamespace;
import org.jni_zero.JniType;

import org.chromium.build.annotations.NullMarked;

/** Description of an address editor input field. */
@NullMarked
@JNINamespace("autofill")
public class AutofillAddressUiComponent {
    /** The type of the field, e.g., {@code FieldType.NAME_FULL}. */
    public final @FieldType int id;

    /** The localized display label for the field, e.g., "City." */
    public final String label;

    /** Whether the field is required. */
    public final boolean isRequired;

    /** Whether the field takes up the full line. */
    public final boolean isFullLine;

    /**
     * Builds a description of an address editor input field.
     *
     * @param id The type of the field, .e.g., FieldType.ADDRESS_HOME_CITY.
     * @param label The localized display label for the field, .e.g., "City."
     * @param isRequired Whether the field is required.
     * @param isFullLine Whether the field takes up the full line.
     */
    @CalledByNative
    public AutofillAddressUiComponent(
            int id, @JniType("std::string") String label, boolean isRequired, boolean isFullLine) {
        this.id = id;
        this.label = label;
        this.isRequired = isRequired;
        this.isFullLine = isFullLine;
    }

    @CalledByNative
    private @FieldType int getFieldType() {
        return id;
    }

    @CalledByNative
    private @JniType("std::string") String getLabel() {
        return label;
    }

    @CalledByNative
    private boolean isRequired() {
        return isRequired;
    }

    @CalledByNative
    private boolean isFullLine() {
        return isFullLine;
    }
}
