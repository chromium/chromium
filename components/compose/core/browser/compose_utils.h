// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_COMPOSE_CORE_BROWSER_COMPOSE_UTILS_H_
#define COMPONENTS_COMPOSE_CORE_BROWSER_COMPOSE_UTILS_H_

#include <string>

namespace compose {

// Returns true if the number of words in `prompt` is between minimum and
// maximum. False otherwise.
bool IsWordCountWithinBounds(const std::string& prompt,
                             unsigned int minimum,
                             unsigned int maximum);

std::string GetTrimmedPageText(std::string inner_text,
                               int max_length,
                               int element_offset,
                               int header_length);

}  // namespace compose

#endif  // COMPONENTS_COMPOSE_CORE_BROWSER_COMPOSE_UTILS_H_
