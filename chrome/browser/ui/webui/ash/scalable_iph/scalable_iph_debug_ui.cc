// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/webui/ash/scalable_iph/scalable_iph_debug_ui.h"

#include "ash/constants/ash_features.h"
#include "base/memory/ref_counted_memory.h"
#include "base/memory/scoped_refptr.h"
#include "chrome/browser/scalable_iph/scalable_iph_factory.h"
#include "chromeos/ash/components/scalable_iph/scalable_iph.h"
#include "content/public/browser/web_contents.h"
#include "content/public/browser/web_ui.h"

namespace ash {

namespace {

constexpr char kLoggingPath[] = "log.txt";
constexpr char kDebugMessageScalableIphNotAvailable[] =
    "ScalableIph keyed service is not created for this profile.";

bool ShouldHandleRequest(const std::string& path) {
  return path == kLoggingPath;
}

}  // namespace

bool ScalableIphDebugUIConfig::IsWebUIEnabled(
    content::BrowserContext* browser_context) {
  return ash::features::IsScalableIphDebugEnabled();
}

ScalableIphDebugUI::ScalableIphDebugUI(content::WebUI* web_ui)
    : ui::UntrustedWebUIController(web_ui) {
  content::WebUIDataSource* web_ui_data_source =
      content::WebUIDataSource::CreateAndAdd(
          web_ui->GetWebContents()->GetBrowserContext(),
          scalable_iph::kScalableIphDebugURL);
  web_ui_data_source->SetRequestFilter(
      base::BindRepeating(&ShouldHandleRequest),
      base::BindRepeating(&ScalableIphDebugUI::HandleRequest,
                          weak_ptr_factory_.GetWeakPtr()));
}

ScalableIphDebugUI::~ScalableIphDebugUI() = default;

void ScalableIphDebugUI::HandleRequest(
    const std::string& path,
    content::WebUIDataSource::GotDataCallback callback) {
  CHECK_EQ(path, kLoggingPath);

  scalable_iph::ScalableIph* scalable_iph =
      ScalableIphFactory::GetForBrowserContext(
          web_ui()->GetWebContents()->GetBrowserContext());
  if (!scalable_iph) {
    // `ScalableIph` might not be available even if the feature flag is on, e.g.
    // pre-conditions don't get satisfied, querying a service before its
    // initialization, etc.
    std::move(callback).Run(base::MakeRefCounted<base::RefCountedString>(
        kDebugMessageScalableIphNotAvailable));
    return;
  }

  std::move(callback).Run(base::MakeRefCounted<base::RefCountedString>(
      scalable_iph->logger()->GenerateLog()));
}

}  // namespace ash
