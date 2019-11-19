// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_ARC_TEST_FAKE_INTENT_HELPER_INSTANCE_H_
#define COMPONENTS_ARC_TEST_FAKE_INTENT_HELPER_INSTANCE_H_

#include <map>
#include <string>
#include <vector>

#include "base/callback.h"
#include "components/arc/mojom/intent_helper.mojom.h"

namespace arc {

class FakeIntentHelperInstance : public mojom::IntentHelperInstance {
 public:
  FakeIntentHelperInstance();

  class Broadcast {
   public:
    Broadcast(const std::string& action,
              const std::string& package_name,
              const std::string& cls,
              const std::string& extras);

    ~Broadcast();

    Broadcast(const Broadcast& broadcast);

    std::string action;
    std::string package_name;
    std::string cls;
    std::string extras;
  };

  // Parameters passed to HandleIntent().
  struct HandledIntent {
    HandledIntent(mojom::IntentInfoPtr intent, mojom::ActivityNamePtr activity);
    HandledIntent(HandledIntent&& other);
    HandledIntent& operator=(HandledIntent&& other);
    ~HandledIntent();

    mojom::IntentInfoPtr intent;
    mojom::ActivityNamePtr activity;
  };

  void clear_broadcasts() { broadcasts_.clear(); }
  void clear_handled_intents() { handled_intents_.clear(); }

  const std::vector<Broadcast>& broadcasts() const { return broadcasts_; }
  const std::vector<HandledIntent>& handled_intents() const {
    return handled_intents_;
  }

  std::vector<Broadcast> GetBroadcastsForAction(
      const std::string& action) const;

  // Sets a list of intent handlers to be returned in response to
  // RequestIntentHandlerList() calls with intents containing |action|.
  void SetIntentHandlers(const std::string& action,
                         std::vector<mojom::IntentHandlerInfoPtr> handlers);

  // mojom::IntentHelperInstance:
  ~FakeIntentHelperInstance() override;

  void AddPreferredPackage(const std::string& package_name) override;

  void AddPreferredApp(const std::string& package_name,
                       IntentFilter intent_filter,
                       mojom::IntentInfoPtr intent) override;

  void GetFileSizeDeprecated(const std::string& url,
                             GetFileSizeDeprecatedCallback callback) override;

  void HandleIntent(mojom::IntentInfoPtr intent,
                    mojom::ActivityNamePtr activity) override;

  void HandleUrl(const std::string& url,
                 const std::string& package_name) override;

  void HandleUrlListDeprecated(std::vector<mojom::UrlWithMimeTypePtr> urls,
                               mojom::ActivityNamePtr activity,
                               mojom::ActionType action) override;

  void InitDeprecated(mojom::IntentHelperHostPtr host_ptr) override;

  void Init(mojom::IntentHelperHostPtr host_ptr,
            InitCallback callback) override;

  void OpenFileToReadDeprecated(
      const std::string& url,
      OpenFileToReadDeprecatedCallback callback) override;

  void RequestActivityIcons(std::vector<mojom::ActivityNamePtr> activities,
                            ::arc::mojom::ScaleFactor scale_factor,
                            RequestActivityIconsCallback callback) override;

  void RequestIntentHandlerList(
      mojom::IntentInfoPtr intent,
      RequestIntentHandlerListCallback callback) override;

  void RequestUrlHandlerList(const std::string& url,
                             RequestUrlHandlerListCallback callback) override;

  void RequestUrlListHandlerList(
      std::vector<mojom::UrlWithMimeTypePtr> urls,
      RequestUrlListHandlerListCallback callback) override;

  void SendBroadcast(const std::string& action,
                     const std::string& package_name,
                     const std::string& cls,
                     const std::string& extras) override;

  void ClassifySelectionDeprecated(
      const std::string& text,
      ::arc::mojom::ScaleFactor scale_factor,
      ClassifySelectionDeprecatedCallback callback) override;

  void RequestTextSelectionActions(
      const std::string& text,
      ::arc::mojom::ScaleFactor scale_factor,
      RequestTextSelectionActionsCallback callback) override;

  void HandleCameraResult(uint32_t intent_id,
                          arc::mojom::CameraIntentAction action,
                          const std::vector<uint8_t>& data,
                          HandleCameraResultCallback callback) override;

 private:
  std::vector<Broadcast> broadcasts_;

  // Information about calls to HandleIntent().
  std::vector<HandledIntent> handled_intents_;

  // Map from action names to intent handlers to be returned by
  // RequestIntentHandlerList().
  std::map<std::string, std::vector<mojom::IntentHandlerInfoPtr>>
      intent_handlers_;

  // Keeps the binding alive so that calls to this class can be correctly
  // routed.
  mojom::IntentHelperHostPtr host_;

  DISALLOW_COPY_AND_ASSIGN(FakeIntentHelperInstance);
};

}  // namespace arc

#endif  // COMPONENTS_ARC_TEST_FAKE_INTENT_HELPER_INSTANCE_H_
