// Copyright 2015 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROMECAST_CRASH_LINUX_DUMMY_MINIDUMP_GENERATOR_H_
#define CHROMECAST_CRASH_LINUX_DUMMY_MINIDUMP_GENERATOR_H_

#include <string>

#include "chromecast/crash/linux/minidump_generator.h"

namespace chromecast {

class DummyMinidumpGenerator : public MinidumpGenerator {
 public:
  // A dummy minidump generator to move an existing minidump into
  // crash_uploader's monitoring path ($HOME/minidumps). The path is monitored
  // with file lock-control, so that third process should not write to it
  // directly.
  explicit DummyMinidumpGenerator(const std::string& existing_minidump_path);

  DummyMinidumpGenerator(const DummyMinidumpGenerator&) = delete;
  DummyMinidumpGenerator& operator=(const DummyMinidumpGenerator&) = delete;

  // MinidumpGenerator implementation:
  // Moves the minidump located at |existing_minidump_path_| to |minidump_path|.
  // Returns true if successful, false otherwise. Note that this function MUST
  // be called on a thread without IO restrictions, or it will fail fatally.
  bool Generate(const std::string& minidump_path) override;

 private:
  const std::string existing_minidump_path_;
};

}  // namespace chromecast

#endif  // CHROMECAST_CRASH_LINUX_DUMMY_MINIDUMP_GENERATOR_H_
