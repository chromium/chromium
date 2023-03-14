// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_DEVTOOLS_DEVTOOLS_PIPE_DEVTOOLS_PIPE_H_
#define COMPONENTS_DEVTOOLS_DEVTOOLS_PIPE_DEVTOOLS_PIPE_H_

#include "build/build_config.h"

namespace devtools_pipe {

// The following file descriptors are used by DevTools remote debugging pipe
// handler to read and write protocol messages. These should be identical to
// the ones specified in //content/browser/devtools/devtools_pipe_handler.cc
constexpr int kReadFD = 3;
constexpr int kWriteFD = 4;

// Checks if DevTools remote debugging pipe file descriptros are open. Embedders
// are supposed to call this before opening any files if --remote-debugging-pipe
// switch is present, see: https://crbug.com/1423048
bool AreFileDescriptorsOpen();

}  // namespace devtools_pipe

#endif  // COMPONENTS_DEVTOOLS_DEVTOOLS_PIPE_DEVTOOLS_PIPE_H_
