// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.components.autofill.autofill_ai.utils;

import org.chromium.components.autofill.autofill_ai.EntityInstance;
import org.chromium.components.autofill.autofill_ai.EntityInstanceWithLabels;
import org.chromium.components.autofill.autofill_ai.EntityType;
import org.chromium.components.autofill.autofill_ai.EntityTypeName;

import java.time.LocalDate;
import java.time.ZoneId;
import java.util.Collections;
import java.util.UUID;

public final class TestUtils {

    /** Prevents instantiation. */
    private TestUtils() {}

    public static EntityType getVehicleEntityType(
            boolean isReadOnly, boolean isEnabled, boolean isEligibleForWalletStorage) {
        return new EntityType(
                EntityTypeName.VEHICLE,
                isReadOnly,
                isEnabled,
                isEligibleForWalletStorage,
                /* isMaskedStorageSupported= */ false,
                /* typeNameAsString= */ "Vehicle",
                /* typeNameAsMetricsString= */ "Vehicle",
                /* addEntityTypeString= */ "Add Vehicle",
                /* editEntityTypeString= */ "Edit Vehicle",
                /* deleteEntityTypeString= */ "Delete Vehicle",
                /* attributeTypes= */ Collections.emptyList(),
                /* requiredAttributes= */ Collections.emptyList());
    }

    public static EntityType getVehicleEntityType() {
        return getVehicleEntityType(
                /* isReadOnly= */ false,
                /* isEnabled= */ true,
                /* isEligibleForWalletStorage= */ false);
    }

    public static EntityType getPassportEntityType(
            boolean isReadOnly, boolean isEnabled, boolean isEligibleForWalletStorage) {
        return new EntityType(
                EntityTypeName.PASSPORT,
                isReadOnly,
                isEnabled,
                isEligibleForWalletStorage,
                /* isMaskedStorageSupported= */ true,
                /* typeNameAsString= */ "Passport",
                /* typeNameAsMetricsString= */ "Passport",
                /* addEntityTypeString= */ "Add passport",
                /* editEntityTypeString= */ "Edit passport",
                /* deleteEntityTypeString= */ "Delete passport",
                /* attributeTypes= */ Collections.emptyList(),
                /* requiredAttributes= */ Collections.emptyList());
    }

    public static EntityType getPassportEntityType() {
        return getPassportEntityType(
                /* isReadOnly= */ false,
                /* isEnabled= */ true,
                /* isEligibleForWalletStorage= */ false);
    }

    public static EntityType getNationalIdEntityType(
            boolean isReadOnly, boolean isEnabled, boolean isEligibleForWalletStorage) {
        return new EntityType(
                EntityTypeName.NATIONAL_ID_CARD,
                isReadOnly,
                isEnabled,
                isEligibleForWalletStorage,
                /* isMaskedStorageSupported= */ true,
                /* typeNameAsString= */ "National ID",
                /* typeNameAsMetricsString= */ "NationalId",
                /* addEntityTypeString= */ "Add National ID",
                /* editEntityTypeString= */ "Edit National ID",
                /* deleteEntityTypeString= */ "Delete National ID",
                /* attributeTypes= */ Collections.emptyList(),
                /* requiredAttributes= */ Collections.emptyList());
    }

    public static EntityType getNationalIdEntityType() {
        return getNationalIdEntityType(
                /* isReadOnly= */ false,
                /* isEnabled= */ true,
                /* isEligibleForWalletStorage= */ false);
    }

    public static EntityInstanceWithLabels buildEntityInstanceWithLabels(
            EntityType entityType, String label, String subLabel) {
        EntityInstance entityInstance =
                new EntityInstance.Builder(entityType)
                        .setGuid(UUID.randomUUID().toString())
                        .setModifiedDate(LocalDate.now(ZoneId.systemDefault()))
                        .setUseCount(0)
                        .build();
        return new EntityInstanceWithLabels(
                entityInstance.getGuid(),
                entityType,
                label,
                subLabel,
                /* storedInWallet= */ true,
                /* walletEntityUrl= */ null);
    }
}
