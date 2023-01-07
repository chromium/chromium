// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_GWP_ASAN_CRASH_HANDLER_CRASH_HANDLER_H_
#define COMPONENTS_GWP_ASAN_CRASH_HANDLER_CRASH_HANDLER_H_

#include "third_party/crashpad/crashpad/handler/user_stream_data_source.h"

namespace crashpad {
class ProcessSnapshot;
}  // namespace crashpad

namespace gwp_asan {

namespace internal {

// The stream type assigned to the minidump stream that holds the serialized
// GWP ASan crash state.
const uint32_t kGwpAsanMinidumpStreamType = 0x4B6B0004;

}  // namespace internal

// A crashpad extension installed at crashpad handler start-up to inspect the
// crashing process, see if the crash was caused by a GWP-ASan exception, and
// add a GWP-ASan stream to the minidump if so.
class UserStreamDataSource : public crashpad::UserStreamDataSource {
 public:
  UserStreamDataSource() = default;

  UserStreamDataSource(const UserStreamDataSource&) = delete;
  UserStreamDataSource& operator=(const UserStreamDataSource&) = delete;

  std::unique_ptr<crashpad::MinidumpUserExtensionStreamDataSource>
  ProduceStreamData(crashpad::ProcessSnapshot* process_snapshot) override;
};

}  // namespace gwp_asan

#endif  // COMPONENTS_GWP_ASAN_CRASH_HANDLER_CRASH_HANDLER_H_
