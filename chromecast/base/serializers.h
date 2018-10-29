// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROMECAST_BASE_SERIALIZERS_H_
#define CHROMECAST_BASE_SERIALIZERS_H_

#include <memory>
#include <string>

#include "base/optional.h"

namespace base {
class Value;
class FilePath;
}

namespace chromecast {

// Helper function which deserializes JSON |text| into a base::Value. If |text|
// is empty, is not valid JSON, or if some other deserialization error occurs,
// the return value will hold the NULL pointer.
std::unique_ptr<base::Value> DeserializeFromJson(const std::string& text);

// Helper function which serializes |value| into a JSON string. If a
// serialization error occurs,the return value will be base::nullopt.
// Dereferencing the result is equivalent to DCHECK()-ing that serialization
// succeeded and retrieving the serialized string.
base::Optional<std::string> SerializeToJson(const base::Value& value);

// Helper function which deserializes JSON file at |path| into a base::Value.
// If file in |path| is empty, is not valid JSON, or if some other
// deserialization error occurs, the return value will hold the NULL pointer.
std::unique_ptr<base::Value> DeserializeJsonFromFile(
    const base::FilePath& path);

// Helper function which serializes |value| into the file at |path|. The
// function returns true on success, false otherwise.
bool SerializeJsonToFile(const base::FilePath& path, const base::Value& value);

}  // namespace chromecast

#endif  // CHROMECAST_BASE_SERIALIZERS_H_
