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
  explicit ShillServiceInfo(unsigned int id, std::string service_type);
  ~ShillServiceInfo();

  const std::string& service_name() const { return service_name_; }
  const std::string& service_path() const { return service_path_; }
  const std::string& service_guid() const { return service_guid_; }

  // Configure the shill service and add to the shared profile. Make a
  // connection request to the network if `connected`.
  void ConfigureService(bool connected) const;

 private:
  const std::string service_name_;
  const std::string service_path_;
  const std::string service_guid_;
  const std::string service_type_;
};

void ConnectShillService(const std::string& service_path);

void DisconnectShillService(const std::string& service_path);

}  // namespace ash

#endif  // CHROME_TEST_BASE_ASH_INTERACTIVE_NETWORK_SHILL_SERVICE_UTIL_H_
