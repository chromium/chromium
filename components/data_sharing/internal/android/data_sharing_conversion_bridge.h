// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_DATA_SHARING_INTERNAL_ANDROID_DATA_SHARING_CONVERSION_BRIDGE_H_
#define COMPONENTS_DATA_SHARING_INTERNAL_ANDROID_DATA_SHARING_CONVERSION_BRIDGE_H_

#include "base/android/jni_android.h"
#include "base/android/scoped_java_ref.h"
#include "base/memory/raw_ptr.h"
#include "base/memory/scoped_refptr.h"
#include "components/data_sharing/public/data_sharing_service.h"
#include "components/data_sharing/public/group_data.h"
#include "components/data_sharing/public/service_status.h"

using base::android::ScopedJavaLocalRef;

namespace data_sharing {

// Utility for JNI conversion of the data types used by the service.
class DataSharingConversionBridge {
 public:
  // Creates an object of
  // org.chromium.components.data_sharing.DataSharingService.
  //   GroupDataOrFailureOutcome.
  static ScopedJavaLocalRef<jobject> CreateGroupDataOrFailureOutcome(
      JNIEnv* env,
      const DataSharingService::GroupDataOrFailureOutcome& data);

  // Creates an object of
  // org.chromium.components.data_sharing.DataSharingService.
  //   GroupsDataSetOrFailureOutcome.
  static ScopedJavaLocalRef<jobject> CreateGroupDataSetOrFailureOutcome(
      JNIEnv* env,
      const DataSharingService::GroupsDataSetOrFailureOutcome& data);

  // Creates an Integer object that identifies the generated enum
  // PeopleGroupActionOutcome. The object is useful since Java Callback does not
  // support primitive type int.
  static ScopedJavaLocalRef<jobject> CreatePeopleGroupActionOutcome(JNIEnv* env,
                                                                    int value);

  // Creates an object of
  // org.chromium.components.data_sharing.DataSharingService.ParseURLResult.
  static ScopedJavaLocalRef<jobject> CreateParseURLResult(
      JNIEnv* env,
      const DataSharingService::ParseURLResult& data);

  // Creates an object of
  // org.chromium.components.data_sharing.DataSharingService.
  //   SharedDataPreviewOrFailureOutcome.
  static ScopedJavaLocalRef<jobject> CreateSharedDataPreviewOrFailureOutcome(
      JNIEnv* env,
      const DataSharingService::SharedDataPreviewOrFailureOutcome& data);
};

}  // namespace data_sharing

#endif  // COMPONENTS_DATA_SHARING_INTERNAL_ANDROID_DATA_SHARING_CONVERSION_BRIDGE_H_
