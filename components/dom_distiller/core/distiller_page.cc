// Copyright 2013 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/dom_distiller/core/distiller_page.h"

#include <stddef.h>

#include <algorithm>
#include <utility>

#include "base/debug/dump_without_crashing.h"
#include "base/feature_list.h"
#include "base/functional/bind.h"
#include "base/json/json_writer.h"
#include "base/location.h"
#include "base/logging.h"
#include "base/metrics/histogram_functions.h"
#include "base/strings/string_util.h"
#include "base/strings/to_string.h"
#include "base/strings/utf_string_conversions.h"
#include "base/task/single_thread_task_runner.h"
#include "base/values.h"
#include "build/build_config.h"
#include "components/dom_distiller/core/dom_distiller_features.h"
#include "components/dom_distiller/core/extraction_utils.h"
#include "components/grit/components_resources.h"
#include "third_party/dom_distiller_js/dom_distiller.pb.h"
#include "third_party/dom_distiller_js/dom_distiller_json_converter.h"
#include "ui/base/resource/resource_bundle.h"
#include "url/gurl.h"

namespace dom_distiller {

namespace {

#if BUILDFLAG(IS_ANDROID)
constexpr int kDefaultMinContentLength = 100;
#else
constexpr int kDefaultMinContentLength = 0;
#endif

}  // namespace

DistillerPageFactory::~DistillerPageFactory() = default;

DistillerPage::DistillerPage()
    : ready_(true), min_content_length_(kDefaultMinContentLength) {}

DistillerPage::~DistillerPage() = default;

void DistillerPage::DistillPage(const GURL& gurl,
                                const DistillerOptions& options,
                                DistillerPageCallback callback) {
  CHECK(ready_);
  CHECK(callback);
  CHECK(!distiller_page_callback_);
  // It is only possible to distill one page at a time. |ready_| is reset when
  // the callback to OnDistillationDone happens.
  ready_ = false;
  distiller_page_callback_ = std::move(callback);

  std::string script;
  switch (GetDistillerType()) {
    case DistillerType::kReadability:
      script = GetReadabilityDistillerScript(options.readability);
      break;
    case DistillerType::kDOMDistiller:
      script = GetDistillerScriptWithOptions(options.dom_distiller);
      break;
  }

  DistillPageImpl(gurl, script);
}

void DistillerPage::DistillPage(
    const GURL& gurl,
    const dom_distiller::proto::DomDistillerOptions dom_distiller_options,
    DistillerPageCallback callback) {
  DistillPage(gurl, DistillerOptions(dom_distiller_options),
              std::move(callback));
}

void DistillerPage::OnDistillationDone(const GURL& page_url,
                                       const base::Value* value) {
  DCHECK(!ready_);
  ready_ = true;

  std::unique_ptr<dom_distiller::proto::DomDistillerResult> distiller_result(
      new dom_distiller::proto::DomDistillerResult());

  DistillationParseResult result = DistillationParseResult::kParseFailure;

  if (!value || value->is_none()) {
    result = DistillationParseResult::kNoData;
  } else {
    bool parsed_successfully = false;
    switch (GetDistillerType()) {
      case DistillerType::kReadability:
        parsed_successfully = ReadabilityDistillerResultToDomDistillerResult(
            *value, distiller_result.get());
        break;
      case DistillerType::kDOMDistiller:
        parsed_successfully =
            dom_distiller::proto::json::DomDistillerResult::ReadFromValue(
                *value, distiller_result.get());
        break;
    }

    if (parsed_successfully) {
      // Assume success unless a specific validation check fails.
      result = DistillationParseResult::kSuccess;

      // Apply a content length check specifically for the Readability
      // distiller.
      if (GetDistillerType() == DistillerType::kReadability) {
        bool content_is_long_enough = true;
        if (distiller_result->has_statistics_info() &&
            distiller_result->statistics_info().has_word_count()) {
          content_is_long_enough =
              distiller_result->statistics_info().word_count() >=
              GetMinimumAllowableDistilledContentLength();
        }

        // If content is too short, update the result state.
        if (!content_is_long_enough) {
          result = DistillationParseResult::kContentTooShort;
        }
      }
    } else {
      // Parsing failed, the default state is already kParseFailure.
      DVLOG(1) << "Unable to parse DomDistillerResult.";
    }
  }

  // Record result for page distillation
  base::UmaHistogramEnumeration("DomDistiller.Distillation.Result", result);

  base::SingleThreadTaskRunner::GetCurrentDefault()->PostTask(
      FROM_HERE, base::BindOnce(std::move(distiller_page_callback_),
                                std::move(distiller_result), result));
}

}  // namespace dom_distiller
