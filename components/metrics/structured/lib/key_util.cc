// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/metrics/structured/lib/key_util.h"

#include <utility>

#include "base/check_op.h"
#include "base/unguessable_token.h"

namespace metrics::structured::util {
namespace {

constexpr std::string_view kKeyString = "key";
constexpr std::string_view kLastRotationString = "last_rotation";
constexpr std::string_view kRotationPeriodString = "rotation_period";

}  // namespace

std::string GenerateNewKey() {
  const std::string key = base::UnguessableToken::Create().ToString();
  CHECK_EQ(key.size(), kKeySize);
  return key;
}

base::Value CreateValueFromKeyProto(const KeyProto& proto) {
  base::Value::Dict key =
      base::Value::Dict()
          .Set(kKeyString, proto.key())
          // last_rotation and rotation_period are represented as int64's but
          // should never exceed int.
          .Set(kLastRotationString, static_cast<int>(proto.last_rotation()))
          .Set(kRotationPeriodString,
               static_cast<int>(proto.rotation_period()));

  return base::Value(std::move(key));
}

std::optional<KeyProto> CreateKeyProtoFromValue(
    const base::Value::Dict& value) {
  const std::string* key = value.FindString(kKeyString);
  if (!key) {
    return std::nullopt;
  }

  std::optional<int> last_rotation = value.FindInt(kLastRotationString);
  if (!last_rotation.has_value()) {
    return std::nullopt;
  }

  std::optional<int> rotation_period = value.FindInt(kRotationPeriodString);
  if (!rotation_period.has_value()) {
    return std::nullopt;
  }

  KeyProto proto;
  proto.set_key(*key);
  proto.set_last_rotation(*last_rotation);
  proto.set_rotation_period(*rotation_period);

  return proto;
}

}  // namespace metrics::structured::util
