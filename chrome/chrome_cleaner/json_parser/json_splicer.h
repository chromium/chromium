// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_CHROME_CLEANER_JSON_PARSER_JSON_SPLICER_H_
#define CHROME_CHROME_CLEANER_JSON_PARSER_JSON_SPLICER_H_

#include "base/values.h"

namespace chrome_cleaner {

class JsonSplicer {
 public:
  ~JsonSplicer();

  // Deletes the |key| entry from the |dictionary|.
  //
  // Returns true on success.
  bool RemoveKeyFromDictionary(base::Value* dictionary, const std::string& key);

  // Deletes the entry from the |list| with |key|.
  //
  // Returns true on success.
  bool RemoveValueFromList(base::Value* list, const std::string& key);
};

}  // namespace chrome_cleaner

#endif  // CHROME_CHROME_CLEANER_JSON_PARSER_JSON_SPLICER_H_
