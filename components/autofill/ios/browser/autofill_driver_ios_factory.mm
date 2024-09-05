// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "components/autofill/ios/browser/autofill_driver_ios_factory.h"

#import <memory>
#import <ranges>

#import "base/check.h"
#import "components/autofill/ios/browser/autofill_driver_ios.h"
#import "components/autofill/ios/browser/autofill_java_script_feature.h"

namespace autofill {

void AutofillDriverIOSFactory::Observer::OnAutofillDriverFactoryDestroyed(
    AutofillDriverFactory& factory) {
  OnAutofillDriverIOSFactoryDestroyed(
      static_cast<AutofillDriverIOSFactory&>(factory));
}

void AutofillDriverIOSFactory::Observer::OnAutofillDriverCreated(
    AutofillDriverFactory& factory,
    AutofillDriver& driver) {
  OnAutofillDriverIOSCreated(static_cast<AutofillDriverIOSFactory&>(factory),
                             static_cast<AutofillDriverIOS&>(driver));
}

void AutofillDriverIOSFactory::Observer::OnAutofillDriverStateChanged(
    AutofillDriverFactory& factory,
    AutofillDriver& driver,
    LifecycleState old_state,
    LifecycleState new_state) {
  OnAutofillDriverIOSStateChanged(
      static_cast<AutofillDriverIOSFactory&>(factory),
      static_cast<AutofillDriverIOS&>(driver), old_state, new_state);
}

AutofillDriverIOSFactory::AutofillDriverIOSFactory(
    web::WebState* web_state,
    AutofillClient* client,
    id<AutofillDriverIOSBridge> bridge,
    const std::string& app_locale)
    : app_locale_(app_locale),
      client_(client),
      web_state_(web_state),
      bridge_(bridge) {
  web_state_->AddObserver(this);
  GetWebFramesManager().AddObserver(this);
}

AutofillDriverIOSFactory::~AutofillDriverIOSFactory() {
  TearDown();
}

void AutofillDriverIOSFactory::TearDown() {
  if (web_state_) {
    for (const auto& [frame_id, driver] : driver_map_) {
      if (driver) {
        SetLifecycleStateAndNotifyObservers(*driver,
                                            LifecycleState::kPendingDeletion);
      }
    }
    driver_map_.clear();
    for (auto& observer : AutofillDriverFactory::observers()) {
      observer.OnAutofillDriverFactoryDestroyed(*this);
    }
    GetWebFramesManager().RemoveObserver(this);
    web_state_->RemoveObserver(this);
    web_state_ = nullptr;
  }
}

void AutofillDriverIOSFactory::WebStateDestroyed(web::WebState* web_state) {
  TearDown();
}

web::WebFramesManager& AutofillDriverIOSFactory::GetWebFramesManager() {
  CHECK(web_state_);
  auto* web_frames_manager =
      AutofillJavaScriptFeature::GetInstance()->GetWebFramesManager(web_state_);
  CHECK(web_frames_manager) << "Tests must set the WebFramesManager before "
                               "instantiating AutofillDriverIOSFactory";
  return *web_frames_manager;
}

void AutofillDriverIOSFactory::WebFrameBecameAvailable(
    web::WebFramesManager* web_frames_manager,
    web::WebFrame* web_frame) {
  // Remove the null driver for `web_frame` to unblock DriverForFrame() from
  // creating a driver for the available frame.
  // Also clean up the null drivers for deleted WebFrames.
  std::erase_if(driver_map_, [&](const auto& p) {
    const std::string& frame_id = p.first;
    const AutofillDriverIOS* driver = p.second.get();
    return driver == nullptr &&
           (web_frames_manager->GetFrameWithId(frame_id) == nullptr ||
            frame_id == web_frame->GetFrameId());
  });
}

void AutofillDriverIOSFactory::WebFrameBecameUnavailable(
    web::WebFramesManager* web_frames_manager,
    const std::string& frame_id) {
  // Keep a null driver for `frame_id` in the map to block DriverForFrame() from
  // creating a driver for the unavailable frame.
  std::unique_ptr<AutofillDriverIOS>& driver = driver_map_[frame_id];
  if (driver) {
    SetLifecycleStateAndNotifyObservers(*driver,
                                        LifecycleState::kPendingDeletion);
  }
  driver = nullptr;
  DCHECK_EQ(&driver_map_[frame_id], &driver);
}

AutofillDriverIOS* AutofillDriverIOSFactory::DriverForFrame(
    web::WebFrame* web_frame) {
  if (!web_state_) {
    // WebStateDestroyed() has already been fired.
    return nullptr;
  }
  std::string web_frame_id = web_frame->GetFrameId();
  auto [iter, insertion_happened] = driver_map_.emplace(web_frame_id, nullptr);
  std::unique_ptr<AutofillDriverIOS>& driver = iter->second;
  if (insertion_happened) {
    driver = std::make_unique<AutofillDriverIOS>(
        web_state_, web_frame, client_, &router_, bridge_, app_locale_,
        base::PassKey<AutofillDriverIOSFactory>());
    for (auto& observer : observers()) {
      observer.OnAutofillDriverCreated(*this, *driver);
    }
    DCHECK(driver->IsActive());
    SetLifecycleStateAndNotifyObservers(*driver, LifecycleState::kActive);
    DCHECK_EQ(&driver_map_[web_frame_id], &driver);
  }
  // `driver` may be null if WebFrameBecameUnavailable() has been called for its
  // `web_frame` already.
  return driver.get();
}

WEB_STATE_USER_DATA_KEY_IMPL(AutofillDriverIOSFactory)

}  //  namespace autofill
