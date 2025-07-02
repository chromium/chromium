// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.components.segmentation_platform;

import org.jni_zero.CalledByNative;
import org.jni_zero.JNINamespace;
import org.jni_zero.NativeMethods;

import org.chromium.base.Callback;
import org.chromium.build.annotations.NullMarked;
import org.chromium.build.annotations.Nullable;

import java.util.Arrays;
import java.util.HashSet;
import java.util.Set;

/**
 * Java side of the JNI bridge between SegmentationPlatformServiceImpl in Java and C++. All method
 * calls are delegated to the native C++ class.
 */
@JNINamespace("segmentation_platform")
@NullMarked
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
                .getSelectedSegment(mNativePtr, segmentationKey, callback);
    }

    @Override
    public void getClassificationResult(
            String segmentationKey,
            PredictionOptions predictionOptions,
            @Nullable InputContext inputContext,
            Callback<ClassificationResult> callback) {
        SegmentationPlatformServiceImplJni.get()
                .getClassificationResult(
                        mNativePtr, segmentationKey, predictionOptions, inputContext, callback);
    }

    @Override
    public SegmentSelectionResult getCachedSegmentResult(String segmentationKey) {
        return SegmentationPlatformServiceImplJni.get()
                .getCachedSegmentResult(mNativePtr, segmentationKey);
    }

    @Override
    public void getInputKeysForModel(String segmentationKey, Callback<Set<String>> callback) {
        SegmentationPlatformServiceImplJni.get()
                .getInputKeysForModel(
                        mNativePtr,
                        segmentationKey,
                        (inputArray) -> {
                            callback.onResult(new HashSet<>(Arrays.asList(inputArray)));
                        });
    }

    @Override
    public void collectTrainingData(
            int segmentId,
            long requestId,
            long ukmSourceId,
            TrainingLabels param,
            Callback<Boolean> callback) {
        SegmentationPlatformServiceImplJni.get()
                .collectTrainingData(
                        mNativePtr, segmentId, requestId, ukmSourceId, param, callback);
    }

    @CalledByNative
    private void clearNativePtr() {
        mNativePtr = 0;
    }

    @NativeMethods
    interface Natives {
        void getSelectedSegment(
                long nativeSegmentationPlatformServiceAndroid,
                String segmentationKey,
                Callback<SegmentSelectionResult> callback);

        void getClassificationResult(
                long nativeSegmentationPlatformServiceAndroid,
                String segmentationKey,
                PredictionOptions predictionOptions,
                @Nullable InputContext inputContext,
                Callback<ClassificationResult> callback);

        SegmentSelectionResult getCachedSegmentResult(
                long nativeSegmentationPlatformServiceAndroid, String segmentationKey);

        void getInputKeysForModel(
                long nativeSegmentationPlatformServiceAndroid,
                String segmentationKey,
                Callback<String[]> callback);

        void collectTrainingData(
                long nativeSegmentationPlatformServiceAndroid,
                int segmentId,
                long requestId,
                long ukmSourceId,
                TrainingLabels param,
                Callback<Boolean> callback);
    }
}
