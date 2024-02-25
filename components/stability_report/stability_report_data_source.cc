// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/stability_report/stability_report_data_source.h"

#include "third_party/crashpad/crashpad/minidump/minidump_user_extension_stream_data_source.h"

namespace stability_report {

StabilityReportDataSource::StabilityReportDataSource(
    const StabilityReport& report)
    : crashpad::MinidumpUserExtensionStreamDataSource(kStreamType),
      data_(report.SerializeAsString()) {
  // On error, SerializeAsString() will return an empty string which will
  // cause ReadStreamData() to harmlessly return no data.
}

size_t StabilityReportDataSource::StreamDataSize() {
  return data_.size();
}

bool StabilityReportDataSource::ReadStreamData(Delegate* delegate) {
  return delegate->ExtensionStreamDataSourceRead(data_.data(), data_.size());
}

}  // namespace stability_report
