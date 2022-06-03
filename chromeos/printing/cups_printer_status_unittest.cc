// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chromeos/printing/cups_printer_status.h"

#include <string>
#include <vector>

#include "base/test/scoped_mock_clock_override.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace chromeos {

using CupsPrinterStatusReason = CupsPrinterStatus::CupsPrinterStatusReason;
using CupsReason = CupsPrinterStatus::CupsPrinterStatusReason::Reason;
using CupsSeverity = CupsPrinterStatus::CupsPrinterStatusReason::Severity;

class CupsPrinterStatusTest : public testing::Test {
 public:
  CupsPrinterStatusTest() = default;
  ~CupsPrinterStatusTest() override = default;

 protected:
  base::ScopedMockClockOverride clock_;
};

// Baseline test for creating a chromeos::CupsPrinterStatus.
TEST_F(CupsPrinterStatusTest, CreateCupsPrinterStatus) {
  CupsPrinterStatus cups_printer_status("id");
  EXPECT_EQ("id", cups_printer_status.GetPrinterId());
  EXPECT_EQ(clock_.Now(), cups_printer_status.GetTimestamp());
}

TEST_F(CupsPrinterStatusTest, AddStatusReasons) {
  CupsPrinterStatus cups_printer_status("id");
  cups_printer_status.AddStatusReason(
      CupsPrinterStatusReason::Reason::kNoError,
      CupsPrinterStatusReason::Severity::kReport);
  cups_printer_status.AddStatusReason(
      CupsPrinterStatusReason::Reason::kDoorOpen,
      CupsPrinterStatusReason::Severity::kWarning);

  EXPECT_EQ(2u, cups_printer_status.GetStatusReasons().size());
  EXPECT_EQ("id", cups_printer_status.GetPrinterId());
  EXPECT_EQ(clock_.Now(), cups_printer_status.GetTimestamp());

  std::vector<CupsPrinterStatus::CupsPrinterStatusReason> expected_reasons{
      CupsPrinterStatus::CupsPrinterStatusReason(CupsReason::kNoError,
                                                 CupsSeverity::kReport),
      CupsPrinterStatus::CupsPrinterStatusReason(CupsReason::kDoorOpen,
                                                 CupsSeverity::kWarning)};
  EXPECT_THAT(cups_printer_status.GetStatusReasons(), expected_reasons);
}

// Ensure that if printer returns two of the same status but each with different
// severities, make sure both are saved.
TEST_F(CupsPrinterStatusTest, SameReasonDifferentSeverity) {
  CupsPrinterStatus cups_printer_status("id");
  cups_printer_status.AddStatusReason(
      CupsPrinterStatusReason::Reason::kDeviceError,
      CupsPrinterStatusReason::Severity::kReport);
  cups_printer_status.AddStatusReason(
      CupsPrinterStatusReason::Reason::kDeviceError,
      CupsPrinterStatusReason::Severity::kWarning);

  EXPECT_EQ(2u, cups_printer_status.GetStatusReasons().size());
  EXPECT_EQ("id", cups_printer_status.GetPrinterId());
  EXPECT_EQ(clock_.Now(), cups_printer_status.GetTimestamp());

  std::vector<CupsPrinterStatus::CupsPrinterStatusReason> expected_reasons{
      CupsPrinterStatus::CupsPrinterStatusReason(CupsReason::kDeviceError,
                                                 CupsSeverity::kReport),
      CupsPrinterStatus::CupsPrinterStatusReason(CupsReason::kDeviceError,
                                                 CupsSeverity::kWarning)};
  EXPECT_THAT(cups_printer_status.GetStatusReasons(), expected_reasons);
}

// Ensure if printer returns the same status and severity twice, duplicates are
// not added to CupsPrinterStatus.
TEST_F(CupsPrinterStatusTest, SameReasonSameSeverity) {
  CupsPrinterStatus cups_printer_status("id");
  cups_printer_status.AddStatusReason(
      CupsPrinterStatusReason::Reason::kPaused,
      CupsPrinterStatusReason::Severity::kError);
  cups_printer_status.AddStatusReason(
      CupsPrinterStatusReason::Reason::kPaused,
      CupsPrinterStatusReason::Severity::kError);

  EXPECT_EQ(1u, cups_printer_status.GetStatusReasons().size());
  EXPECT_EQ("id", cups_printer_status.GetPrinterId());
  EXPECT_EQ(clock_.Now(), cups_printer_status.GetTimestamp());

  std::vector<CupsPrinterStatus::CupsPrinterStatusReason> expected_reasons{
      CupsPrinterStatus::CupsPrinterStatusReason(CupsReason::kPaused,
                                                 CupsSeverity::kError)};
  EXPECT_THAT(cups_printer_status.GetStatusReasons(), expected_reasons);
}

}  // namespace chromeos
