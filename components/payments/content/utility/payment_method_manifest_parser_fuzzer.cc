// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <stddef.h>
#include <stdint.h>

#include <string>
#include <string_view>
#include <vector>

#include "base/at_exit.h"
#include "base/check.h"
#include "base/command_line.h"
#include "base/i18n/icu_util.h"
#include "base/json/json_reader.h"
#include "components/payments/content/utility/payment_manifest_parser.h"
#include "components/payments/core/error_logger.h"
#include "url/gurl.h"
#include "url/origin.h"

struct IcuEnvironment {
  IcuEnvironment() { CHECK(base::i18n::InitializeICU()); }
  // used by ICU integration.
  base::AtExitManager at_exit_manager;
};

IcuEnvironment* env = new IcuEnvironment();

extern "C" int LLVMFuzzerTestOneInput(const uint8_t* data, size_t size) {
  std::vector<GURL> web_app_manifest_urls;
  std::vector<url::Origin> supported_origins;

  std::string_view json_data(reinterpret_cast<const char*>(data), size);
  std::optional<base::Value> value = base::JSONReader::Read(json_data);
  if (!value) {
    return 0;
  }

  base::CommandLine::Init(0, nullptr);

  payments::ErrorLogger log;
  log.DisableInTest();
  payments::PaymentManifestParser::ParsePaymentMethodManifestIntoVectors(
      GURL("https://chromium.org/pmm.json"), std::move(*value), log,
      &web_app_manifest_urls, &supported_origins);
  return 0;
}
