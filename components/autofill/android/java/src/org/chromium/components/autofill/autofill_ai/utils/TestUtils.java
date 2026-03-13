// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.components.autofill.autofill_ai.utils;

import org.chromium.components.autofill.FieldType;
import org.chromium.components.autofill.autofill_ai.AttributeType;
import org.chromium.components.autofill.autofill_ai.AttributeTypeName;
import org.chromium.components.autofill.autofill_ai.DataType;
import org.chromium.components.autofill.autofill_ai.EntityInstance;
import org.chromium.components.autofill.autofill_ai.EntityInstanceWithLabels;
import org.chromium.components.autofill.autofill_ai.EntityType;
import org.chromium.components.autofill.autofill_ai.EntityTypeName;

import java.time.LocalDate;
import java.time.ZoneId;
import java.util.Collections;
import java.util.List;
import java.util.UUID;

public final class TestUtils {

    /** Prevents instantiation. */
    private TestUtils() {}

    public static EntityType getVehicleEntityType(boolean isReadOnly, boolean isEnabled) {
        return new EntityType(
                EntityTypeName.VEHICLE,
                isReadOnly,
                isEnabled,
                /* typeNameAsString= */ "Vehicle",
                /* typeNameAsMetricsString= */ "Vehicle",
                /* addEntityTypeString= */ "Add Vehicle",
                /* editEntityTypeString= */ "Edit Vehicle",
                /* deleteEntityTypeString= */ "Delete Vehicle",
                /* attributeTypes= */ Collections.emptyList(),
                /* requiredAttributes= */ Collections.emptyList());
    }

    public static EntityType getVehicleEntityType() {
        return getVehicleEntityType(/* isReadOnly= */ false, /* isEnabled= */ true);
    }

    public static AttributeType getPassportNameAttributeType() {
        return new AttributeType(
                /* typeName= */ AttributeTypeName.PASSPORT_NAME,
                /* typeNameAsString= */ "Passport name",
                /* dataType= */ DataType.NAME,
                /* fieldType= */ FieldType.NAME_FULL);
    }

    public static AttributeType getPassportCountryAttributeType() {
        return new AttributeType(
                /* typeName= */ AttributeTypeName.PASSPORT_COUNTRY,
                /* typeNameAsString= */ "Passport country",
                /* dataType= */ DataType.COUNTRY,
                /* fieldType= */ FieldType.PASSPORT_ISSUING_COUNTRY);
    }

    public static AttributeType getPassportNumberAttributeType() {
        return new AttributeType(
                /* typeName= */ AttributeTypeName.PASSPORT_NUMBER,
                /* typeNameAsString= */ "Passport number",
                /* dataType= */ DataType.STRING,
                /* fieldType= */ FieldType.PASSPORT_NUMBER);
    }

    public static AttributeType getPassportIssueDateAttributeType() {
        return new AttributeType(
                /* typeName= */ AttributeTypeName.PASSPORT_ISSUE_DATE,
                /* typeNameAsString= */ "Issue date",
                /* dataType= */ DataType.DATE,
                /* fieldType= */ FieldType.PASSPORT_ISSUE_DATE);
    }

    public static AttributeType getPassportExpirationDateAttributeType() {
        return new AttributeType(
                /* typeName= */ AttributeTypeName.PASSPORT_EXPIRATION_DATE,
                /* typeNameAsString= */ "Expiration date",
                /* dataType= */ DataType.DATE,
                /* fieldType= */ FieldType.PASSPORT_EXPIRATION_DATE);
    }

    public static EntityType getPassportEntityType(boolean isReadOnly, boolean isEnabled) {
        return new EntityType(
                EntityTypeName.PASSPORT,
                isReadOnly,
                isEnabled,
                /* typeNameAsString= */ "Passport",
                /* typeNameAsMetricsString= */ "Passport",
                /* addEntityTypeString= */ "Add passport",
                /* editEntityTypeString= */ "Edit passport",
                /* deleteEntityTypeString= */ "Delete passport",
                /* attributeTypes= */ List.of(
                        getPassportNameAttributeType(),
                        getPassportCountryAttributeType(),
                        getPassportNumberAttributeType(),
                        getPassportIssueDateAttributeType(),
                        getPassportExpirationDateAttributeType()),
                /* requiredAttributes= */ List.of(getPassportNumberAttributeType()));
    }

    public static EntityType getPassportEntityType() {
        return getPassportEntityType(/* isReadOnly= */ false, /* isEnabled= */ true);
    }

    public static EntityType getNationalIdEntityType(boolean isReadOnly, boolean isEnabled) {
        return new EntityType(
                EntityTypeName.NATIONAL_ID_CARD,
                isReadOnly,
                isEnabled,
                /* typeNameAsString= */ "National ID",
                /* typeNameAsMetricsString= */ "NationalId",
                /* addEntityTypeString= */ "Add National ID",
                /* editEntityTypeString= */ "Edit National ID",
                /* deleteEntityTypeString= */ "Delete National ID",
                /* attributeTypes= */ Collections.emptyList(),
                /* requiredAttributes= */ Collections.emptyList());
    }

    public static EntityType getNationalIdEntityType() {
        return getNationalIdEntityType(/* isReadOnly= */ false, /* isEnabled= */ true);
    }

    public static EntityInstanceWithLabels buildEntityInstanceWithLabels(
            EntityType entityType, String label, String subLabel) {
        EntityInstance entityInstance =
                new EntityInstance.Builder(entityType)
                        .setGUID(UUID.randomUUID().toString())
                        .setModifiedDate(LocalDate.now(ZoneId.systemDefault()))
                        .setUseCount(0)
                        .build();
        return new EntityInstanceWithLabels(
                entityInstance.getGUID(), entityType, label, subLabel, /* storedInWallet= */ true);
    }
}
