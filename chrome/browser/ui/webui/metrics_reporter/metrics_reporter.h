// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_WEBUI_METRICS_REPORTER_METRICS_REPORTER_H_
#define CHROME_BROWSER_UI_WEBUI_METRICS_REPORTER_METRICS_REPORTER_H_

#include <optional>
#include <string>

#include "base/time/time.h"
#include "mojo/public/cpp/bindings/pending_receiver.h"
#include "mojo/public/cpp/bindings/pending_remote.h"
#include "mojo/public/cpp/bindings/receiver.h"
#include "mojo/public/cpp/bindings/remote.h"
#include "ui/webui/resources/js/metrics_reporter/metrics_reporter.mojom.h"

class MetricsReporterClient;
class MetricsReporterTest;

// A WebUI utility that measures time between marks.
// See usage at ui/webui/resources/js/metrics_reporter/metrics_reporter.ts
class MetricsReporter : public metrics_reporter::mojom::PageMetricsHost {
 public:
  MetricsReporter();
  ~MetricsReporter() override;

  using MeasureCallback = base::OnceCallback<void(base::TimeDelta delta)>;
  using HasMarkCallback = base::OnceCallback<void(bool)>;

  virtual void Mark(const std::string& name);
  virtual void Measure(const std::string& start_mark, MeasureCallback callback);
  virtual void Measure(const std::string& start_mark,
                       const std::string& end_mark,
                       MeasureCallback callback);
  virtual void HasMark(const std::string& name, HasMarkCallback callback);
  virtual bool HasLocalMark(const std::string& name);
  virtual void ClearMark(const std::string& name);

  void BindInterface(
      mojo::PendingReceiver<metrics_reporter::mojom::PageMetricsHost> receiver);

 protected:
  // metrics_reporter::mojom::PageMetricsHost:
  void OnPageRemoteCreated(
      mojo::PendingRemote<metrics_reporter::mojom::PageMetrics> page) override;
  void OnGetMark(const std::string& name, OnGetMarkCallback callback) override;
  void OnClearMark(const std::string& name) override;
  void OnUmaReportTime(const std::string& name, base::TimeDelta time) override;

 private:
  friend MetricsReporterClient;
  friend MetricsReporterTest;

  void MeasureInternal(const std::string& start_mark,
                       std::optional<std::string> end_mark,
                       MeasureCallback callback);

  std::map<std::string, base::TimeTicks> marks_;

  mojo::Remote<metrics_reporter::mojom::PageMetrics> page_;
  mojo::Receiver<metrics_reporter::mojom::PageMetricsHost> host_{this};
};

#endif  // CHROME_BROWSER_UI_WEBUI_METRICS_REPORTER_METRICS_REPORTER_H_
