// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/record_replay/content/browser/content_record_replay_driver_factory.h"

#include <vector>

#include "base/containers/map_util.h"
#include "components/record_replay/content/browser/content_record_replay_driver.h"
#include "components/record_replay/core/browser/record_replay_client.h"
#include "components/record_replay/core/browser/record_replay_driver.h"
#include "components/record_replay/core/common/element_id.h"
#include "content/public/browser/web_contents.h"

namespace record_replay {

ContentRecordReplayDriverFactory::ContentRecordReplayDriverFactory(
    RecordReplayClient& client)
    : client_(client) {}

ContentRecordReplayDriverFactory::~ContentRecordReplayDriverFactory() {}

ContentRecordReplayDriver* ContentRecordReplayDriverFactory::GetOrCreateDriver(
    content::RenderFrameHost* rfh) {
  if (!rfh->IsRenderFrameLive()) {
    return nullptr;
  }
  std::unique_ptr<ContentRecordReplayDriver>& driver =
      drivers_[rfh->GetFrameToken().value()];
  if (!driver) {
    driver = std::make_unique<ContentRecordReplayDriver>(rfh, *client_);
  }
  return driver.get();
}

RecordReplayDriver* ContentRecordReplayDriverFactory::GetDriver(
    const ElementId& element_id) {
  return GetDriver(element_id.token());
}

RecordReplayDriver* ContentRecordReplayDriverFactory::GetDriver(
    const autofill::FieldGlobalId& field_id) {
  return GetDriver(field_id.frame_token.value());
}

std::vector<RecordReplayDriver*>
ContentRecordReplayDriverFactory::GetActiveDrivers() {
  std::vector<RecordReplayDriver*> drivers;
  ForEachDriver([&](RecordReplayDriver& driver) {
    if (driver.IsActive()) {
      drivers.push_back(&driver);
    }
  });
  return drivers;
}

void ContentRecordReplayDriverFactory::ForEachDriver(
    base::FunctionRef<void(RecordReplayDriver&)> fun) {
  for (const auto& [rfh, driver] : drivers_) {
    fun(*driver);
  }
}

RecordReplayDriver* ContentRecordReplayDriverFactory::GetDriver(
    const base::UnguessableToken& frame_token) {
  std::unique_ptr<ContentRecordReplayDriver>* driver =
      base::FindOrNull(drivers_, frame_token);
  return driver ? driver->get() : nullptr;
}

void ContentRecordReplayDriverFactory::RenderFrameCreated(
    content::RenderFrameHost* rfh) {
  RecordReplayDriver* driver = GetOrCreateDriver(rfh);
  if (driver && record_future_drivers_) {
    driver->StartRecording();
  }
}

void ContentRecordReplayDriverFactory::RenderFrameDeleted(
    content::RenderFrameHost* rfh) {
  drivers_.erase(rfh->GetFrameToken().value());
}

void ContentRecordReplayDriverFactory::SetRecordForFutureDrivers(bool enable) {
  record_future_drivers_ = enable;
}

}  // namespace record_replay
