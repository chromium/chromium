// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <stddef.h>
#include <stdint.h>

#include <memory>
#include <string>
#include <vector>

#include "base/command_line.h"
#include "base/json/json_reader.h"
#include "components/payments/content/utility/payment_manifest_parser.h"
#include "components/payments/content/web_app_manifest.h"
#include "components/payments/core/error_logger.h"

extern "C" int LLVMFuzzerTestOneInput(const uint8_t* data, size_t size) {
  std::string json_data(reinterpret_cast<const char*>(data), size);
  std::optional<base::Value> value = base::JSONReader::Read(json_data);
  if (!value) {
    return 0;
  }

  base::CommandLine::Init(0, nullptr);

  payments::ErrorLogger log;
  log.DisableInTest();
  std::vector<payments::WebAppManifestSection> output;
  payments::PaymentManifestParser::ParseWebAppManifestIntoVector(
      std::move(*value), log, &output);
  return 0;
}
