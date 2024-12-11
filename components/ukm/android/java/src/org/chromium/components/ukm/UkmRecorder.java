// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.components.ukm;

import androidx.annotation.VisibleForTesting;

import org.jni_zero.CalledByNative;
import org.jni_zero.JNINamespace;
import org.jni_zero.NativeMethods;

import org.chromium.content_public.browser.WebContents;

import java.util.ArrayList;
import java.util.List;

/**
 * A class for the occurrence of a single UKM event. It is generated from its Builder class and must
 * have a non-null WebContents, an event name, and at least one metric. AddMetric and
 * AddBooleanMetric can be called any number of times to add metrics corresponding to the named
 * event in tools/metrics/ukm/ukm.xml.
 */
@JNINamespace("metrics")
public class UkmRecorder {
    private WebContents mWebContents;
    private String mEventName;
    private List<Metric> mMetrics;

    @VisibleForTesting(otherwise = VisibleForTesting.PACKAGE_PRIVATE)
    public static class Metric {
        public String mName;
        public int mValue;

        public Metric(String name, int value) {
            mName = name;
            mValue = value;
        }
    }

    public UkmRecorder(WebContents webContents, String eventName) {
        assert webContents != null : "UKM recording requires a WebContents";
        assert eventName != null && eventName.length() > 0 : "UKM recording requires an event name";
        mWebContents = webContents;
        mEventName = eventName;
    }

    public UkmRecorder addMetric(String name, int value) {
        if (mMetrics == null) {
            mMetrics = new ArrayList<Metric>();
        }
        mMetrics.add(new Metric(name, value));
        return this;
    }

    public UkmRecorder addBooleanMetric(String name) {
        if (mMetrics == null) {
            mMetrics = new ArrayList<Metric>();
        }
        // Boolean metrics have implied value "true".
        mMetrics.add(new Metric(name, 1));
        return this;
    }

    public void record() {
        if (mWebContents.isDestroyed()) {
            // https://crbug.com/356429588
            assert false;
            return;
        }
        Metric[] metricsArray = mMetrics.toArray(new Metric[mMetrics.size()]);
        UkmRecorderJni.get().recordEventWithMultipleMetrics(mWebContents, mEventName, metricsArray);
    }

    @CalledByNative
    private static String getNameFromMetric(Metric metric) {
        return metric.mName;
    }

    @CalledByNative
    private static int getValueFromMetric(Metric metric) {
        return metric.mValue;
    }

    @NativeMethods
    @VisibleForTesting(otherwise = VisibleForTesting.PACKAGE_PRIVATE)
    public interface Natives {
        void recordEventWithMultipleMetrics(
                WebContents webContents, String eventName, Metric[] metrics);
    }
}
