// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_SERVICES_APP_SERVICE_PUBLIC_CPP_INSTANCE_UPDATE_H_
#define COMPONENTS_SERVICES_APP_SERVICE_PUBLIC_CPP_INSTANCE_UPDATE_H_

#include <string>

#include "base/memory/raw_ptr.h"
#include "base/time/time.h"
#include "components/services/app_service/public/cpp/instance.h"

namespace apps {

class Instance;

// Wraps two Instance's, a prior state and a delta on top of that state. The
// state is conceptually the "sum" of all of the previous deltas, with
// "addition" or "merging" simply being that the most recent version of each
// field "wins".
//
// The state may be nullptr, meaning that there are no previous deltas.
// Alternatively, the delta may be nullptr, meaning that there is no change in
// state. At least one of state and delta must be non-nullptr.
//
// The combination of the two (state and delta) can answer questions such as:
//  - What is the app's launch_id? If the delta knows, that's the answer.
//  Otherwise, ask the state.
//  - Is the app launched? Likewise, if the delta says yes or no, that's the
//  answer. Otherwise, the delta says "unknown", ask the state.
//
// An InstanceUpdate is read-only once constructed. All of its fields and
// methods are const. The constructor caller must guarantee that the Instance
// pointer remain valid for the lifetime of the AppUpdate.
class InstanceUpdate {
 public:
  // Modifies |state| by copying over all of |delta|'s known fields: those
  // fields whose values aren't "unknown" or invalid. The |state| may not be
  // nullptr.
  static void Merge(Instance* state, const Instance* delta);

  // Returns true if |state| exists and is equal to |delta|, or |delta| are
  // nullptr. Return false otherwise.
  static bool Equals(const Instance* state, const Instance* delta);

  // At most one of |state| or |delta| may be nullptr.
  InstanceUpdate(const Instance* state, const Instance* delta);

  InstanceUpdate(const InstanceUpdate&) = delete;
  InstanceUpdate& operator=(const InstanceUpdate&) = delete;

  // Returns whether this is the first update for the given window.
  // Equivalently, there are no previous deltas for the window.
  bool StateIsNull() const;

  // Returns true if this update represents the creation of an instance, which
  // will now be visible to the user.
  bool IsCreation() const;

  // Returns true if this update represents the destruction of an instance.
  bool IsDestruction() const;

  const std::string& AppId() const;

  const base::UnguessableToken& InstanceId() const;

  aura::Window* Window() const;
  bool WindowChanged() const;

  const std::string& LaunchId() const;
  bool LaunchIdChanged() const;

  InstanceState State() const;
  bool StateChanged() const;

  base::Time LastUpdatedTime() const;
  bool LastUpdatedTimeChanged() const;

  content::BrowserContext* BrowserContext() const;
  bool BrowserContextChanged() const;

 private:
  raw_ptr<const Instance> state_;
  raw_ptr<const Instance> delta_;
};

}  // namespace apps

#endif  // COMPONENTS_SERVICES_APP_SERVICE_PUBLIC_CPP_INSTANCE_UPDATE_H_
