// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_STABILITY_REPORT_STABILITY_REPORT_DATA_SOURCE_H_
#define COMPONENTS_STABILITY_REPORT_STABILITY_REPORT_DATA_SOURCE_H_

#include "components/stability_report/stability_report.pb.h"
#include "third_party/crashpad/crashpad/minidump/minidump_user_extension_stream_data_source.h"

namespace stability_report {

// The stream type assigned to the minidump stream that holds the serialized
// stability report.
// Note: the value was obtained by adding 1 to the stream type used for holding
// the SyzyAsan proto.
constexpr uint32_t kStreamType = 0x4B6B0002;

// A data source that holds a serialized StabilityReport.
class StabilityReportDataSource final
    : public crashpad::MinidumpUserExtensionStreamDataSource {
 public:
  explicit StabilityReportDataSource(const StabilityReport& report);
  ~StabilityReportDataSource() final = default;

  StabilityReportDataSource(const StabilityReportDataSource&) = delete;
  StabilityReportDataSource& operator=(const StabilityReportDataSource&) =
      delete;

  size_t StreamDataSize() final;

  bool ReadStreamData(Delegate* delegate) final;

 private:
  std::string data_;
};

}  // namespace stability_report

#endif  // COMPONENTS_STABILITY_REPORT_STABILITY_REPORT_DATA_SOURCE_H_
