// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/test/chromedriver/chrome/network_conditions.h"

#include "base/json/json_reader.h"
#include "base/strings/string_number_conversions.h"
#include "base/strings/string_split.h"
#include "base/values.h"
#include "chrome/test/chromedriver/chrome/network_list.h"
#include "chrome/test/chromedriver/chrome/status.h"

NetworkConditions::NetworkConditions() {}
NetworkConditions::NetworkConditions(
    bool offline, double latency, double download_throughput,
    double upload_throughput)
  : offline(offline),
    latency(latency),
    download_throughput(download_throughput),
    upload_throughput(upload_throughput) {}
NetworkConditions::~NetworkConditions() {}

Status FindPresetNetwork(std::string network_name,
                         NetworkConditions* network_conditions) {
  base::JSONReader json_reader(base::JSON_ALLOW_TRAILING_COMMAS);
  std::unique_ptr<base::Value> networks_value =
      json_reader.ReadToValue(kNetworks);
  if (!networks_value)
    return Status(kUnknownError,
                  "could not parse network list because " +
                  json_reader.GetErrorMessage());

  base::ListValue* networks;
  if (!networks_value->GetAsList(&networks))
    return Status(kUnknownError, "malformed networks list");

  for (auto it = networks->begin(); it != networks->end(); ++it) {
    base::DictionaryValue* network = NULL;
    if (!it->GetAsDictionary(&network)) {
      return Status(kUnknownError,
                    "malformed network in list: should be a dictionary");
    }

    if (network == NULL)
      continue;

    std::string title;
    if (!network->GetString("title", &title)) {
      return Status(kUnknownError,
                    "malformed network title: should be a string");
    }
    if (title != network_name)
      continue;

    if (!network->GetDouble("latency",  &network_conditions->latency)) {
      return Status(kUnknownError,
                    "malformed network latency: should be a double");
    }
    // Preset list maintains a single "throughput" attribute for each network,
    // so we use that value for both |download_throughput| and
    // |upload_throughput| in the NetworkConditions (as does Chrome).
    if (!network->GetDouble("throughput",
                            &network_conditions->download_throughput) ||
        !network->GetDouble("throughput",
                            &network_conditions->upload_throughput)) {
      return Status(kUnknownError,
                    "malformed network throughput: should be a double");
    }

    // The throughputs of the network presets are listed in kbps, but must be
    // supplied to the OverrideNetworkConditions command as bps.
    network_conditions->download_throughput *= 1024;
    network_conditions->upload_throughput *= 1024;

    // |offline| is always false for now.
    network_conditions->offline = false;
    return Status(kOk);
  }

  return Status(kUnknownError, "must be a valid network");
}
