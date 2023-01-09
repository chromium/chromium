// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_CHROME_CLEANER_PARSERS_JSON_PARSER_JSON_PARSER_API_H_
#define CHROME_CHROME_CLEANER_PARSERS_JSON_PARSER_JSON_PARSER_API_H_

#include "base/functional/callback.h"
#include "base/values.h"
#include "third_party/abseil-cpp/absl/types/optional.h"

namespace chrome_cleaner {

using ParseDoneCallback =
    base::OnceCallback<void(absl::optional<base::Value>,
                            const absl::optional<std::string>&)>;

class JsonParserAPI {
 public:
  virtual ~JsonParserAPI() = default;

  virtual void Parse(const std::string& json, ParseDoneCallback callback) = 0;
};

}  // namespace chrome_cleaner

#endif  // CHROME_CHROME_CLEANER_PARSERS_JSON_PARSER_JSON_PARSER_API_H_
