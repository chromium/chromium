// Copyright 2013 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/dom_distiller/core/distiller_page.h"

#include <stddef.h>

#include <utility>

#include "base/debug/dump_without_crashing.h"
#include "base/functional/bind.h"
#include "base/json/json_writer.h"
#include "base/location.h"
#include "base/logging.h"
#include "base/metrics/histogram_macros.h"
#include "base/strings/string_util.h"
#include "base/strings/utf_string_conversions.h"
#include "base/task/single_thread_task_runner.h"
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

  base::Value options_value =
      dom_distiller::proto::json::DomDistillerOptions::WriteToValue(options);
  std::string options_json;
  if (!base::JSONWriter::Write(options_value, &options_json)) {
    NOTREACHED_IN_MIGRATION();
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

DistillerPageFactory::~DistillerPageFactory() = default;

DistillerPage::DistillerPage() : ready_(true) {}

DistillerPage::~DistillerPage() = default;

void DistillerPage::DistillPage(
    const GURL& gurl,
    const dom_distiller::proto::DomDistillerOptions options,
    DistillerPageCallback callback) {
  CHECK(ready_, base::NotFatalUntil::M126);
  CHECK(callback, base::NotFatalUntil::M127);
  CHECK(!distiller_page_callback_, base::NotFatalUntil::M127);
  // It is only possible to distill one page at a time. |ready_| is reset when
  // the callback to OnDistillationDone happens.
  ready_ = false;
  distiller_page_callback_ = std::move(callback);
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
            *value, distiller_result.get());
    if (!found_content) {
      DVLOG(1) << "Unable to parse DomDistillerResult.";
    }
  }

  base::SingleThreadTaskRunner::GetCurrentDefault()->PostTask(
      FROM_HERE, base::BindOnce(std::move(distiller_page_callback_),
                                std::move(distiller_result), found_content));
}

}  // namespace dom_distiller
