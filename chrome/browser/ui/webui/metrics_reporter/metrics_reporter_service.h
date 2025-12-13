// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_WEBUI_METRICS_REPORTER_METRICS_REPORTER_SERVICE_H_
#define CHROME_BROWSER_UI_WEBUI_METRICS_REPORTER_METRICS_REPORTER_SERVICE_H_

#include "base/memory/weak_ptr.h"
#include "base/observer_list.h"
#include "chrome/browser/ui/webui/metrics_reporter/metrics_reporter.h"
#include "content/public/browser/web_contents.h"
#include "content/public/browser/web_contents_user_data.h"
#include "mojo/public/cpp/bindings/pending_receiver.h"
#include "ui/webui/resources/js/metrics_reporter/metrics_reporter.mojom.h"

class MetricsReporterService
    : public content::WebContentsUserData<MetricsReporterService> {
 public:
  explicit MetricsReporterService(content::WebContents* web_contents);
  MetricsReporterService(const MetricsReporterService&) = delete;
  MetricsReporterService& operator=(const MetricsReporterService&) = delete;

  ~MetricsReporterService() override;

  // Returns the MetricsReportService linked to the WebContents.
  // Installs a MetricsReporterService if it's not already present.
  static MetricsReporterService* GetFromWebContents(
      content::WebContents* web_contents);

  void BindReceiver(
      mojo::PendingReceiver<metrics_reporter::mojom::PageMetricsHost> receiver);

  MetricsReporter* metrics_reporter() { return metrics_reporter_.get(); }

  void SetMetricsReporterForTesting(
      std::unique_ptr<MetricsReporter> metrics_reporter);

 private:
  friend class content::WebContentsUserData<MetricsReporterService>;

  std::unique_ptr<MetricsReporter> metrics_reporter_;

  base::WeakPtrFactory<MetricsReporterService> weak_factory_{this};

  WEB_CONTENTS_USER_DATA_KEY_DECL();
};

#endif  // CHROME_BROWSER_UI_WEBUI_METRICS_REPORTER_METRICS_REPORTER_SERVICE_H_
