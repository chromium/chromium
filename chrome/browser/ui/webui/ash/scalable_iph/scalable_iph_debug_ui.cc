// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/webui/ash/scalable_iph/scalable_iph_debug_ui.h"

#include <sstream>
#include <string_view>

#include "ash/constants/ash_features.h"
#include "base/containers/fixed_flat_set.h"
#include "base/memory/ref_counted_memory.h"
#include "base/memory/scoped_refptr.h"
#include "base/strings/strcat.h"
#include "chromeos/ash/components/scalable_iph/logger.h"
#include "chromeos/ash/components/scalable_iph/scalable_iph.h"
#include "chromeos/ash/components/scalable_iph/scalable_iph_factory.h"
#include "content/public/browser/web_contents.h"
#include "content/public/browser/web_ui.h"

namespace ash {

namespace {

constexpr char kLoggingPath[] = "log.txt";
constexpr char kRecordFiveMinTickEventPath[] = "record-five-min-tick-event";
constexpr char kRecordedFiveMinTickEvent[] = "Recorded ScalableIphFiveMinTick.";

constexpr char kPreTagBegin[] = "<pre>";
constexpr char kPreTagEnd[] = "</pre>";
constexpr char kNewline[] = "\n";

constexpr auto kSupportedPaths = base::MakeFixedFlatSet<std::string_view>(
    {kLoggingPath, kRecordFiveMinTickEventPath});

std::string WrapWithPreTags(const std::string& content) {
  return base::StrCat({kPreTagBegin, kNewline, content, kNewline, kPreTagEnd});
}

bool ShouldHandleRequest(const std::string& path) {
  return kSupportedPaths.contains(path);
}

std::string CollectServiceStartUpDebugLog(
    content::BrowserContext* browser_context) {
  std::ostringstream log;
  log << "Failed to get a Scalable Iph service. Collect debug logs for the "
         "service initialization.\n";

  ScalableIphFactory* scalable_iph_factory = ScalableIphFactory::GetInstance();
  if (!scalable_iph_factory) {
    log << "Failed to obtain ScalableIphFactory instance.\n";
    return log.str();
  }

  log << "Call ScalableIphFactory::GetBrowserContextToUse for debugging.\n";
  scalable_iph::Logger logger;
  content::BrowserContext* result =
      scalable_iph_factory->GetBrowserContextToUseForDebug(browser_context,
                                                           &logger);
  log << "Return value of ScalableIphFactory::GetBrowserContextToUseForDebug: "
         "result == nullptr: "
      << (result == nullptr) << "\n";
  log << "Log from ScalableIphFactory::GetBrowserContextToUseForDebug:\n";
  log << logger.GenerateLog() << "\n";

  return log.str();
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
  CHECK(kSupportedPaths.contains(path));

  content::BrowserContext* browser_context =
      web_ui()->GetWebContents()->GetBrowserContext();
  scalable_iph::ScalableIph* scalable_iph =
      ScalableIphFactory::GetForBrowserContext(browser_context);

  if (!scalable_iph) {
    // `ScalableIph` might not be available even if the feature flag is on, e.g.
    // pre-conditions don't get satisfied, querying a service before its
    // initialization, etc.
    std::move(callback).Run(base::MakeRefCounted<base::RefCountedString>(
        WrapWithPreTags(CollectServiceStartUpDebugLog(browser_context))));
    return;
  }

  if (path == kRecordFiveMinTickEventPath) {
    scalable_iph->RecordEvent(scalable_iph::ScalableIph::Event::kFiveMinTick);
    std::move(callback).Run(base::MakeRefCounted<base::RefCountedString>(
        WrapWithPreTags(kRecordedFiveMinTickEvent)));
    return;
  } else if (path == kLoggingPath) {
    std::move(callback).Run(base::MakeRefCounted<base::RefCountedString>(
        WrapWithPreTags(scalable_iph->GetLogger()->GenerateLog())));
    return;
  }
}

}  // namespace ash
