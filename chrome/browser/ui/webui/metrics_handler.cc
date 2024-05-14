// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/webui/metrics_handler.h"

#include "base/functional/bind.h"
#include "base/functional/callback_helpers.h"
#include "base/metrics/histogram_functions.h"
#include "base/metrics/histogram_macros.h"
#include "base/metrics/user_metrics.h"
#include "base/notreached.h"
#include "base/values.h"
#include "build/build_config.h"
#include "chrome/browser/ui/tab_contents/core_tab_helper.h"
#include "content/public/browser/web_contents.h"
#include "content/public/browser/web_ui.h"

MetricsHandler::MetricsHandler() = default;

MetricsHandler::~MetricsHandler() = default;

void MetricsHandler::RegisterMessages() {
  web_ui()->RegisterMessageCallback(
      "metricsHandler:recordAction",
      base::BindRepeating(&MetricsHandler::HandleRecordAction,
                          base::Unretained(this)));
  web_ui()->RegisterMessageCallback(
      "metricsHandler:recordInHistogram",
      base::BindRepeating(&MetricsHandler::HandleRecordInHistogram,
                          base::Unretained(this)));
  web_ui()->RegisterMessageCallback(
      "metricsHandler:recordBooleanHistogram",
      base::BindRepeating(&MetricsHandler::HandleRecordBooleanHistogram,
                          base::Unretained(this)));
  web_ui()->RegisterMessageCallback(
      "metricsHandler:recordTime",
      base::BindRepeating(&MetricsHandler::HandleRecordTime,
                          base::Unretained(this)));
  web_ui()->RegisterMessageCallback(
      "metricsHandler:recordMediumTime",
      base::BindRepeating(&MetricsHandler::HandleRecordMediumTime,
                          base::Unretained(this)));
  web_ui()->RegisterMessageCallback(
      "metricsHandler:recordSparseHistogram",
      base::BindRepeating(&MetricsHandler::HandleRecordSparseHistogram,
                          base::Unretained(this)));
}

void MetricsHandler::HandleRecordAction(const base::Value::List& args) {
  CHECK_EQ(1U, args.size());
  std::string string_action = args[0].GetString();
  base::RecordComputedAction(string_action);
}

void MetricsHandler::HandleRecordInHistogram(const base::Value::List& args) {
  const std::string& histogram_name = args[0].GetString();
  int int_value = static_cast<int>(args[1].GetDouble());
  int int_boundary_value = static_cast<int>(args[2].GetDouble());

  DCHECK_GE(int_value, 0);
  DCHECK_LE(int_value, int_boundary_value);
  DCHECK_LT(int_boundary_value, 4000);

  int bucket_count = int_boundary_value;
  while (bucket_count >= 100) {
    bucket_count /= 10;
  }

  // As |histogram_name| may change between calls, the UMA_HISTOGRAM_ENUMERATION
  // macro cannot be used here.
  base::HistogramBase* counter =
      base::LinearHistogram::FactoryGet(
          histogram_name, 1, int_boundary_value, bucket_count + 1,
          base::HistogramBase::kUmaTargetedHistogramFlag);
  counter->Add(int_value);
}

void MetricsHandler::HandleRecordBooleanHistogram(
    const base::Value::List& args) {
  if (args.size() < 2 || !args[0].is_string() || !args[1].is_bool()) {
    NOTREACHED_IN_MIGRATION();
    return;
  }
  const std::string histogram_name = args[0].GetString();
  const bool value = args[1].GetBool();

  base::HistogramBase* counter = base::BooleanHistogram::FactoryGet(
      histogram_name, base::HistogramBase::kUmaTargetedHistogramFlag);
  counter->AddBoolean(value);
}

void MetricsHandler::HandleRecordTime(const base::Value::List& args) {
  const std::string& histogram_name = args[0].GetString();
  double value = args[1].GetDouble();

  DCHECK_GE(value, 0);

  base::TimeDelta time_value = base::Milliseconds(value);

  base::HistogramBase* counter = base::Histogram::FactoryTimeGet(
      histogram_name, base::Milliseconds(1), base::Seconds(10), 50,
      base::HistogramBase::kUmaTargetedHistogramFlag);
  counter->AddTime(time_value);
}

void MetricsHandler::HandleRecordMediumTime(const base::Value::List& args) {
  const std::string& histogram_name = args[0].GetString();
  double value = args[1].GetDouble();

  DCHECK_GE(value, 0);

  base::UmaHistogramMediumTimes(histogram_name, base::Milliseconds(value));
}

void MetricsHandler::HandleRecordSparseHistogram(
    const base::Value::List& args) {
  const std::string& histogram_name = args[0].GetString();
  int sample = args[1].GetInt();

  base::UmaHistogramSparse(histogram_name, sample);
}
