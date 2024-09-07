// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_DATA_SHARING_PUBLIC_DATA_SHARING_UI_DELEGATE_H_
#define COMPONENTS_DATA_SHARING_PUBLIC_DATA_SHARING_UI_DELEGATE_H_

#include "build/build_config.h"

#if BUILDFLAG(IS_ANDROID)
#include "base/android/jni_android.h"
#endif  // BUILDFLAG(IS_ANDROID)

class GURL;

namespace data_sharing {

// An interface for the data sharing service to communicate with UI elements.
class DataSharingUIDelegate {
 public:
  DataSharingUIDelegate() = default;
  virtual ~DataSharingUIDelegate() = default;

  // Handle the intercepted URL to show relevant data sharing group information.
  virtual void HandleShareURLIntercepted(const GURL& url) = 0;

#if BUILDFLAG(IS_ANDROID)
  // Returns a Java object of the type DataSharingService for the given
  // DataSharingService.
  virtual base::android::ScopedJavaLocalRef<jobject> GetJavaObject();
#endif  // BUILDFLAG(IS_ANDROID)
};

}  // namespace data_sharing

#endif  // COMPONENTS_DATA_SHARING_PUBLIC_DATA_SHARING_UI_DELEGATE_H_
