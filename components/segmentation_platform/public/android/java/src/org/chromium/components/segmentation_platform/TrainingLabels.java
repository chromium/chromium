// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.components.segmentation_platform;

import org.jni_zero.CalledByNative;
import org.jni_zero.JNINamespace;
import org.jni_zero.JniType;

import org.chromium.build.annotations.NullMarked;

@JNINamespace("segmentation_platform")
@NullMarked
public class TrainingLabels {
    public final String outputMetricName;
    public final int outputMetricSample;

    public TrainingLabels(String outputMetricName, int outputMetricSample) {
        this.outputMetricName = outputMetricName;
        this.outputMetricSample = outputMetricSample;
    }

    @CalledByNative
    @JniType("std::string")
    String getOutputMetricName() {
        return outputMetricName;
    }

    @CalledByNative
    int getOutputMetricSample() {
        return outputMetricSample;
    }
}
