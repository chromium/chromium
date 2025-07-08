// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_COLLABORATION_PUBLIC_COMMENTS_COMMENTS_SERVICE_H_
#define COMPONENTS_COLLABORATION_PUBLIC_COMMENTS_COMMENTS_SERVICE_H_

#include "base/supports_user_data.h"
#include "components/keyed_service/core/keyed_service.h"

namespace collaboration::comments {

// This interface serves as the primary interaction point for the UI to query
// for and manage comments. It provides a purely asynchronous API for all data
// access to ensure the UI thread is never blocked.
class CommentsService : public KeyedService, public base::SupportsUserData {
 public:
  ~CommentsService() override = default;

  // Returns whether the service has fully initialized.
  virtual bool IsInitialized() const = 0;

  // Returns true if this is an empty implementation.
  virtual bool IsEmptyService() const = 0;
};

}  // namespace collaboration::comments

#endif  // COMPONENTS_COLLABORATION_PUBLIC_COMMENTS_COMMENTS_SERVICE_H_
