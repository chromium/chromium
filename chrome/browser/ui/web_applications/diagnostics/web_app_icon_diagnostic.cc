// Copyright 2022 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/web_applications/diagnostics/web_app_icon_diagnostic.h"

#include "chrome/browser/web_applications/web_app.h"
#include "chrome/browser/web_applications/web_app_provider.h"
#include "chrome/browser/web_applications/web_app_registrar.h"

namespace web_app {

WebAppIconDiagnostic::WebAppIconDiagnostic(Profile* profile, AppId app_id)
    : profile_(profile), app_id_(std::move(app_id)) {}

WebAppIconDiagnostic::~WebAppIconDiagnostic() = default;

void WebAppIconDiagnostic::Run(
    base::OnceCallback<void(absl::optional<Result>)> result_callback) {
  absl::optional<Result> result;

  auto* provider = WebAppProvider::GetForLocalAppsUnchecked(profile_.get());
  const WebApp* app = provider->registrar().GetAppById(app_id_);
  if (!app) {
    std::move(result_callback).Run(std::move(result));
    return;
  }

  result.emplace();
  result->has_empty_downloaded_icon_sizes =
      app->downloaded_icon_sizes(IconPurpose::ANY).empty();
  result->has_generated_icon_flag = app->is_generated_icon();
  std::move(result_callback).Run(std::move(result));
}

}  // namespace web_app
