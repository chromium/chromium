// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/app_list/search/search_result_ranker/app_list_launch_metrics_provider.h"

#include <set>
#include <string>

#include "base/base64.h"
#include "base/bind.h"
#include "base/files/file_util.h"
#include "base/files/important_file_writer.h"
#include "base/files/scoped_temp_dir.h"
#include "base/strings/string_number_conversions.h"
#include "base/strings/string_piece.h"
#include "base/strings/string_util.h"
#include "base/test/metrics/histogram_tester.h"
#include "base/test/task_environment.h"
#include "base/threading/scoped_blocking_call.h"
#include "chrome/browser/chromeos/profiles/profile_helper.h"
#include "chrome/browser/profiles/profile_manager.h"
#include "chrome/browser/ui/app_list/search/search_result_ranker/app_list_launch_recorder.h"
#include "chrome/browser/ui/app_list/search/search_result_ranker/app_list_launch_recorder_util.h"
#include "chrome/test/base/testing_browser_process.h"
#include "chrome/test/base/testing_profile.h"
#include "components/metrics/client_info.h"
#include "components/metrics/metrics_pref_names.h"
#include "components/metrics/metrics_service.h"
#include "content/public/test/browser_task_environment.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/metrics_proto/chrome_user_metrics_extension.pb.h"
#include "third_party/protobuf/src/google/protobuf/repeated_field.h"

namespace app_list {

namespace {

using ::chromeos::ProfileHelper;

}  // namespace

class AppListLaunchMetricsProviderTest : public testing::Test {
 protected:
  void SetUp() override { ASSERT_TRUE(temp_dir_.CreateUniqueTempDir()); }

  base::Optional<base::FilePath> GetTempDir() { return temp_dir_.GetPath(); }

  void Wait() { task_environment_.RunUntilIdle(); }

  base::HistogramTester histogram_tester_;
  base::test::TaskEnvironment task_environment_;
  base::ScopedTempDir temp_dir_;

  std::unique_ptr<AppListLaunchMetricsProvider> provider_;
};

}  // namespace app_list
