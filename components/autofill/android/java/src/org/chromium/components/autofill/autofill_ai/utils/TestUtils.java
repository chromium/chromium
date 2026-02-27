// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.components.autofill.autofill_ai.utils;

import org.chromium.components.autofill.autofill_ai.EntityType;
import org.chromium.components.autofill.autofill_ai.EntityTypeName;

import java.util.Collections;

public final class TestUtils {

    /** Prevents instantiation. */
    private TestUtils() {}

    public static EntityType getVehicleEntityType() {
        return new EntityType(
                EntityTypeName.VEHICLE,
                /* isReadOnly= */ false,
                /* typeNameAsString= */ "Vehicle",
                /* typeNameAsMetricsString= */ "Vehicle",
                /* addEntityTypeString= */ "Add Vehicle",
                /* editEntityTypeString= */ "Edit Vehicle",
                /* deleteEntityTypeString= */ "Delete Vehicle",
                /* attributeTypes= */ Collections.emptyList());
    }

    public static EntityType getPassportEntityType() {
        return new EntityType(
                EntityTypeName.PASSPORT,
                /* isReadOnly= */ false,
                /* typeNameAsString= */ "Passport",
                /* typeNameAsMetricsString= */ "Passport",
                /* addEntityTypeString= */ "Add passport",
                /* editEntityTypeString= */ "Edit passport",
                /* deleteEntityTypeString= */ "Delete passport",
                /* attributeTypes= */ Collections.emptyList());
    }
}
