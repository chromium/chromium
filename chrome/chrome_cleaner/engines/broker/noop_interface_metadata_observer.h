// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_CHROME_CLEANER_ENGINES_BROKER_NOOP_INTERFACE_METADATA_OBSERVER_H_
#define CHROME_CHROME_CLEANER_ENGINES_BROKER_NOOP_INTERFACE_METADATA_OBSERVER_H_

#include <map>
#include <string>

#include "base/macros.h"
#include "chrome/chrome_cleaner/engines/broker/interface_metadata_observer.h"

namespace chrome_cleaner {

class NoOpInterfaceMetadataObserver : public InterfaceMetadataObserver {
 public:
  NoOpInterfaceMetadataObserver();
  ~NoOpInterfaceMetadataObserver() override;

  // InterfaceMetadataObserver

  void ObserveCall(const LogInformation& log_information,
                   const std::map<std::string, std::string>& params) override;
  void ObserveCall(const LogInformation& log_information) override;

  DISALLOW_COPY_AND_ASSIGN(NoOpInterfaceMetadataObserver);
};

}  // namespace chrome_cleaner

#endif  // CHROME_CHROME_CLEANER_ENGINES_BROKER_NOOP_INTERFACE_METADATA_OBSERVER_H_
