// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_CHROME_CLEANER_ENGINES_BROKER_NOOP_INTERFACE_METADATA_OBSERVER_H_
#define CHROME_CHROME_CLEANER_ENGINES_BROKER_NOOP_INTERFACE_METADATA_OBSERVER_H_

#include <map>
#include <string>

#include "chrome/chrome_cleaner/engines/broker/interface_metadata_observer.h"

namespace chrome_cleaner {

class NoOpInterfaceMetadataObserver : public InterfaceMetadataObserver {
 public:
  NoOpInterfaceMetadataObserver();

  NoOpInterfaceMetadataObserver(const NoOpInterfaceMetadataObserver&) = delete;
  NoOpInterfaceMetadataObserver& operator=(
      const NoOpInterfaceMetadataObserver&) = delete;

  ~NoOpInterfaceMetadataObserver() override;

  // InterfaceMetadataObserver

  void ObserveCall(const LogInformation& log_information,
                   const std::map<std::string, std::string>& params) override;
  void ObserveCall(const LogInformation& log_information) override;
};

}  // namespace chrome_cleaner

#endif  // CHROME_CHROME_CLEANER_ENGINES_BROKER_NOOP_INTERFACE_METADATA_OBSERVER_H_
