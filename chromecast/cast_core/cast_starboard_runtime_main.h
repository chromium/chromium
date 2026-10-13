// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROMECAST_CAST_CORE_CAST_STARBOARD_RUNTIME_MAIN_H_
#define CHROMECAST_CAST_CORE_CAST_STARBOARD_RUNTIME_MAIN_H_

#ifndef CHROMECAST_EXPORT
#define CHROMECAST_EXPORT __attribute__((visibility("default")))
#endif

#ifdef __cplusplus
extern "C" {
#endif

// Entry point into Cast's web runtime. Do not strip arguments passed to the
// runtime.
//
// The web runtime is based on Chromium and carries Chromium's usual caveats.
// * Chromium spawns multi-process child processes (--type=zygote,
// --type=renderer,
//   --type=gpu-process, etc.) by re-executing /proc/self/exe, or the process
//   specified by --browser-subprocess-path which MUST point to the same binary.
//   Any OEM initialization performed in main() prior to calling
//   CoreRuntimeStarboardMain() will execute in every child process unless
//   main() immediately delegates to CoreRuntimeStarboardMain() or guards
//   initialization when child process flags are present in argv.
// * Do not install signal handlers, as Chromium code overrides and is
// responsible for handling signals.
// * Do not start threads, as this may lead to CHECK failures in Chromium
// zygotes.
// * Avoid mutexes as this can lead to deadlocks after fork() has been called.
// * Any files opened must be set to close on execution so they are cleaned up
// when Chromium forks.
CHROMECAST_EXPORT int CoreRuntimeStarboardMain(int argc, const char** argv);

#ifdef __cplusplus
}
#endif

#endif  // CHROMECAST_CAST_CORE_CAST_STARBOARD_RUNTIME_MAIN_H_
