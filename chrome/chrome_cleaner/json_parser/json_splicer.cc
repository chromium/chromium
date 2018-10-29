// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/chrome_cleaner/json_parser/json_splicer.h"

#include "base/values.h"

namespace chrome_cleaner {

JsonSplicer::~JsonSplicer() = default;

bool JsonSplicer::RemoveKeyFromDictionary(base::Value* dictionary,
                                          const std::string& key) {
  bool result = false;
  base::DictionaryValue* entries = nullptr;
  if (dictionary == nullptr || !dictionary->is_dict() ||
      !dictionary->GetAsDictionary(&entries)) {
    LOG(ERROR) << "Got a "
               << (dictionary ? dictionary->GetTypeName(dictionary->type())
                              : "NULL")
               << " but expected a dictionary.";
    return result;
  }
  if (!(result = entries->RemoveKey(key))) {
    LOG(ERROR) << key << " was not found in the dictionary";
  }
  return result;
}

bool JsonSplicer::RemoveValueFromList(base::Value* list,
                                      const std::string& key) {
  if (list == nullptr || !list->is_list()) {
    LOG(ERROR) << "Got a " << (list ? list->GetTypeName(list->type()) : "NULL")
               << " but expected a list.";
    return false;
  }
  std::vector<base::Value>& entries = list->GetList();
  auto iter = std::remove(entries.begin(), entries.end(), base::Value(key));
  if (iter == entries.end()) {
    LOG(ERROR) << key << " was not found in the list";
    return false;
  }
  entries.erase(iter);
  return true;
}

}  // namespace chrome_cleaner
