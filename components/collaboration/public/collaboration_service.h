// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_COLLABORATION_PUBLIC_COLLABORATION_SERVICE_H_
#define COMPONENTS_COLLABORATION_PUBLIC_COLLABORATION_SERVICE_H_

#include "base/supports_user_data.h"
#include "build/build_config.h"
#include "components/collaboration/public/collaboration_controller_delegate.h"
#include "components/keyed_service/core/keyed_service.h"
#include "components/saved_tab_groups/public/types.h"
#include "url/gurl.h"

#if BUILDFLAG(IS_ANDROID)
#include "base/android/jni_android.h"
#endif  // BUILDFLAG(IS_ANDROID)

namespace collaboration {

// The core class for managing collaboration group flows.
class CollaborationService : public KeyedService,
                             public base::SupportsUserData {
 public:
#if BUILDFLAG(IS_ANDROID)
  // Returns a Java object of the type CollaborationService for the given
  // CollaborationService.
  static base::android::ScopedJavaLocalRef<jobject> GetJavaObject(
      CollaborationService* collaboration_service);
#endif  // BUILDFLAG(IS_ANDROID)

  CollaborationService() = default;
  ~CollaborationService() override = default;

  // Disallow copy/assign.
  CollaborationService(const CollaborationService&) = delete;
  CollaborationService& operator=(const CollaborationService&) = delete;

  // Whether the service is an empty implementation. This is here because the
  // Chromium build disables RTTI, and we need to be able to verify that we are
  // using an empty service from the Chrome embedder.
  virtual bool IsEmptyService() = 0;

  // Starts a new join flow. This will cancel all existing ongoing join and
  // share flows in the same browser instance.
  virtual void StartJoinFlow(
      std::unique_ptr<CollaborationControllerDelegate> delegate,
      const GURL& url) = 0;

  // Starts a new share flow. This will cancel all existing ongoing join and
  // share flows in the same browser instance.
  virtual void StartShareFlow(
      std::unique_ptr<CollaborationControllerDelegate> delegate,
      tab_groups::EitherGroupID group_id) = 0;
};

}  // namespace collaboration

#endif  // COMPONENTS_COLLABORATION_PUBLIC_COLLABORATION_SERVICE_H_
