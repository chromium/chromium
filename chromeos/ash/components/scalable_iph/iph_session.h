// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROMEOS_ASH_COMPONENTS_SCALABLE_IPH_IPH_SESSION_H_
#define CHROMEOS_ASH_COMPONENTS_SCALABLE_IPH_IPH_SESSION_H_

#include "base/feature_list.h"
#include "base/memory/raw_ref.h"
#include "chromeos/ash/components/scalable_iph/scalable_iph_constants.h"
#include "components/feature_engagement/public/tracker.h"

namespace scalable_iph {

// `IphSession` manages a single IPH session. An IPH UI is responsible to
// destroy this object once it stops showing the IPH. UI code should interact
// with ScalableIph framework via `IphSession` object which is passed from the
// service. UI code should not query/interact directly with `ScalableIph` keyed
// service.
class IphSession {
 public:
  class Delegate {
   public:
    virtual void PerformActionForIphSession(ActionType action_type) = 0;
  };

  IphSession(const base::Feature& feature,
             feature_engagement::Tracker* tracker,
             Delegate* delegate);
  ~IphSession();

  IphSession(const IphSession& iph_session) = delete;
  IphSession& operator=(const IphSession& iph_session) = delete;

  // Perform `action_type` as a result of a user action. This records a
  // corresponding IPH `event_name` to the feature engagement framework.
  void PerformAction(ActionType action_type, const std::string& event_name);

 private:
  // This is an IPH feature which is tied to this IPH session. See
  // //components/feature_engagement/README.md for details about an IPH feature.
  const raw_ref<const base::Feature> feature_;
  const raw_ptr<feature_engagement::Tracker> tracker_;
  const raw_ptr<Delegate> delegate_;
};

}  // namespace scalable_iph

#endif  // CHROMEOS_ASH_COMPONENTS_SCALABLE_IPH_IPH_SESSION_H_
