// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_ALLOCATION_RECORDER_CRASH_HANDLER_USER_STREAM_DATA_SOURCE_H_
#define COMPONENTS_ALLOCATION_RECORDER_CRASH_HANDLER_USER_STREAM_DATA_SOURCE_H_

#include "base/memory/scoped_refptr.h"
#include "third_party/crashpad/crashpad/handler/user_stream_data_source.h"

namespace crashpad {
class ProcessSnapshot;
class MinidumpUserExtensionStreamDataSource;
}  // namespace crashpad

namespace allocation_recorder::crash_handler {

class AllocationRecorderHolder;
class StreamDataSourceFactory;

// The implementation of crashpad's UserStreamDataSource which will be included
// in the crashpad report. This is the main entry point from the crashpad
// handler into user provided data streams.
class AllocationRecorderStreamDataSource
    : public crashpad::UserStreamDataSource {
 public:
  // Create a new instance of AllocationTraceStreamDataSource. This instance
  // will use the passed holder to store the recorder and factory to create the
  // final stream. See ProduceStreamData for details.
  AllocationRecorderStreamDataSource(
      scoped_refptr<AllocationRecorderHolder> recorder_holder,
      scoped_refptr<StreamDataSourceFactory> stream_source_factory);

  ~AllocationRecorderStreamDataSource() override;

  // Create a stream data source from the passed process snapshot. This will
  // create a copy of the recorder using the AllocationTraceRecorderHolder and
  // StreamDataSourceFactory passed via the constructor.
  std::unique_ptr<crashpad::MinidumpUserExtensionStreamDataSource>
  ProduceStreamData(crashpad::ProcessSnapshot* process_snapshot) override;

 private:
  const scoped_refptr<AllocationRecorderHolder> recorder_holder_;
  const scoped_refptr<StreamDataSourceFactory> stream_source_factory_;
};

}  // namespace allocation_recorder::crash_handler

#endif  // COMPONENTS_ALLOCATION_RECORDER_CRASH_HANDLER_USER_STREAM_DATA_SOURCE_H_
