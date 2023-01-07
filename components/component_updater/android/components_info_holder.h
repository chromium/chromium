// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_COMPONENT_UPDATER_ANDROID_COMPONENTS_INFO_HOLDER_H_
#define COMPONENTS_COMPONENT_UPDATER_ANDROID_COMPONENTS_INFO_HOLDER_H_

#include <string>
#include <vector>

#include "base/containers/flat_map.h"
#include "base/no_destructor.h"
#include "base/sequence_checker.h"

namespace base {
class Version;
}  // namespace base

namespace component_updater {

struct ComponentInfo;

// A singleton class that holds information about successfully loaded
// components in the system.
class ComponentsInfoHolder {
 public:
  ComponentsInfoHolder();
  ~ComponentsInfoHolder();

  ComponentsInfoHolder(const ComponentsInfoHolder&) = delete;
  ComponentsInfoHolder& operator=(const ComponentsInfoHolder&) = delete;

  static ComponentsInfoHolder* GetInstance();

  void AddComponent(const std::string& component_id,
                    const base::Version& version);

  std::vector<ComponentInfo> GetComponents() const;

 private:
  friend class base::NoDestructor<ComponentsInfoHolder>;

  SEQUENCE_CHECKER(sequence_checker_);

  base::flat_map<std::string, base::Version> components_;
};

}  // namespace component_updater

#endif  // COMPONENTS_COMPONENT_UPDATER_ANDROID_COMPONENTS_INFO_HOLDER_H_
