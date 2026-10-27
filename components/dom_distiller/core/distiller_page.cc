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

  DistillPageImpl(gurl, GetDistillerScript(options));
}

void DistillerPage::OnDistillationDone(const GURL& page_url,
                                       const base::Value* value) {
  DCHECK(!ready_);
  ready_ = true;

  auto distilled_page = std::make_unique<DistilledPageProto>();
  DistillationParseResult result = DistillationParseResult::kParseFailure;

  if (!value || value->is_none()) {
    result = DistillationParseResult::kNoData;
  } else if (ParseDistillationResult(*value, distilled_page.get())) {
    result = DistillationParseResult::kSuccess;
    if (distilled_page->has_word_count() &&
        distilled_page->word_count() <
            GetMinimumAllowableDistilledContentLength()) {
      result = DistillationParseResult::kContentTooShort;
    }
  } else {
    DVLOG(1) << "Unable to parse DistilledPageProto.";
  }

  // Record result for page distillation
  base::UmaHistogramEnumeration("DomDistiller.Distillation.Result", result);

  base::SingleThreadTaskRunner::GetCurrentDefault()->PostTask(
      FROM_HERE, base::BindOnce(std::move(distiller_page_callback_),
                                std::move(distilled_page), result));
}

}  // namespace dom_distiller
