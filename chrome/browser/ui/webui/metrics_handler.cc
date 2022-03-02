// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/webui/metrics_handler.h"

#include "base/bind.h"
#include "base/callback_helpers.h"
#include "base/metrics/histogram_functions.h"
#include "base/metrics/histogram_macros.h"
#include "base/metrics/user_metrics.h"
#include "base/notreached.h"
#include "base/strings/utf_string_conversions.h"
#include "base/values.h"
#include "build/build_config.h"
#include "chrome/browser/ui/tab_contents/core_tab_helper.h"
#include "content/public/browser/web_contents.h"
#include "content/public/browser/web_ui.h"

using base::ListValue;
using base::UserMetricsAction;
using content::WebContents;

MetricsHandler::MetricsHandler() {}
MetricsHandler::~MetricsHandler() {}

void MetricsHandler::RegisterMessages() {
  web_ui()->RegisterDeprecatedMessageCallback(
      "metricsHandler:recordAction",
      base::BindRepeating(&MetricsHandler::HandleRecordAction,
                          base::Unretained(this)));
  web_ui()->RegisterDeprecatedMessageCallback(
      "metricsHandler:recordInHistogram",
      base::BindRepeating(&MetricsHandler::HandleRecordInHistogram,
                          base::Unretained(this)));
  web_ui()->RegisterDeprecatedMessageCallback(
      "metricsHandler:recordBooleanHistogram",
      base::BindRepeating(&MetricsHandler::HandleRecordBooleanHistogram,
                          base::Unretained(this)));
  web_ui()->RegisterDeprecatedMessageCallback(
      "metricsHandler:recordTime",
      base::BindRepeating(&MetricsHandler::HandleRecordTime,
                          base::Unretained(this)));
  web_ui()->RegisterDeprecatedMessageCallback(
      "metricsHandler:recordMediumTime",
      base::BindRepeating(&MetricsHandler::HandleRecordMediumTime,
                          base::Unretained(this)));
  web_ui()->RegisterDeprecatedMessageCallback(
      "metricsHandler:recordSparseHistogram",
      base::BindRepeating(&MetricsHandler::HandleRecordSparseHistogram,
                          base::Unretained(this)));
}

void MetricsHandler::HandleRecordAction(const base::ListValue* args) {
  std::string string_action = base::UTF16ToUTF8(ExtractStringValue(args));
  base::RecordComputedAction(string_action);
}

void MetricsHandler::HandleRecordInHistogram(const base::ListValue* args) {
  base::Value::ConstListView list = args->GetListDeprecated();
  const std::string& histogram_name = list[0].GetString();
  int int_value = static_cast<int>(list[1].GetDouble());
  int int_boundary_value = static_cast<int>(list[2].GetDouble());

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

void MetricsHandler::HandleRecordBooleanHistogram(const base::ListValue* args) {
  const auto& list = args->GetListDeprecated();
  if (list.size() < 2 || !list[0].is_string() || !list[1].is_bool()) {
    NOTREACHED();
    return;
  }
  const std::string histogram_name = list[0].GetString();
  const bool value = list[1].GetBool();

  base::HistogramBase* counter = base::BooleanHistogram::FactoryGet(
      histogram_name, base::HistogramBase::kUmaTargetedHistogramFlag);
  counter->AddBoolean(value);
}

void MetricsHandler::HandleRecordTime(const base::ListValue* args) {
  const std::string& histogram_name = args->GetListDeprecated()[0].GetString();
  double value = args->GetListDeprecated()[1].GetDouble();

  DCHECK_GE(value, 0);

  base::TimeDelta time_value = base::Milliseconds(value);

  base::HistogramBase* counter = base::Histogram::FactoryTimeGet(
      histogram_name, base::Milliseconds(1), base::Seconds(10), 50,
      base::HistogramBase::kUmaTargetedHistogramFlag);
  counter->AddTime(time_value);
}

void MetricsHandler::HandleRecordMediumTime(const base::ListValue* args) {
  const std::string& histogram_name = args->GetListDeprecated()[0].GetString();
  double value = args->GetListDeprecated()[1].GetDouble();

  DCHECK_GE(value, 0);

  base::UmaHistogramMediumTimes(histogram_name, base::Milliseconds(value));
}

void MetricsHandler::HandleRecordSparseHistogram(const base::ListValue* args) {
  const std::string& histogram_name = args->GetListDeprecated()[0].GetString();
  int sample = args->GetListDeprecated()[1].GetInt();

  base::UmaHistogramSparse(histogram_name, sample);
}
