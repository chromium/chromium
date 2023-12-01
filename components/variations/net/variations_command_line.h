// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_VARIATIONS_NET_VARIATIONS_COMMAND_LINE_H_
#define COMPONENTS_VARIATIONS_NET_VARIATIONS_COMMAND_LINE_H_

#include <string>

namespace variations {

// Generates a string containing the complete state of variations (including all
// the registered trials with corresponding groups, params and features) for the
// client in command-line format.
std::string GetVariationsCommandLine();

}  // namespace variations

#endif  // COMPONENTS_VARIATIONS_NET_VARIATIONS_COMMAND_LINE_H_