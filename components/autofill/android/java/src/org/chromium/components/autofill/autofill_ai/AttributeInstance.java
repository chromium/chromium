// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.components.autofill.autofill_ai;

import android.text.TextUtils;

import org.jni_zero.CalledByNative;
import org.jni_zero.JNINamespace;
import org.jni_zero.JniType;

import org.chromium.build.annotations.NullMarked;
import org.chromium.build.annotations.Nullable;

import java.time.LocalDate;
import java.time.format.DateTimeParseException;
import java.util.Objects;

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

        @Override
        public boolean equals(Object o) {
            if (this == o) return true;
            if (!(o instanceof StringValue)) return false;
            StringValue that = (StringValue) o;
            return Objects.equals(mValue, that.mValue);
        }

        @Override
        public int hashCode() {
            return Objects.hash(mValue);
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
                                Integer.parseInt(year),
                                Integer.parseInt(month),
                                Integer.parseInt(day));
            }
        }

        public DateValue(String date) {
            mDate = parseDate(date);
        }

        private @Nullable LocalDate parseDate(String date) {
            if (TextUtils.isEmpty(date)) {
                return null;
            }
            try {
                return LocalDate.parse(date);
            } catch (DateTimeParseException e) {
                assert false : "Invalid date format: " + date;
                return null;
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

        public @Nullable LocalDate getDate() {
            return mDate;
        }

        @Override
        public String toString() {
            return mDate == null ? "" : mDate.toString();
        }

        @Override
        public boolean equals(Object o) {
            if (this == o) return true;
            if (!(o instanceof DateValue)) return false;
            DateValue that = (DateValue) o;
            return Objects.equals(mDate, that.mDate);
        }

        @Override
        public int hashCode() {
            return Objects.hash(mDate);
        }
    }

    private final AttributeType mAttributeType;
    private final AttributeValue mAttributeValue;

    @CalledByNative
    public AttributeInstance(
            AttributeType attributeType,
            @JniType("std::u16string") String day,
            @JniType("std::u16string") String month,
            @JniType("std::u16string") String year) {
        this(attributeType, new DateValue(day, month, year));
    }

    @CalledByNative
    public AttributeInstance(AttributeType attributeType, @JniType("std::u16string") String value) {
        this(attributeType, new StringValue(value));
    }

    public AttributeInstance(AttributeType attributeType, AttributeValue attributeValue) {
        mAttributeType = attributeType;
        mAttributeValue = attributeValue;
    }

    @CalledByNative
    public AttributeType getAttributeType() {
        return mAttributeType;
    }

    public AttributeValue getAttributeValue() {
        return mAttributeValue;
    }

    @CalledByNative
    public boolean isDateType() {
        return mAttributeValue instanceof DateValue;
    }
}
