// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/offline_pages/core/background/request_coordinator_event_logger.h"

#include "testing/gtest/include/gtest/gtest.h"

namespace offline_pages {

namespace {

const char kNamespace[] = "last_n";
const Offliner::RequestStatus kOfflinerStatus = Offliner::RequestStatus::SAVED;
const RequestNotifier::BackgroundSavePageResult kDroppedResult =
    RequestNotifier::BackgroundSavePageResult::START_COUNT_EXCEEDED;
const int64_t kId = 1234;
const UpdateRequestResult kQueueUpdateResult =
    UpdateRequestResult::STORE_FAILURE;

const char kOfflinerStatusLogString[] =
    "Background save attempt for last_n:1234 - SAVED";
const char kDroppedResultLogString[] =
    "Background save request removed last_n:1234 - START_COUNT_EXCEEDED";
const char kQueueUpdateResultLogString[] =
    "Updating queued request for last_n failed - STORE_FAILURE";
const int kTimeLength = 21;

}  // namespace

TEST(RequestCoordinatorEventLoggerTest, RecordsWhenLoggingIsOn) {
  RequestCoordinatorEventLogger logger;
  std::vector<std::string> log;

  logger.SetIsLogging(true);
  logger.RecordOfflinerResult(kNamespace, kOfflinerStatus, kId);
  logger.RecordDroppedSavePageRequest(kNamespace, kDroppedResult, kId);
  logger.RecordUpdateRequestFailed(kNamespace, kQueueUpdateResult);
  logger.GetLogs(&log);

  EXPECT_EQ(3u, log.size());
  EXPECT_EQ(std::string(kQueueUpdateResultLogString),
            log[0].substr(kTimeLength));
  EXPECT_EQ(std::string(kDroppedResultLogString), log[1].substr(kTimeLength));
  EXPECT_EQ(std::string(kOfflinerStatusLogString), log[2].substr(kTimeLength));
}

TEST(RequestCoordinatorEventLoggerTest, RecordsWhenLoggingIsOff) {
  RequestCoordinatorEventLogger logger;
  std::vector<std::string> log;

  logger.SetIsLogging(false);
  logger.RecordOfflinerResult(kNamespace, kOfflinerStatus, kId);
  logger.GetLogs(&log);

  EXPECT_EQ(0u, log.size());
}

TEST(RequestCoordinatorEventLoggerTest, DoesNotExceedMaxSize) {
  RequestCoordinatorEventLogger logger;
  std::vector<std::string> log;

  logger.SetIsLogging(true);
  for (size_t i = 0; i < kMaxLogCount + 1; ++i) {
    logger.RecordOfflinerResult(kNamespace, kOfflinerStatus, kId);
  }
  logger.GetLogs(&log);

  EXPECT_EQ(kMaxLogCount, log.size());
}

}  // namespace offline_pages
