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
}

void MetricsHandler::HandleRecordAction(const base::ListValue* args) {
  std::string string_action = base::UTF16ToUTF8(ExtractStringValue(args));
  base::RecordComputedAction(string_action);
}

void MetricsHandler::HandleRecordInHistogram(const base::ListValue* args) {
  std::string histogram_name;
  double value;
  double boundary_value;
  if (!args->GetString(0, &histogram_name) ||
      !args->GetDouble(1, &value) ||
      !args->GetDouble(2, &boundary_value)) {
    NOTREACHED();
    return;
  }

  int int_value = static_cast<int>(value);
  int int_boundary_value = static_cast<int>(boundary_value);
  if (int_boundary_value >= 4000 ||
      int_value > int_boundary_value ||
      int_value < 0) {
    NOTREACHED();
    return;
  }

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
  std::string histogram_name;
  bool value;
  if (!args->GetString(0, &histogram_name) || !args->GetBoolean(1, &value)) {
    NOTREACHED();
    return;
  }

  base::HistogramBase* counter = base::BooleanHistogram::FactoryGet(
      histogram_name, base::HistogramBase::kUmaTargetedHistogramFlag);
  counter->AddBoolean(value);
}

void MetricsHandler::HandleRecordTime(const base::ListValue* args) {
  std::string histogram_name;
  double value;

  if (!args->GetString(0, &histogram_name) ||
      !args->GetDouble(1, &value) ||
      value < 0) {
    NOTREACHED();
    return;
  }

  base::TimeDelta time_value = base::TimeDelta::FromMilliseconds(value);

  base::HistogramBase* counter = base::Histogram::FactoryTimeGet(
      histogram_name, base::TimeDelta::FromMilliseconds(1),
      base::TimeDelta::FromSeconds(10), 50,
      base::HistogramBase::kUmaTargetedHistogramFlag);
  counter->AddTime(time_value);
}

void MetricsHandler::HandleRecordMediumTime(const base::ListValue* args) {
  std::string histogram_name;
  double value;

  if (!args->GetString(0, &histogram_name) || !args->GetDouble(1, &value) ||
      value < 0) {
    NOTREACHED();
    return;
  }

  base::UmaHistogramMediumTimes(histogram_name,
                                base::TimeDelta::FromMilliseconds(value));
}
