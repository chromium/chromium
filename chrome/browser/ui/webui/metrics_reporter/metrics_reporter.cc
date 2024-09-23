// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/webui/metrics_reporter/metrics_reporter.h"

#include "base/metrics/histogram_functions.h"

MetricsReporter::MetricsReporter() = default;

MetricsReporter::~MetricsReporter() = default;

void MetricsReporter::Mark(const std::string& name) {
  marks_[name] = base::TimeTicks::Now();
}

void MetricsReporter::Measure(const std::string& start_mark,
                              MeasureCallback callback) {
  MeasureInternal(start_mark, std::nullopt, std::move(callback));
}

void MetricsReporter::Measure(const std::string& start_mark,
                              const std::string& end_mark,
                              MeasureCallback callback) {
  return MeasureInternal(start_mark, std::make_optional(end_mark),
                         std::move(callback));
}

void MetricsReporter::MeasureInternal(const std::string& start_mark,
                                      std::optional<std::string> end_mark,
                                      MeasureCallback callback) {
  const base::TimeTicks end_time =
      end_mark ? marks_[*end_mark] : base::TimeTicks::Now();

  if (marks_.count(start_mark)) {
    std::move(callback).Run(end_time - marks_[start_mark]);
    return;
  }

  DCHECK(page_.is_bound());
  page_->OnGetMark(
      start_mark,
      base::BindOnce(
          [](MeasureCallback callback, base::TimeTicks end_time,
             std::string start_mark,
             std::optional<base::TimeDelta> start_time_since_epoch) {
            if (!start_time_since_epoch) {
              LOG(WARNING) << "Mark \"" << start_mark << "\" does not exists.";
              return;
            }
            base::TimeTicks start_time =
                base::TimeTicks() + *start_time_since_epoch;
            std::move(callback).Run(end_time - start_time);
          },
          std::move(callback), end_time, start_mark));
}

void MetricsReporter::HasMark(const std::string& name,
                              HasMarkCallback callback) {
  if (marks_.count(name)) {
    std::move(callback).Run(true);
    return;
  }

  page_->OnGetMark(name, base::BindOnce(
                             [](HasMarkCallback callback,
                                std::optional<base::TimeDelta> time) {
                               std::move(callback).Run(time.has_value());
                             },
                             std::move(callback)));
}

bool MetricsReporter::HasLocalMark(const std::string& name) {
  return marks_.count(name) > 0;
}

void MetricsReporter::ClearMark(const std::string& name) {
  marks_.erase(name);
  page_->OnClearMark(name);
}

void MetricsReporter::BindInterface(
    mojo::PendingReceiver<metrics_reporter::mojom::PageMetricsHost> receiver) {
  host_.reset();
  host_.Bind(std::move(receiver));
}

void MetricsReporter::OnPageRemoteCreated(
    mojo::PendingRemote<metrics_reporter::mojom::PageMetrics> page) {
  page_.reset();
  page_.Bind(std::move(page));
}

void MetricsReporter::OnGetMark(const std::string& name,
                                OnGetMarkCallback callback) {
  std::move(callback).Run(marks_.count(name)
                              ? std::make_optional(marks_[name].since_origin())
                              : std::nullopt);
}

void MetricsReporter::OnClearMark(const std::string& name) {
  marks_.erase(name);
}

void MetricsReporter::OnUmaReportTime(const std::string& name,
                                      base::TimeDelta time) {
  base::UmaHistogramTimes(name, time);
}
