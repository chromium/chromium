// Copyright 2013 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/webui/ash/slow/slow_trace_ui.h"

#include <stddef.h>

#include <memory>

#include "base/functional/bind.h"
#include "base/memory/ref_counted_memory.h"
#include "base/strings/string_number_conversions.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/common/url_constants.h"
#include "components/feedback/content/content_tracing_manager.h"
#include "content/public/browser/url_data_source.h"
#include "content/public/browser/web_ui.h"

namespace ash {

////////////////////////////////////////////////////////////////////////////////
//
// SlowTraceSource
//
////////////////////////////////////////////////////////////////////////////////

SlowTraceSource::SlowTraceSource() {
}

std::string SlowTraceSource::GetSource() {
  return chrome::kChromeUISlowTraceHost;
}

void SlowTraceSource::StartDataRequest(
    const GURL& url,
    const content::WebContents::Getter& wc_getter,
    content::URLDataSource::GotDataCallback callback) {
  int trace_id = 0;
  // TODO(crbug.com/40050262): Simplify usages of |path| since |url| is
  // available.
  const std::string path = content::URLDataSource::URLToRequestPath(url);
  size_t pos = path.find('#');
  ContentTracingManager* manager = ContentTracingManager::Get();
  if (!manager ||
      pos == std::string::npos ||
      !base::StringToInt(path.substr(pos + 1), &trace_id)) {
    std::move(callback).Run(nullptr);
    return;
  }
  manager->GetTraceData(
      trace_id, base::BindOnce(&SlowTraceSource::OnGetTraceData,
                               base::Unretained(this), std::move(callback)));
}

std::string SlowTraceSource::GetMimeType(const GURL& url) {
  return "application/zip";
}

SlowTraceSource::~SlowTraceSource() {}

void SlowTraceSource::OnGetTraceData(
    content::URLDataSource::GotDataCallback callback,
    scoped_refptr<base::RefCountedString> trace_data) {
  std::move(callback).Run(trace_data.get());
}

bool SlowTraceSource::AllowCaching() {
  // Should not be cached to reflect dynamically-generated contents that may
  // depend on current settings.
  return false;
}

////////////////////////////////////////////////////////////////////////////////
//
// SlowTraceController
//
////////////////////////////////////////////////////////////////////////////////

SlowTraceController::SlowTraceController(content::WebUI* web_ui)
    : WebUIController(web_ui) {

  // Set up the chrome://slow_trace/ source.
  content::URLDataSource::Add(Profile::FromWebUI(web_ui),
                              std::make_unique<SlowTraceSource>());
}

}  // namespace ash
