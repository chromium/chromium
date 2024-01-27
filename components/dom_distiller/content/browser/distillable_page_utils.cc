// Copyright 2015 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/dom_distiller/content/browser/distillable_page_utils.h"

#include <string>

#include "base/functional/bind.h"
#include "base/location.h"
#include "base/observer_list.h"
#include "base/strings/utf_string_conversions.h"
#include "base/task/single_thread_task_runner.h"
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
                               base::OnceCallback<void(bool)> callback,
                               base::Value result) {
  std::move(callback).Run(
      detector->Classify(CalculateDerivedFeaturesFromJSON(&result)));
}

}  // namespace

void IsDistillablePageForDetector(content::WebContents* web_contents,
                                  const DistillablePageDetector* detector,
                                  base::OnceCallback<void(bool)> callback) {
  content::RenderFrameHost* main_frame = web_contents->GetPrimaryMainFrame();
  if (!main_frame) {
    base::SingleThreadTaskRunner::GetCurrentDefault()->PostTask(
        FROM_HERE, base::BindOnce(std::move(callback), false));
    return;
  }
  std::string extract_features_js =
      ui::ResourceBundle::GetSharedInstance().LoadDataResourceString(
          IDR_EXTRACT_PAGE_FEATURES_JS);
  RunIsolatedJavaScript(
      main_frame, extract_features_js,
      base::BindOnce(OnExtractFeaturesJsResult, detector, std::move(callback)));
}

bool operator==(const DistillabilityResult& first,
                const DistillabilityResult& second) {
  return first.is_distillable == second.is_distillable &&
         first.is_last == second.is_last &&
         first.is_mobile_friendly == second.is_mobile_friendly;
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

std::optional<DistillabilityResult> GetLatestResult(
    content::WebContents* web_contents) {
  CHECK(web_contents);
  DistillabilityDriver::CreateForWebContents(web_contents);

  DistillabilityDriver* driver =
      DistillabilityDriver::FromWebContents(web_contents);
  CHECK(driver);
  return driver->GetLatestResult();
}

}  // namespace dom_distiller
