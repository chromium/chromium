// Copyright 2015 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_TEST_CHROMEDRIVER_CHROME_NETWORK_CONDITIONS_H_
#define CHROME_TEST_CHROMEDRIVER_CHROME_NETWORK_CONDITIONS_H_

#include <memory>
#include <string>

class Status;

struct NetworkConditions {
  NetworkConditions();
  NetworkConditions(bool offline, double latency,
                    double download_throughput, double upload_throughput);
  ~NetworkConditions();
  bool offline;
  double latency;
  double download_throughput;
  double upload_throughput;
};

Status FindPresetNetwork(
    std::string network_name,
    NetworkConditions* network_conditions);

#endif  // CHROME_TEST_CHROMEDRIVER_CHROME_NETWORK_CONDITIONS_H_
