// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/content_settings/core/common/content_settings_partition_key.h"

#include <optional>

#include "base/check.h"
#include "base/json/json_reader.h"
#include "base/json/json_writer.h"
#include "base/values.h"

namespace content_settings {

#if BUILDFLAG(IS_IOS)
// static
const PartitionKey& PartitionKey::GetDefault() {
  return GetDefaultImpl();
}
#endif  // BUILDFLAG(IS_IOS)

// static
const PartitionKey& PartitionKey::GetDefaultForTesting() {
  return GetDefaultImpl();
}

// static
PartitionKey PartitionKey::CreateForTesting(std::string domain,
                                            std::string name,
                                            bool in_memory) {
  return PartitionKey(domain, name, in_memory);
}

// static
const PartitionKey& PartitionKey::WipGetDefault() {
  return GetDefaultImpl();
}

// static
std::optional<PartitionKey> PartitionKey::Deserialize(const std::string& data) {
  const auto json = base::JSONReader::Read(data);
  if (!json.has_value()) {
    return std::nullopt;
  }
  const auto* list = json->GetIfList();
  if (!list || list->size() != 3) {
    return std::nullopt;
  }
  const std::string* domain = (*list)[0].GetIfString();
  const std::string* name = (*list)[1].GetIfString();
  const std::optional<bool> in_memory = (*list)[2].GetIfBool();
  if (domain && name && in_memory) {
    auto key = std::make_optional(PartitionKey(*domain, *name, *in_memory));
    if (key->Serialize() != data) {
      // Reject non-canonical serialized key.
      return std::nullopt;
    }
    return key;
  }

  return std::nullopt;
}

std::string PartitionKey::Serialize() const {
  std::string out;
  base::JSONWriter::Write(
      base::Value::List().Append(domain_).Append(name_).Append(in_memory_),
      &out);
  return out;
}

PartitionKey::PartitionKey(const PartitionKey& key) = default;
PartitionKey::PartitionKey(PartitionKey&& key) = default;
std::strong_ordering PartitionKey::operator<=>(const PartitionKey&) const =
    default;
bool PartitionKey::operator==(const PartitionKey&) const = default;

// static
const PartitionKey& PartitionKey::GetDefaultImpl() {
  static const base::NoDestructor<PartitionKey> key;
  return *key;
}

PartitionKey::PartitionKey() : PartitionKey("", "", false) {}

PartitionKey::PartitionKey(std::string domain, std::string name, bool in_memory)
    : domain_{std::move(domain)},
      name_{std::move(name)},
      in_memory_{in_memory} {
  if (domain_.empty()) {
    // This is a default partition key.
    CHECK(name_.empty());
    CHECK(!in_memory_);
  }
}

std::ostream& operator<<(std::ostream& os, const PartitionKey& key) {
  return os << "{domain=" << key.domain() << ", name=" << key.name()
            << ", in_memory=" << key.in_memory() << "}";
}

}  // namespace content_settings
