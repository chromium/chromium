// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_STABILITY_REPORT_TEST_STABILITY_REPORT_READER_H_
#define COMPONENTS_STABILITY_REPORT_TEST_STABILITY_REPORT_READER_H_

#include "components/stability_report/stability_report.pb.h"
#include "third_party/crashpad/crashpad/minidump/minidump_user_extension_stream_data_source.h"

namespace stability_report::test {

// Utility class for inspecting the data collected by Crashpad.
class StabilityReportReader final
    : public crashpad::MinidumpUserExtensionStreamDataSource::Delegate {
 public:
  StabilityReportReader() = default;
  ~StabilityReportReader() = default;

  StabilityReportReader(const StabilityReportReader&) = delete;
  StabilityReportReader& operator=(const StabilityReportReader&) = delete;

  const StabilityReport& report() const;

  bool ExtensionStreamDataSourceRead(const void* data, size_t size) final;

 private:
  StabilityReport report_;
};

}  // namespace stability_report::test

#endif  // COMPONENTS_STABILITY_REPORT_TEST_STABILITY_REPORT_READER_H_
