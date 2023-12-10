// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.components.segmentation_platform;

import org.jni_zero.CalledByNative;
import org.jni_zero.JNINamespace;
import org.jni_zero.NativeMethods;

import org.chromium.base.Callback;

/**
 * Java side of the JNI bridge between SegmentationPlatformServiceImpl in Java
 * and C++. All method calls are delegated to the native C++ class.
 */
@JNINamespace("segmentation_platform")
public class SegmentationPlatformServiceImpl implements SegmentationPlatformService {
    private long mNativePtr;

    @CalledByNative
    private static SegmentationPlatformServiceImpl create(long nativePtr) {
        return new SegmentationPlatformServiceImpl(nativePtr);
    }

    private SegmentationPlatformServiceImpl(long nativePtr) {
        mNativePtr = nativePtr;
    }

    @Override
    public void getSelectedSegment(
            String segmentationKey, Callback<SegmentSelectionResult> callback) {
        SegmentationPlatformServiceImplJni.get()
                .getSelectedSegment(mNativePtr, this, segmentationKey, callback);
    }

    @Override
    public void getClassificationResult(
            String segmentationKey,
            PredictionOptions predictionOptions,
            InputContext inputContext,
            Callback<ClassificationResult> callback) {
        SegmentationPlatformServiceImplJni.get()
                .getClassificationResult(
                        mNativePtr,
                        this,
                        segmentationKey,
                        predictionOptions,
                        inputContext,
                        callback);
    }

    @Override
    public SegmentSelectionResult getCachedSegmentResult(String segmentationKey) {
        return SegmentationPlatformServiceImplJni.get()
                .getCachedSegmentResult(mNativePtr, this, segmentationKey);
    }

    @CalledByNative
    private void clearNativePtr() {
        mNativePtr = 0;
    }

    @NativeMethods
    interface Natives {
        void getSelectedSegment(
                long nativeSegmentationPlatformServiceAndroid,
                SegmentationPlatformServiceImpl caller,
                String segmentationKey,
                Callback<SegmentSelectionResult> callback);

        void getClassificationResult(
                long nativeSegmentationPlatformServiceAndroid,
                SegmentationPlatformServiceImpl caller,
                String segmentationKey,
                PredictionOptions predictionOptions,
                InputContext inputContext,
                Callback<ClassificationResult> callback);

        SegmentSelectionResult getCachedSegmentResult(
                long nativeSegmentationPlatformServiceAndroid,
                SegmentationPlatformServiceImpl caller,
                String segmentationKey);
    }
}
