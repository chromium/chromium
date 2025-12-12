// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.components.autofill.autofill_ai;

import org.jni_zero.CalledByNative;
import org.jni_zero.JNINamespace;
import org.jni_zero.JniType;

import org.chromium.build.annotations.NullMarked;
import org.chromium.build.annotations.Nullable;

import java.time.LocalDate;

/** Represents information of an Autofill AI attribute instance. */
@JNINamespace("autofill")
@NullMarked
public class AttributeInstance {
    /**
     * Entity attributes hold either a raw string value (such as passport numbers) or date values.
     */
    public sealed interface AttributeValue permits StringValue, DateValue {}

    public static final class StringValue implements AttributeValue {
        private final String mValue;

        public StringValue(String value) {
            mValue = value;
        }

        @CalledByNative
        public @JniType("std::u16string") String getValue() {
            return mValue;
        }
    }

    public static final class DateValue implements AttributeValue {
        private final @Nullable LocalDate mDate;

        public DateValue(String day, String month, String year) {
            if (day.isEmpty() || month.isEmpty() || year.isEmpty()) {
                this.mDate = null;
            } else {
                this.mDate =
                        LocalDate.of(
                                Integer.parseInt(day),
                                Integer.parseInt(month),
                                Integer.parseInt(year));
            }
        }

        @CalledByNative
        public @JniType("std::u16string") String getDay() {
            return mDate != null ? Integer.toString(mDate.getDayOfMonth()) : "";
        }

        @CalledByNative
        public @JniType("std::u16string") String getMonth() {
            return mDate != null ? Integer.toString(mDate.getMonthValue()) : "";
        }

        @CalledByNative
        public @JniType("std::u16string") String getYear() {
            return mDate != null ? Integer.toString(mDate.getYear()) : "";
        }
    }

    public final AttributeType attributeType;
    public final @Nullable AttributeValue attributeValue;

    @CalledByNative
    public AttributeInstance(
            AttributeType attributeType,
            @JniType("std::u16string") String day,
            @JniType("std::u16string") String month,
            @JniType("std::u16string") String year) {
        this.attributeType = attributeType;
        this.attributeValue = new DateValue(day, month, year);
    }

    @CalledByNative
    public AttributeInstance(AttributeType attributeType, @JniType("std::u16string") String value) {
        this.attributeType = attributeType;
        this.attributeValue = new StringValue(value);
    }

    @CalledByNative
    public AttributeType getAttributeType() {
        return attributeType;
    }

    @CalledByNative
    public boolean isDateType() {
        return attributeValue instanceof DateValue;
    }
}
