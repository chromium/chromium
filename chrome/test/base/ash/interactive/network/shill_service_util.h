// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_TEST_BASE_ASH_INTERACTIVE_NETWORK_SHILL_SERVICE_UTIL_H_
#define CHROME_TEST_BASE_ASH_INTERACTIVE_NETWORK_SHILL_SERVICE_UTIL_H_

#include <string>

namespace ash {

// Helper class to simplify the definition of shill service constants.
class ShillServiceInfo {
 public:
  // The `id` parameter is used when generating values for each of the different
  // shill service constants below.
  explicit ShillServiceInfo(unsigned int id);
  ~ShillServiceInfo();

  const std::string& service_name() const { return service_name_; }
  const std::string& service_path() const { return service_path_; }
  const std::string& service_guid() const { return service_guid_; }

 private:
  const std::string service_name_;
  const std::string service_path_;
  const std::string service_guid_;
};

}  // namespace ash

#endif  // CHROME_TEST_BASE_ASH_INTERACTIVE_NETWORK_SHILL_SERVICE_UTIL_H_
