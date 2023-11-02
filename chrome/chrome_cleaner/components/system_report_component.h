// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_CHROME_CLEANER_COMPONENTS_SYSTEM_REPORT_COMPONENT_H_
#define CHROME_CHROME_CLEANER_COMPONENTS_SYSTEM_REPORT_COMPONENT_H_

#include <vector>

#include "chrome/chrome_cleaner/chrome_utils/extension_file_logger.h"
#include "chrome/chrome_cleaner/components/component_api.h"
#include "chrome/chrome_cleaner/parsers/json_parser/json_parser_api.h"
#include "chrome/chrome_cleaner/parsers/shortcut_parser/broker/sandboxed_shortcut_parser.h"

namespace chrome_cleaner {

// This class manages the production of a system information report.
class SystemReportComponent : public ComponentAPI {
 public:
  explicit SystemReportComponent(JsonParserAPI* json_parser,
                                 ShortcutParserAPI* shortcut_parser);

  // ComponentAPI methods.
  void PreScan() override;
  void PostScan(const std::vector<UwSId>& found_pups) override;
  void PreCleanup() override;
  void PostCleanup(ResultCode result_code, RebooterAPI* rebooter) override;
  void PostValidation(ResultCode result_code) override;
  void OnClose(ResultCode result_code) override;

  void CreateFullSystemReport();

  // Only exposed for tests.
  bool created_report() { return created_report_; }
  void SetUserDataPathForTesting(const base::FilePath& test_user_data_path);

 private:
  bool created_report_;
  JsonParserAPI* json_parser_;
  ShortcutParserAPI* shortcut_parser_;
  base::FilePath user_data_path_;
};

}  // namespace chrome_cleaner

#endif  // CHROME_CHROME_CLEANER_COMPONENTS_SYSTEM_REPORT_COMPONENT_H_
