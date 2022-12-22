// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_FUCHSIA_COMPONENT_SUPPORT_SERIALIZE_ARGUMENTS_H_
#define COMPONENTS_FUCHSIA_COMPONENT_SUPPORT_SERIALIZE_ARGUMENTS_H_

#include <stdint.h>

#include <vector>

namespace base {
class CommandLine;
}

namespace fuchsia_component_support {

// Serializes everything but the program name from `command_line` into a
// sequence of bytes. See AppendArgumentsFromFile for the converse.
std::vector<uint8_t> SerializeArguments(const base::CommandLine& command_line);

}  // namespace fuchsia_component_support

#endif  // COMPONENTS_FUCHSIA_COMPONENT_SUPPORT_SERIALIZE_ARGUMENTS_H_
