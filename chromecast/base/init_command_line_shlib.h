// Copyright 2016 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROMECAST_BASE_INIT_COMMAND_LINE_SHLIB_H_
#define CHROMECAST_BASE_INIT_COMMAND_LINE_SHLIB_H_

#include <string>
#include <vector>

namespace chromecast {

// A utility function which initializes command line flags and logging. It is
// intended to be called at the entry point of a shared library.
//
// When doing a component build, calls to this function will be a no-op.
// cast_shell will initialize the single global instance of base::CommandLine,
// which lives in libbase.so. For a non-component build, each shared lib will
// statically link its own instance of base::CommandLine, and will initialize
// it here on the initial call. Every subsequent call from within the same
// library will be a no-op.
//
// THREAD SAFTEY: Accessing the CommandLine instance for this process is
// technically not threadsafe when using the component build. However, the
// instance is initialized on the main thread before any other threads are
// started (see content/app/content_main_runner_impl.cc), so accessing this
// instance on those threads for read-operations is safe in practice.
void InitCommandLineShlib(const std::vector<std::string>& argv);

}  // namespace chromecast

#endif  // CHROMECAST_BASE_INIT_COMMAND_LINE_SHLIB_H_
