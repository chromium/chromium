// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/dom_distiller/core/distiller_page.h"

#include <stddef.h>

#include <utility>

#include "base/bind.h"
#include "base/json/json_writer.h"
#include "base/location.h"
#include "base/logging.h"
#include "base/metrics/histogram_macros.h"
#include "base/single_thread_task_runner.h"
#include "base/strings/string_util.h"
#include "base/strings/utf_string_conversions.h"
#include "base/threading/thread_task_runner_handle.h"
#include "base/time/time.h"
#include "components/grit/components_resources.h"
#include "third_party/dom_distiller_js/dom_distiller.pb.h"
#include "third_party/dom_distiller_js/dom_distiller_json_converter.h"
#include "ui/base/resource/resource_bundle.h"
#include "url/gurl.h"

namespace dom_distiller {

namespace {

const char* kOptionsPlaceholder = "$$OPTIONS";
const char* kStringifyPlaceholder = "$$STRINGIFY";

std::string GetDistillerScriptWithOptions(
    const dom_distiller::proto::DomDistillerOptions& options,
    bool stringify_output) {
  std::string script =
      ui::ResourceBundle::GetSharedInstance().LoadDataResourceString(
          IDR_DISTILLER_JS);
  if (script.empty()) {
    return "";
  }

  std::unique_ptr<base::Value> options_value(
      dom_distiller::proto::json::DomDistillerOptions::WriteToValue(options));
  std::string options_json;
  if (!base::JSONWriter::Write(*options_value, &options_json)) {
    NOTREACHED();
  }
  size_t options_offset = script.find(kOptionsPlaceholder);
  DCHECK_NE(std::string::npos, options_offset);
  DCHECK_EQ(std::string::npos,
            script.find(kOptionsPlaceholder, options_offset + 1));
  script =
      script.replace(options_offset, strlen(kOptionsPlaceholder), options_json);

  std::string stringify = stringify_output ? "true" : "false";
  size_t stringify_offset = script.find(kStringifyPlaceholder);
  DCHECK_NE(std::string::npos, stringify_offset);
  DCHECK_EQ(std::string::npos,
            script.find(kStringifyPlaceholder, stringify_offset + 1));
  script = script.replace(stringify_offset, strlen(kStringifyPlaceholder),
                          stringify);

  return script;
}

}  // namespace

DistillerPageFactory::~DistillerPageFactory() {}

DistillerPage::DistillerPage() : ready_(true) {}

DistillerPage::~DistillerPage() {}

void DistillerPage::DistillPage(
    const GURL& gurl,
    const dom_distiller::proto::DomDistillerOptions options,
    const DistillerPageCallback& callback) {
  DCHECK(ready_);
  // It is only possible to distill one page at a time. |ready_| is reset when
  // the callback to OnDistillationDone happens.
  ready_ = false;
  distiller_page_callback_ = callback;
  distillation_start_ = base::TimeTicks::Now();
  DistillPageImpl(gurl,
                  GetDistillerScriptWithOptions(options, StringifyOutput()));
}

void DistillerPage::OnDistillationDone(const GURL& page_url,
                                       const base::Value* value) {
  DCHECK(!ready_);
  ready_ = true;

  std::unique_ptr<dom_distiller::proto::DomDistillerResult> distiller_result(
      new dom_distiller::proto::DomDistillerResult());
  bool found_content;
  if (value->is_none()) {
    found_content = false;
  } else {
    found_content =
        dom_distiller::proto::json::DomDistillerResult::ReadFromValue(
            value, distiller_result.get());
    if (!found_content) {
      DVLOG(1) << "Unable to parse DomDistillerResult.";
    } else {
      base::TimeDelta distillation_time =
          base::TimeTicks::Now() - distillation_start_;
      UMA_HISTOGRAM_TIMES("DomDistiller.Time.DistillPage", distillation_time);
      VLOG(1) << "DomDistiller.Time.DistillPage = " << distillation_time;

      if (distiller_result->has_timing_info()) {
        const dom_distiller::proto::TimingInfo& timing =
            distiller_result->timing_info();
        if (timing.has_markup_parsing_time()) {
          UMA_HISTOGRAM_TIMES(
              "DomDistiller.Time.MarkupParsing",
              base::TimeDelta::FromMillisecondsD(timing.markup_parsing_time()));
        }
        if (timing.has_document_construction_time()) {
          UMA_HISTOGRAM_TIMES("DomDistiller.Time.DocumentConstruction",
                              base::TimeDelta::FromMillisecondsD(
                                  timing.document_construction_time()));
        }
        if (timing.has_article_processing_time()) {
          UMA_HISTOGRAM_TIMES("DomDistiller.Time.ArticleProcessing",
                              base::TimeDelta::FromMillisecondsD(
                                  timing.article_processing_time()));
        }
        if (timing.has_formatting_time()) {
          UMA_HISTOGRAM_TIMES(
              "DomDistiller.Time.Formatting",
              base::TimeDelta::FromMillisecondsD(timing.formatting_time()));
        }
        if (timing.has_total_time()) {
          UMA_HISTOGRAM_TIMES(
              "DomDistiller.Time.DistillationTotal",
              base::TimeDelta::FromMillisecondsD(timing.total_time()));
          VLOG(1) << "DomDistiller.Time.DistillationTotal = "
                  << base::TimeDelta::FromMillisecondsD(timing.total_time());
        }
      }
      if (distiller_result->has_statistics_info()) {
        const dom_distiller::proto::StatisticsInfo& statistics =
            distiller_result->statistics_info();
        if (statistics.has_word_count()) {
          UMA_HISTOGRAM_CUSTOM_COUNTS("DomDistiller.Statistics.WordCount",
                                      statistics.word_count(), 1, 4000, 50);
        }
      }
    }
  }

  base::ThreadTaskRunnerHandle::Get()->PostTask(
      FROM_HERE, base::BindOnce(distiller_page_callback_,
                                std::move(distiller_result), found_content));
}

}  // namespace dom_distiller
