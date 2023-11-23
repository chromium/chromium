// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.components.ukm;

import androidx.annotation.VisibleForTesting;

import org.jni_zero.JNINamespace;
import org.jni_zero.NativeMethods;

import org.chromium.content_public.browser.WebContents;

/** An interface and classes to record User Keyed Metrics. */
public abstract class UkmRecorder {
    /**
     * Records the occurrence of a (boolean) UKM event with name |eventName|.
     * A UKM entry with |eventName| must be present in ukm.xml with a metric that matches
     * |metricName| For example, <event name="SomeFeature.SomeComponent">
     * <owner>owner@chromium.org</owner>
     * <summary>
     * User triggered a specific feature.
     * </summary>
     * <metric name="SomeMetricName" enum="Boolean">
     * <summary>
     * A boolean signaling that the event has occurred (typically only records
     *  true values).
     * </summary>
     * </metric>
     * </event>
     */
    abstract void recordEventWithBooleanMetric(
            WebContents webContents, String eventName, String metricName);

    /**
     * Records the occurrence of an (integer) UKM event with name |eventName|.
     * A UKM entry with |eventName| must be present in ukm.xml with a metric that matches
     * |metricName| For example, <event name="SomeFeature.SomeComponent">
     * <owner>owner@chromium.org</owner>
     * <summary>
     * User triggered a specific feature.
     * </summary>
     * <metric name="SomeMetricName">
     * <summary>
     * An integer signaling the type of even that occurred.
     * </summary>
     * </metric>
     * </event>
     */
    abstract void recordEventWithIntegerMetric(
            WebContents webContents, String eventName, String metricName, int metricValue);

    /** The actual recorder. */
    @JNINamespace("metrics")
    public static class Bridge extends UkmRecorder {
        @Override
        public void recordEventWithBooleanMetric(
                WebContents webContents, String eventName, String metricName) {
            UkmRecorderJni.get().recordEventWithBooleanMetric(webContents, eventName, metricName);
        }

        @Override
        public void recordEventWithIntegerMetric(
                WebContents webContents, String eventName, String metricName, int metricValue) {
            UkmRecorderJni.get()
                    .recordEventWithIntegerMetric(webContents, eventName, metricName, metricValue);
        }
    }

    @NativeMethods
    @VisibleForTesting(otherwise = VisibleForTesting.PACKAGE_PRIVATE)
    public interface Natives {
        void recordEventWithBooleanMetric(
                WebContents webContents, String eventName, String metricName);

        void recordEventWithIntegerMetric(
                WebContents webContents, String eventName, String metricName, int metricValue);
    }
}
