// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_INPUT_INPUT_EVENT_SOURCE_H_
#define COMPONENTS_INPUT_INPUT_EVENT_SOURCE_H_

namespace input {

// The source of the input event, denoting whether it was received in the
// browser process or in the Viz process.
enum class InputEventSource {
  kUnknown,
  kBrowser,
  kViz,
};

}  // namespace input

#endif  // COMPONENTS_INPUT_INPUT_EVENT_SOURCE_H_
