// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/dom_distiller/content/browser/distillability_driver.h"

#include <memory>
#include <utility>

#include "base/bind.h"
#include "build/build_config.h"
#include "content/public/browser/navigation_handle.h"
#include "content/public/browser/render_frame_host.h"
#include "content/public/browser/web_contents.h"
#include "content/public/browser/web_contents_observer.h"
#include "content/public/browser/web_contents_user_data.h"
#include "mojo/public/cpp/bindings/self_owned_receiver.h"

namespace dom_distiller {

// Implementation of the Mojo DistillabilityService. This is called by the
// renderer to notify the browser that a page is distillable.
class DistillabilityServiceImpl : public mojom::DistillabilityService {
 public:
  explicit DistillabilityServiceImpl(
      base::WeakPtr<DistillabilityDriver> distillability_driver)
      : distillability_driver_(distillability_driver) {}

  ~DistillabilityServiceImpl() override = default;

  void NotifyIsDistillable(bool is_distillable,
                           bool is_last_update,
                           bool is_mobile_friendly) override {
    if (!distillability_driver_)
      return;
    DistillabilityResult result;
    result.is_distillable = is_distillable;
    result.is_last = is_last_update;
    result.is_mobile_friendly = is_mobile_friendly;
    DVLOG(1) << "Notifying observers of distillability service result: "
             << result;
    distillability_driver_->OnDistillability(result);
  }

 private:
  base::WeakPtr<DistillabilityDriver> distillability_driver_;
};

DistillabilityDriver::DistillabilityDriver(content::WebContents* web_contents)
    : latest_result_(base::nullopt), web_contents_(web_contents) {
  if (!web_contents)
    return;
}

DistillabilityDriver::~DistillabilityDriver() = default;

void DistillabilityDriver::CreateDistillabilityService(
    mojo::PendingReceiver<mojom::DistillabilityService> receiver) {
  mojo::MakeSelfOwnedReceiver(
      std::make_unique<DistillabilityServiceImpl>(weak_factory_.GetWeakPtr()),
      std::move(receiver));
}

void DistillabilityDriver::SetIsSecureCallback(
    base::RepeatingCallback<bool(content::WebContents*)> is_secure_check) {
  is_secure_check_ = std::move(is_secure_check);
}

void DistillabilityDriver::OnDistillability(
    const DistillabilityResult& result) {
#if !defined(OS_ANDROID)
  if (result.is_distillable) {
    if (!is_secure_check_ || !is_secure_check_.Run(web_contents_)) {
      DistillabilityResult not_distillable;
      not_distillable.is_distillable = false;
      not_distillable.is_last = result.is_last;
      not_distillable.is_mobile_friendly = result.is_mobile_friendly;
      latest_result_ = not_distillable;
      for (auto& observer : observers_)
        observer.OnResult(not_distillable);
      return;
    }
  }
#endif  // !defined(OS_ANDROID)
  latest_result_ = result;
  for (auto& observer : observers_)
    observer.OnResult(result);
}

WEB_CONTENTS_USER_DATA_KEY_IMPL(DistillabilityDriver)

}  // namespace dom_distiller
