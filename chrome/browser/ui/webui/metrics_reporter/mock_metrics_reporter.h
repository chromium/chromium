// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_WEBUI_METRICS_REPORTER_MOCK_METRICS_REPORTER_H_
#define CHROME_BROWSER_UI_WEBUI_METRICS_REPORTER_MOCK_METRICS_REPORTER_H_

#include "chrome/browser/ui/webui/metrics_reporter/metrics_reporter.h"

#include "testing/gmock/include/gmock/gmock.h"

class MockMetricsReporter : public MetricsReporter {
 public:
  MockMetricsReporter();
  ~MockMetricsReporter() override;

  MOCK_METHOD(void, Mark, (const std::string&));
  MOCK_METHOD(void,
              Measure,
              (const std::string&, MetricsReporter::MeasureCallback));
  MOCK_METHOD(void,
              Measure,
              (const std::string&,
               const std::string&,
               MetricsReporter::MeasureCallback));
  MOCK_METHOD(void,
              HasMark,
              (const std::string&, MetricsReporter::HasMarkCallback));
  MOCK_METHOD(bool, HasLocalMark, (const std::string&));
  MOCK_METHOD(void, ClearMark, (const std::string&));
};

#endif  // CHROME_BROWSER_UI_WEBUI_METRICS_REPORTER_MOCK_METRICS_REPORTER_H_
