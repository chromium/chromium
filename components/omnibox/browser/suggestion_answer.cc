// Copyright 2014 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/omnibox/browser/suggestion_answer.h"

#include <stddef.h>

#include <memory>

#include "base/feature_list.h"
#include "base/i18n/rtl.h"
#include "base/metrics/histogram_macros.h"
#include "base/strings/escape.h"
#include "base/strings/string_number_conversions.h"
#include "base/strings/string_util.h"
#include "base/strings/utf_string_conversions.h"
#include "base/trace_event/memory_usage_estimator.h"
#include "build/build_config.h"
#include "third_party/omnibox_proto/answer_type.pb.h"
#include "url/url_constants.h"

namespace {

// All of these are defined here (even though most are only used once each) so
// the format details are easy to locate and update or compare to the spec doc.

constexpr char kAnswerUsedUmaHistogramName[] =
    "Omnibox.SuggestionUsed.AnswerInSuggest";

}  // namespace

namespace omnibox::answer_data_parser {

// If necessary, concatenate scheme and host/path using only ':' as
// separator. This is due to the results delivering strings of the form
// "//host/path", which is web-speak for "use the enclosing page's scheme",
// but not a valid path of a URL. The GWS frontend commonly (always?)
// redirects to HTTPS, so we just default to that here.
GURL GetFormattedURL(const std::string* url_string) {
  return GURL(base::StartsWith(*url_string, "//", base::CompareCase::SENSITIVE)
                  ? (std::string(url::kHttpsScheme) + ":" + *url_string)
                  : *url_string);
}

void LogAnswerUsed(omnibox::AnswerType answer_type) {
  UMA_HISTOGRAM_ENUMERATION(kAnswerUsedUmaHistogramName, answer_type,
                            omnibox::AnswerType_MAX);
}

}  // namespace omnibox::answer_data_parser
