// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_RECORD_REPLAY_CONTENT_BROWSER_CONTENT_RECORD_REPLAY_DRIVER_FACTORY_H_
#define COMPONENTS_RECORD_REPLAY_CONTENT_BROWSER_CONTENT_RECORD_REPLAY_DRIVER_FACTORY_H_

#include <vector>

#include "base/memory/raw_ref.h"
#include "components/record_replay/content/browser/content_record_replay_driver.h"
#include "components/record_replay/core/browser/record_replay_driver_factory.h"
#include "content/public/browser/web_contents_observer.h"
#include "third_party/abseil-cpp/absl/container/flat_hash_map.h"
#include "third_party/blink/public/common/tokens/tokens.h"

namespace content {
class RenderFrameHost;
}

namespace record_replay {

class RecordReplayClient;
class RecordReplayDriver;

// This class manages the lifecycle of RecordReplayDriver instances within a
// tab. It is a WebContentsObserver that automatically creates drivers for new
// frames.
//
// Owned by `RecordReplayClient` (1 per tab) and runs on the UI thread.
class ContentRecordReplayDriverFactory : public RecordReplayDriverFactory,
                                         public content::WebContentsObserver {
 public:
  explicit ContentRecordReplayDriverFactory(RecordReplayClient& client);
  ContentRecordReplayDriverFactory(const ContentRecordReplayDriverFactory&) =
      delete;
  ContentRecordReplayDriverFactory& operator=(
      const ContentRecordReplayDriverFactory&) = delete;
  ~ContentRecordReplayDriverFactory() override;

  using content::WebContentsObserver::Observe;

  record_replay::ContentRecordReplayDriver* GetOrCreateDriver(
      content::RenderFrameHost* rfh);

  // RecordReplayDriverFactory:
  RecordReplayDriver* GetDriver(const ElementId& element_id) override;
  RecordReplayDriver* GetDriver(
      const autofill::FieldGlobalId& field_id) override;
  std::vector<RecordReplayDriver*> GetActiveDrivers() override;
  void ForEachDriver(base::FunctionRef<void(RecordReplayDriver&)> fun) override;
  void SetRecordForFutureDrivers(bool enable) override;

 private:
  RecordReplayDriver* GetDriver(const base::UnguessableToken& frame_token);

  // content::WebContentsObserver:
  void RenderFrameCreated(content::RenderFrameHost* rfh) override;
  void RenderFrameDeleted(content::RenderFrameHost* rfh) override;

  const raw_ref<RecordReplayClient> client_;
  absl::flat_hash_map<base::UnguessableToken,
                      std::unique_ptr<ContentRecordReplayDriver>>
      drivers_;
  // If true, `StartRecording()` will be called on newly created drivers.
  bool record_future_drivers_ = false;
};

}  // namespace record_replay

#endif  // COMPONENTS_RECORD_REPLAY_CONTENT_BROWSER_CONTENT_RECORD_REPLAY_DRIVER_FACTORY_H_
