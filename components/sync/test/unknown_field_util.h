// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_SYNC_TEST_UNKNOWN_FIELD_UTIL_H_
#define COMPONENTS_SYNC_TEST_UNKNOWN_FIELD_UTIL_H_

#include <string>
#include <utility>

#include "third_party/protobuf/src/google/protobuf/message_lite.h"

namespace syncer::test {

// Adds an unknown field to the given `proto` and returns the result proto.
void AddUnknownFieldToProto(::google::protobuf::MessageLite& proto,
                            std::string unknown_field_value);

// Returns the unknown field value from the given `proto`.
std::string GetUnknownFieldValueFromProto(
    const ::google::protobuf::MessageLite& proto);

}  // namespace syncer::test

#endif  // COMPONENTS_SYNC_TEST_UNKNOWN_FIELD_UTIL_H_
