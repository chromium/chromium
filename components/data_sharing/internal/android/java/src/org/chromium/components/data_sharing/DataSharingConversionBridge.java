// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.components.data_sharing;

import org.jni_zero.CalledByNative;
import org.jni_zero.JNINamespace;

import java.util.List;

/** Conversion utility for misc types needed in the JNI layer for the service. */
@JNINamespace("data_sharing")
public class DataSharingConversionBridge {

    @CalledByNative
    private static DataSharingService.GroupDataOrFailureOutcome createGroupDataOrFailureOutcome(
            GroupData groupData, int failureOutcome) {
        return new DataSharingService.GroupDataOrFailureOutcome(groupData, failureOutcome);
    }

    @CalledByNative
    private static DataSharingService.GroupsDataSetOrFailureOutcome
            createGroupDataSetOrFailureOutcome(GroupData[] groupDataSet, int failureOutcome) {
        List<GroupData> list = groupDataSet == null ? null : List.of(groupDataSet);
        return new DataSharingService.GroupsDataSetOrFailureOutcome(list, failureOutcome);
    }

    @CalledByNative
    public static Integer createPeopleGroupActionOutcome(int value) {
        return value;
    }

    @CalledByNative
    public static DataSharingService.ParseURLResult createParseURLResult(
            GroupToken groupToken, int status) {
        return new DataSharingService.ParseURLResult(groupToken, status);
    }

    @CalledByNative
    public static DataSharingService.SharedDataPreviewOrFailureOutcome
            createSharedDataPreviewOrFailureOutcome(SharedEntity[] sharedEntities, int status) {
        return new DataSharingService.SharedDataPreviewOrFailureOutcome(
                new SharedDataPreview(sharedEntities), status);
    }
}
