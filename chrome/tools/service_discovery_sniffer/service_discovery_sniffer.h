// Copyright 2013 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_TOOLS_SERVICE_DISCOVERY_SNIFFER_SERVICE_DISCOVERY_SNIFFER_H_
#define CHROME_TOOLS_SERVICE_DISCOVERY_SNIFFER_SERVICE_DISCOVERY_SNIFFER_H_

#include <map>
#include <memory>
#include <string>

#include "chrome/browser/local_discovery/service_discovery_client.h"

namespace local_discovery {

// Resolves a service and prints out information regarding it to the
// console. |client| must outlive the ServicePrinter.
class ServicePrinter {
 public:
  ServicePrinter(ServiceDiscoveryClient* client,
                 const std::string& service_name);

  ServicePrinter(const ServicePrinter&) = delete;
  ServicePrinter& operator=(const ServicePrinter&) = delete;

  ~ServicePrinter();

  void Added();
  void Changed();
  void Removed();

 private:
  void OnServiceResolved(ServiceResolver::RequestStatus status,
                         const ServiceDescription& service);

  bool changed_;
  std::unique_ptr<ServiceResolver> service_resolver_;
};

// Monitors a service type and prints information regarding all services on it
// to the console. |client| must outlive the ServiceTypePrinter.
class ServiceTypePrinter {
 public:
  ServiceTypePrinter(ServiceDiscoveryClient* client,
                     const std::string& service_type);

  ServiceTypePrinter(const ServiceTypePrinter&) = delete;
  ServiceTypePrinter& operator=(const ServiceTypePrinter&) = delete;

  virtual ~ServiceTypePrinter();

  void Start();
  void OnServiceUpdated(ServiceWatcher::UpdateType,
                        const std::string& service_name);

 private:
  std::map<std::string, std::unique_ptr<ServicePrinter>> services_;
  std::unique_ptr<ServiceWatcher> watcher_;
  ServiceDiscoveryClient* client_;
};

}  // namespace local_discovery

#endif  // CHROME_TOOLS_SERVICE_DISCOVERY_SNIFFER_SERVICE_DISCOVERY_SNIFFER_H_
