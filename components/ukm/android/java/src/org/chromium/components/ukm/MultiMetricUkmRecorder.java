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

/** Class for recording UKM events with multiple metrics. */
@JNINamespace("metrics")
public class MultiMetricUkmRecorder {
    private WebContents mWebContents;
    private String mEventName;
    private List<Metric> mMetrics;

    static class Metric {
        public String mName;
        public int mValue;

        public Metric(String name, int value) {
            mName = name;
            mValue = value;
        }
    }

    private MultiMetricUkmRecorder(
            WebContents webContents, String eventName, List<Metric> metrics) {
        this.mWebContents = webContents;
        this.mEventName = eventName;
        this.mMetrics = metrics;
    }

    public void record() {
        Metric[] metricsArray = mMetrics.toArray(new Metric[mMetrics.size()]);
        MultiMetricUkmRecorderJni.get()
                .recordEventWithMultipleMetrics(mWebContents, mEventName, metricsArray);
    }

    public static final class Builder {
        private WebContents mWebContents;
        private String mEventName;
        private List<Metric> mMetrics;

        public Builder setEventName(String eventName) {
            assert mEventName == null;
            mEventName = eventName;
            return this;
        }

        public Builder setWebContents(WebContents webContents) {
            assert mWebContents == null;
            mWebContents = webContents;
            return this;
        }

        public Builder addMetric(String name, int value) {
            if (mMetrics == null) {
                mMetrics = new ArrayList<Metric>();
            }
            mMetrics.add(new Metric(name, value));
            return this;
        }

        public MultiMetricUkmRecorder build() {
            assert mWebContents != null : "UKM recording requires a WebContents";
            assert mEventName != null : "UKM recording requires an event name";
            assert mMetrics != null && mMetrics.size() > 0
                    : "cannot record UKM event with no metrics";
            return new MultiMetricUkmRecorder(mWebContents, mEventName, mMetrics);
        }
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
