// Copyright 2015 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/dom_distiller/content/browser/distillability_driver.h"

#include <memory>
#include <utility>

#include "base/functional/bind.h"
#include "base/observer_list.h"
#include "build/build_config.h"
#include "content/public/browser/navigation_handle.h"
#include "content/public/browser/page_user_data.h"
#include "content/public/browser/render_frame_host.h"
#include "content/public/browser/web_contents.h"
#include "content/public/browser/web_contents_observer.h"
#include "content/public/browser/web_contents_user_data.h"
#include "mojo/public/cpp/bindings/self_owned_receiver.h"

namespace dom_distiller {
namespace {
class DistillabilityResultPageData
    : public content::PageUserData<DistillabilityResultPageData> {
 public:
  explicit DistillabilityResultPageData(content::Page& page);

  DistillabilityResultPageData(const DistillabilityResultPageData&) = delete;
  DistillabilityResultPageData& operator=(const DistillabilityResultPageData&) =
      delete;

  ~DistillabilityResultPageData() override;

  DistillabilityResult distillability_result;

  PAGE_USER_DATA_KEY_DECL();
};

DistillabilityResultPageData::DistillabilityResultPageData(content::Page& page)
    : PageUserData<DistillabilityResultPageData>(page) {}
DistillabilityResultPageData::~DistillabilityResultPageData() = default;

PAGE_USER_DATA_KEY_IMPL(DistillabilityResultPageData);

}  // namespace

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
                           bool is_long_article,
                           bool is_mobile_friendly) override {
    if (!distillability_driver_)
      return;
    DistillabilityResult result;
    result.is_distillable = is_distillable;
    result.is_last = is_last_update;
    result.is_long_article = is_long_article;
    result.is_mobile_friendly = is_mobile_friendly;
    DVLOG(1) << "Notifying observers of distillability service result: "
             << result;
    distillability_driver_->OnDistillability(result);
  }

 private:
  base::WeakPtr<DistillabilityDriver> distillability_driver_;
};

DistillabilityDriver::DistillabilityDriver(content::WebContents* web_contents)
    : content::WebContentsUserData<DistillabilityDriver>(*web_contents),
      content::WebContentsObserver(web_contents) {}

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

void DistillabilityDriver::PrimaryPageChanged(content::Page& page) {
  DistillabilityResultPageData* page_data =
      DistillabilityResultPageData::GetForPage(page);
  if (page_data) {
    OnDistillability(page_data->distillability_result);
  }
}

void DistillabilityDriver::OnDistillability(
    const DistillabilityResult& result) {
#if !BUILDFLAG(IS_ANDROID)
  if (result.is_distillable) {
    if (!is_secure_check_ || !is_secure_check_.Run(&GetWebContents())) {
      DistillabilityResult not_distillable;
      not_distillable.is_distillable = false;
      not_distillable.is_last = result.is_last;
      not_distillable.is_long_article = result.is_long_article;
      not_distillable.is_mobile_friendly = result.is_mobile_friendly;
      latest_result_ = not_distillable;
      for (auto& observer : observers_)
        observer.OnResult(not_distillable);
      return;
    }
  }
#endif  // !BUILDFLAG(IS_ANDROID)

  DistillabilityResultPageData::CreateForPage(
      GetWebContents().GetPrimaryPage());
  DistillabilityResultPageData* page_data =
      DistillabilityResultPageData::GetForPage(
          GetWebContents().GetPrimaryPage());
  page_data->distillability_result = result;
  latest_result_ = result;
  for (auto& observer : observers_)
    observer.OnResult(result);
}

WEB_CONTENTS_USER_DATA_KEY_IMPL(DistillabilityDriver);

}  // namespace dom_distiller
