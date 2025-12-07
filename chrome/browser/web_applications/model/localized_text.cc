// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/web_applications/model/localized_text.h"

#include <utility>

#include "base/check.h"
#include "base/values.h"
#include "third_party/blink/public/common/manifest/manifest_util.h"

namespace web_app {

LocalizedText::LocalizedText() = default;

LocalizedText::LocalizedText(
    std::u16string value,
    std::optional<std::u16string> lang,
    std::optional<blink::mojom::Manifest_TextDirection> dir)
    : value_(std::move(value)), lang_(std::move(lang)), dir_(std::move(dir)) {
  CHECK(!value_.empty());
}

LocalizedText::LocalizedText(const LocalizedText&) = default;
LocalizedText& LocalizedText::operator=(const LocalizedText&) = default;
LocalizedText::LocalizedText(LocalizedText&&) = default;
LocalizedText& LocalizedText::operator=(LocalizedText&&) = default;

LocalizedText::~LocalizedText() = default;

bool LocalizedText::empty() const {
  if (lang_.has_value() || dir_.has_value()) {
    CHECK(!value_.empty())
        << "LocalizedText missing a value when lang or dir is populated";
  }
  return value_.empty();
}

base::Value LocalizedText::AsDebugValue() const {
  base::Value::Dict dict;
  dict.Set("value", value_);
  if (lang_.has_value()) {
    dict.Set("lang", lang_.value());
  }
  if (dir_.has_value()) {
    dict.Set("dir", blink::TextDirectionToString(dir_.value()));
  }
  return base::Value(std::move(dict));
}

}  // namespace web_app
