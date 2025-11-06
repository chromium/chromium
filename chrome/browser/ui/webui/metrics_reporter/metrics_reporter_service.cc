// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/webui/metrics_reporter/metrics_reporter_service.h"

#include "base/logging.h"
#include "base/metrics/histogram_functions.h"
#include "content/public/browser/page_user_data.h"
#include "content/public/browser/render_frame_host.h"
#include "content/public/browser/web_contents.h"
#include "content/public/browser/web_contents_user_data.h"
#include "mojo/public/cpp/bindings/self_owned_receiver.h"

MetricsReporterService::MetricsReporterService(
    content::WebContents* web_contents)
    : content::WebContentsUserData<MetricsReporterService>(*web_contents),
      metrics_reporter_(std::make_unique<MetricsReporter>()) {}

MetricsReporterService::~MetricsReporterService() = default;

// static
MetricsReporterService* MetricsReporterService::GetFromWebContents(
    content::WebContents* web_contents) {
  MetricsReporterService* service =
      MetricsReporterService::FromWebContents(web_contents);
  if (!service) {
    MetricsReporterService::CreateForWebContents(web_contents);
    service = MetricsReporterService::FromWebContents(web_contents);
  }
  return service;
}

void MetricsReporterService::BindReceiver(
    mojo::PendingReceiver<metrics_reporter::mojom::PageMetricsHost> receiver) {
  metrics_reporter_->BindInterface(std::move(receiver));
}

void MetricsReporterService::SetMetricsReporterForTesting(
    std::unique_ptr<MetricsReporter> metrics_reporter) {
  metrics_reporter_ = std::move(metrics_reporter);
}

WEB_CONTENTS_USER_DATA_KEY_IMPL(MetricsReporterService);
