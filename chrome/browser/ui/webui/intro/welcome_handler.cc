// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/webui/intro/welcome_handler.h"

#include <optional>
#include <utility>

#include "base/check.h"
#include "base/check_is_test.h"
#include "base/functional/bind.h"
#include "chrome/browser/metrics/metrics_reporting_state.h"
#include "chrome/browser/shell_integration.h"

WelcomeHandler::WelcomeHandler(
    base::OnceClosure callback,
    mojo::PendingReceiver<intro::mojom::WelcomePageHandler> receiver)
    : callback_(std::move(callback)), receiver_(this, std::move(receiver)) {
  CHECK(callback_);
}

WelcomeHandler::WelcomeHandler(
    base::OnceClosure callback,
    mojo::PendingReceiver<intro::mojom::WelcomePageHandler> receiver,
    base::OnceClosure on_set_as_default_completed_callback,
    base::OnceCallback<void(bool)>
        on_change_metrics_reporting_state_callback)
    : WelcomeHandler(std::move(callback), std::move(receiver)) {
  CHECK_IS_TEST();
  on_set_as_default_completed_callback_for_testing_ =
      std::move(on_set_as_default_completed_callback);
  on_change_metrics_reporting_state_callback_for_testing_ =
      std::move(on_change_metrics_reporting_state_callback);
}

WelcomeHandler::~WelcomeHandler() = default;

void WelcomeHandler::Continue(std::optional<bool> is_uma_opt_in,
                              std::optional<bool> is_default_browser) {
  if (!callback_) {
    return;
  }

  if (is_uma_opt_in.has_value()) {
    if (on_change_metrics_reporting_state_callback_for_testing_) {
      std::move(on_change_metrics_reporting_state_callback_for_testing_)
          .Run(*is_uma_opt_in);
    } else {
      metrics::ChangeMetricsReportingState(
          *is_uma_opt_in,
          metrics::ChangeMetricsReportingStateCalledFrom::kUiFirstRun);
    }
  }

  if (is_default_browser.has_value() && *is_default_browser) {
    base::MakeRefCounted<shell_integration::DefaultBrowserWorker>()
        ->StartSetAsDefault(base::BindOnce(
            [](base::OnceClosure on_completed_callback,
               shell_integration::DefaultWebClientState state) {
              if (on_completed_callback) {
                std::move(on_completed_callback).Run();
              }
            },
            std::move(on_set_as_default_completed_callback_for_testing_)));
    // TODO(crbug.com/542895787): Record metrics
  }

  std::move(callback_).Run();
}
