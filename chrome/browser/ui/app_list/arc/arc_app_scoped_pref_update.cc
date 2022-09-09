// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/app_list/arc/arc_app_scoped_pref_update.h"

#include "base/values.h"

namespace arc {

ArcAppScopedPrefUpdate::ArcAppScopedPrefUpdate(PrefService* service,
                                               const std::string& id,
                                               const std::string& path)
    : DictionaryPrefUpdate(service, path), id_(id) {}

ArcAppScopedPrefUpdate::~ArcAppScopedPrefUpdate() = default;

base::Value* ArcAppScopedPrefUpdate::Get() {
  base::Value* dict = DictionaryPrefUpdate::Get();
  base::Value* dict_item =
      dict->FindKeyOfType(id_, base::Value::Type::DICTIONARY);
  if (!dict_item)
    dict_item = dict->SetKey(id_, base::Value(base::Value::Type::DICTIONARY));
  return dict_item;
}

}  // namespace arc
