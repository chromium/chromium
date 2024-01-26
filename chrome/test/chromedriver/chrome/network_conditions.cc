// Copyright 2014 The Chromium Authors
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
  auto parsed_json = base::JSONReader::ReadAndReturnValueWithError(
      kNetworks, base::JSON_ALLOW_TRAILING_COMMAS);
  if (!parsed_json.has_value())
    return Status(kUnknownError, "could not parse network list because " +
                                     parsed_json.error().message);

  if (!parsed_json->is_list())
    return Status(kUnknownError, "malformed networks list");

  for (const auto& entry : parsed_json->GetList()) {
    if (!entry.is_dict()) {
      return Status(kUnknownError,
                    "malformed network in list: should be a dictionary");
    }

    const base::Value::Dict& network = entry.GetDict();

    const std::string* title = network.FindString("title");
    if (!title) {
      return Status(kUnknownError,
                    "malformed network title: should be a string");
    }
    if (*title != network_name)
      continue;

    std::optional<double> maybe_latency = network.FindDouble("latency");
    std::optional<double> maybe_throughput = network.FindDouble("throughput");

    if (!maybe_latency.has_value()) {
      return Status(kUnknownError,
                    "malformed network latency: should be a double");
    }
    // Preset list maintains a single "throughput" attribute for each network,
    // so we use that value for both |download_throughput| and
    // |upload_throughput| in the NetworkConditions (as does Chrome).
    if (!maybe_throughput.has_value()) {
      return Status(kUnknownError,
                    "malformed network throughput: should be a double");
    }

    network_conditions->latency = maybe_latency.value();
    // The throughputs of the network presets are listed in kbps, but
    // must be supplied to the OverrideNetworkConditions command as bps.
    network_conditions->download_throughput = maybe_throughput.value() * 1024;
    network_conditions->upload_throughput = maybe_throughput.value() * 1024;

    // |offline| is always false for now.
    network_conditions->offline = false;
    return Status(kOk);
  }

  return Status(kUnknownError, "must be a valid network");
}
