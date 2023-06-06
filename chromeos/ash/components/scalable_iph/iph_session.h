// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROMEOS_ASH_COMPONENTS_SCALABLE_IPH_IPH_SESSION_H_
#define CHROMEOS_ASH_COMPONENTS_SCALABLE_IPH_IPH_SESSION_H_

#include "base/feature_list.h"
#include "components/feature_engagement/public/tracker.h"

namespace scalable_iph {

// `IphSession` manages a single IPH session. An IPH UI is responsible to
// destroy this object once it stops showing the IPH.
class IphSession {
 public:
  IphSession(const base::Feature& feature,
             feature_engagement::Tracker* tracker);
  ~IphSession();

  IphSession(const IphSession& iph_session) = delete;
  IphSession& operator=(const IphSession& iph_session) = delete;

 private:
  // This is an IPH feature which is tied to this IPH session. See
  // //components/feature_engagement/README.md for details about an IPH feature.
  const base::Feature& feature_;
  const raw_ptr<feature_engagement::Tracker> tracker_;
};

}  // namespace scalable_iph

#endif  // CHROMEOS_ASH_COMPONENTS_SCALABLE_IPH_IPH_SESSION_H_
