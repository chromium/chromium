// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/app_list/arc/arc_app_scoped_pref_update.h"

#include "base/values.h"

namespace arc {

ArcAppScopedPrefUpdate::ArcAppScopedPrefUpdate(PrefService* service,
                                               const std::string& id,
                                               const std::string& path)
    : DictionaryPrefUpdateDeprecated(service, path), id_(id) {}

ArcAppScopedPrefUpdate::~ArcAppScopedPrefUpdate() = default;

base::DictionaryValue* ArcAppScopedPrefUpdate::Get() {
  base::DictionaryValue* dict = DictionaryPrefUpdateDeprecated::Get();
  base::Value* dict_item =
      dict->FindKeyOfType(id_, base::Value::Type::DICTIONARY);
  if (!dict_item)
    dict_item = dict->SetKey(id_, base::Value(base::Value::Type::DICTIONARY));
  return static_cast<base::DictionaryValue*>(dict_item);
}

}  // namespace arc
