// Copyright 2015 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROMECAST_CRASH_LINUX_MINIDUMP_GENERATOR_H_
#define CHROMECAST_CRASH_LINUX_MINIDUMP_GENERATOR_H_

#include <string>

namespace chromecast {

// An interface to generate a minidump at a given filepath.
class MinidumpGenerator {
 public:
  virtual ~MinidumpGenerator() {}

  // Generates a minidump file at |minidump_path|. This method should only be
  // called on a thread without IO restrictions, as non-trivial implementations
  // will almost certainly require IO permissions. Returns true if minidump was
  // successfully generated.
  virtual bool Generate(const std::string& minidump_path) = 0;
};

}  // namespace chromecast

#endif  // CHROMECAST_CRASH_LINUX_MINIDUMP_GENERATOR_H_
