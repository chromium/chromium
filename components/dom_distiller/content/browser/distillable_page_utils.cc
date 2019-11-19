// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/dom_distiller/content/browser/distillable_page_utils.h"

#include <string>

#include "base/bind.h"
#include "base/location.h"
#include "base/single_thread_task_runner.h"
#include "base/strings/utf_string_conversions.h"
#include "base/threading/thread_task_runner_handle.h"
#include "base/values.h"
#include "components/dom_distiller/content/browser/distillability_driver.h"
#include "components/dom_distiller/content/browser/distiller_javascript_utils.h"
#include "components/dom_distiller/core/distillable_page_detector.h"
#include "components/dom_distiller/core/experiments.h"
#include "components/dom_distiller/core/page_features.h"
#include "components/grit/components_resources.h"
#include "content/public/browser/render_frame_host.h"
#include "content/public/browser/web_contents.h"
#include "ui/base/resource/resource_bundle.h"

namespace dom_distiller {
namespace {

void OnExtractFeaturesJsResult(const DistillablePageDetector* detector,
                               base::Callback<void(bool)> callback,
                               base::Value result) {
  callback.Run(detector->Classify(CalculateDerivedFeaturesFromJSON(&result)));
}

}  // namespace

void IsDistillablePageForDetector(content::WebContents* web_contents,
                                  const DistillablePageDetector* detector,
                                  base::Callback<void(bool)> callback) {
  content::RenderFrameHost* main_frame = web_contents->GetMainFrame();
  if (!main_frame) {
    base::ThreadTaskRunnerHandle::Get()->PostTask(
        FROM_HERE, base::BindOnce(callback, false));
    return;
  }
  std::string extract_features_js =
      ui::ResourceBundle::GetSharedInstance().LoadDataResourceString(
          IDR_EXTRACT_PAGE_FEATURES_JS);
  RunIsolatedJavaScript(
      main_frame, extract_features_js,
      base::BindOnce(OnExtractFeaturesJsResult, detector, callback));
}

std::ostream& operator<<(std::ostream& os, const DistillabilityResult& result) {
  os << "DistillabilityResult: { is_distillable: " << result.is_distillable
     << ", is_last: " << result.is_last
     << ", is_mobile_friendly: " << result.is_mobile_friendly << " }";
  return os;
}

void AddObserver(content::WebContents* web_contents,
                 DistillabilityObserver* observer) {
  DCHECK(observer);
  CHECK(web_contents);
  DistillabilityDriver::CreateForWebContents(web_contents);

  DistillabilityDriver* driver =
      DistillabilityDriver::FromWebContents(web_contents);
  CHECK(driver);
  base::ObserverList<DistillabilityObserver>* observer_list =
      driver->GetObserverList();
  if (!observer_list->HasObserver(observer)) {
    observer_list->AddObserver(observer);
  }
}

void RemoveObserver(content::WebContents* web_contents,
                    DistillabilityObserver* observer) {
  DCHECK(observer);
  CHECK(web_contents);
  DistillabilityDriver::CreateForWebContents(web_contents);

  DistillabilityDriver* driver =
      DistillabilityDriver::FromWebContents(web_contents);
  CHECK(driver);
  base::ObserverList<DistillabilityObserver>* observer_list =
      driver->GetObserverList();
  if (observer_list->HasObserver(observer)) {
    observer_list->RemoveObserver(observer);
  }
}

base::Optional<DistillabilityResult> GetLatestResult(
    content::WebContents* web_contents) {
  CHECK(web_contents);
  DistillabilityDriver::CreateForWebContents(web_contents);

  DistillabilityDriver* driver =
      DistillabilityDriver::FromWebContents(web_contents);
  CHECK(driver);
  return driver->GetLatestResult();
}

}  // namespace dom_distiller
