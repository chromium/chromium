// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/updater/external_constants_builder.h"

#include <string>
#include <vector>

#include "base/json/json_file_value_serializer.h"
#include "base/logging.h"
#include "base/optional.h"
#include "chrome/updater/constants.h"
#include "chrome/updater/util.h"

namespace updater {

ExternalConstantsBuilder::~ExternalConstantsBuilder() {
  LOG_IF(WARNING, !written_) << "An ExternalConstantsBuilder with "
                             << overrides_.DictSize() << " entries is being "
                             << "discarded without being written to a file.";
}

ExternalConstantsBuilder& ExternalConstantsBuilder::SetUpdateURL(
    const std::vector<std::string>& urls) {
  base::Value::ListStorage url_list;
  url_list.reserve(urls.size());
  for (const std::string& url_string : urls) {
    url_list.push_back(base::Value(url_string));
  }
  overrides_.SetKey(kDevOverrideKeyUrl, base::Value(std::move(url_list)));
  return *this;
}

ExternalConstantsBuilder& ExternalConstantsBuilder::ClearUpdateURL() {
  overrides_.RemoveKey(kDevOverrideKeyUrl);
  return *this;
}

ExternalConstantsBuilder& ExternalConstantsBuilder::SetUseCUP(bool use_cup) {
  overrides_.SetBoolKey(kDevOverrideKeyUseCUP, use_cup);
  return *this;
}

ExternalConstantsBuilder& ExternalConstantsBuilder::ClearUseCUP() {
  overrides_.RemoveKey(kDevOverrideKeyUseCUP);
  return *this;
}

ExternalConstantsBuilder& ExternalConstantsBuilder::SetInitialDelay(
    double initial_delay) {
  overrides_.SetDoubleKey(kDevOverrideKeyInitialDelay, initial_delay);
  return *this;
}

ExternalConstantsBuilder& ExternalConstantsBuilder::ClearInitialDelay() {
  overrides_.RemoveKey(kDevOverrideKeyInitialDelay);
  return *this;
}

ExternalConstantsBuilder& ExternalConstantsBuilder::SetServerKeepAliveSeconds(
    int server_keep_alive_seconds) {
  overrides_.SetIntKey(kDevOverrideKeyServerKeepAliveSeconds,
                       server_keep_alive_seconds);
  return *this;
}

ExternalConstantsBuilder&
ExternalConstantsBuilder::ClearServerKeepAliveSeconds() {
  overrides_.RemoveKey(kDevOverrideKeyServerKeepAliveSeconds);
  return *this;
}

bool ExternalConstantsBuilder::Overwrite() {
  base::Optional<base::FilePath> base_path = GetBaseDirectory();
  if (!base_path) {
    LOG(ERROR) << "Can't find base directory; can't save constant overrides.";
    return false;
  }
  const base::FilePath override_file_path =
      base_path.value().AppendASCII(kDevOverrideFileName);
  bool ok = JSONFileValueSerializer(override_file_path).Serialize(overrides_);
  written_ = written_ || ok;
  return ok;
}

}  // namespace updater
