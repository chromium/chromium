// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/contextual_cueing/contextual_cueing_utils.h"

#include "base/no_destructor.h"
#include "third_party/re2/src/re2/re2.h"
#include "url/gurl.h"

namespace contextual_cueing {

namespace {

const char kHomepagePathPattern[] =
    "(?i)(/((us/en|en)\\b/?)?((index|default|home|homepage|main|welcome)(\\.[^/"
    "?;]+)?)?)?";

}  // namespace

bool IsHomepageUrl(const GURL& url) {
  if (!url.is_valid()) {
    return false;
  }
  static const base::NoDestructor<re2::RE2> kHomepagePathRegex(
      kHomepagePathPattern);
  return RE2::FullMatch(url.path(), *kHomepagePathRegex);
}

}  // namespace contextual_cueing
