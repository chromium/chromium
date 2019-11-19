// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_GCM_DRIVER_FAKE_GCM_PROFILE_SERVICE_H_
#define COMPONENTS_GCM_DRIVER_FAKE_GCM_PROFILE_SERVICE_H_

#include <list>
#include <memory>
#include <string>
#include <vector>

#include "base/macros.h"
#include "components/gcm_driver/common/gcm_message.h"
#include "components/gcm_driver/gcm_client.h"
#include "components/gcm_driver/gcm_profile_service.h"

namespace content {
class BrowserContext;
}  // namespace content

namespace gcm {

// Acts as a bridge between GCM API and GCM Client layer for testing purposes.
class FakeGCMProfileService : public GCMProfileService {
 public:
  // Helper function to be used with KeyedServiceFactory::SetTestingFactory().
  static std::unique_ptr<KeyedService> Build(content::BrowserContext* context);

  FakeGCMProfileService();
  ~FakeGCMProfileService() override;

  void AddExpectedUnregisterResponse(GCMClient::Result result);

  void DispatchMessage(const std::string& app_id,
                       const IncomingMessage& message);

  const OutgoingMessage& last_sent_message() const {
    return last_sent_message_;
  }

  const WebPushMessage& last_web_push_message() const {
    return last_web_push_message_;
  }

  const std::string& last_receiver_id() const { return last_receiver_id_; }

  const std::string& last_registered_app_id() const {
    return last_registered_app_id_;
  }

  const std::vector<std::string>& last_registered_sender_ids() const {
    return last_registered_sender_ids_;
  }

  // Set whether the service will collect parameters of the calls for further
  // verification in tests.
  void set_collect(bool collect) { collect_ = collect; }

  // Crude offline simulation: requests fail and never run their callbacks (in
  // reality, callbacks run within GetGCMBackoffPolicy().maximum_backoff_ms).
  void set_offline(bool is_offline) { is_offline_ = is_offline; }

 private:
  class CustomFakeGCMDriver;
  friend class CustomFakeGCMDriver;

  bool collect_ = false;
  bool is_offline_ = false;

  std::string last_registered_app_id_;
  std::vector<std::string> last_registered_sender_ids_;
  std::list<GCMClient::Result> unregister_responses_;
  OutgoingMessage last_sent_message_;
  std::string last_receiver_id_;
  WebPushMessage last_web_push_message_;

  DISALLOW_COPY_AND_ASSIGN(FakeGCMProfileService);
};

}  // namespace gcm

#endif  // COMPONENTS_GCM_DRIVER_FAKE_GCM_PROFILE_SERVICE_H_
