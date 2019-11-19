// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_CHROME_CLEANER_ENGINES_BROKER_INTERFACE_METADATA_OBSERVER_H_
#define CHROME_CHROME_CLEANER_ENGINES_BROKER_INTERFACE_METADATA_OBSERVER_H_

#include <map>
#include <string>

namespace chrome_cleaner {

struct LogInformation {
  std::string function_name;
  std::string file_name;
};

class InterfaceMetadataObserver {
 public:
  InterfaceMetadataObserver() = default;
  virtual ~InterfaceMetadataObserver() = default;

  // Logs a call to |function_name| from the given |class_name| and also logs
  // the passed parameters recorded on |params|.
  virtual void ObserveCall(
      const LogInformation& log_information,
      const std::map<std::string, std::string>& params) = 0;

  // Logs a call to |function_name| without parameters.
  virtual void ObserveCall(const LogInformation& log_information) = 0;
};

// Define a macro to make the use of ObserveCall easier.
#define CURRENT_FILE_AND_METHOD \
  LogInformation { __func__, __FILE__ }

}  // namespace chrome_cleaner

#endif  // CHROME_CHROME_CLEANER_ENGINES_BROKER_INTERFACE_METADATA_OBSERVER_H_
