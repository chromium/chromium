// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/allocation_recorder/crash_handler/user_stream_data_source.h"

#include <utility>

#include "base/check.h"
#include "components/allocation_recorder/crash_handler/stream_data_source_factory.h"
#include "third_party/crashpad/crashpad/minidump/minidump_user_extension_stream_data_source.h"

namespace allocation_recorder::crash_handler {

AllocationRecorderStreamDataSource::AllocationRecorderStreamDataSource(
    scoped_refptr<StreamDataSourceFactory> stream_source_factory)
    : stream_source_factory_(std::move(stream_source_factory)) {
  DCHECK(stream_source_factory_);
}

AllocationRecorderStreamDataSource::~AllocationRecorderStreamDataSource() =
    default;

std::unique_ptr<crashpad::MinidumpUserExtensionStreamDataSource>
AllocationRecorderStreamDataSource::ProduceStreamData(
    crashpad::ProcessSnapshot* const process_snapshot) {
#if BUILDFLAG(ENABLE_ALLOCATION_STACK_TRACE_RECORDER)
  return stream_source_factory_->CreateReportStream();
#else
  return stream_source_factory_->CreateErrorMessage(
      "!!NO ALLOCATION RECORDER AVAILABLE!!");
#endif
}

}  // namespace allocation_recorder::crash_handler
