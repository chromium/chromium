// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/keyed_service/core/simple_key_map.h"

#include "base/check.h"
#include "base/no_destructor.h"

SimpleKeyMap::SimpleKeyMap() = default;

SimpleKeyMap::~SimpleKeyMap() = default;

// static
SimpleKeyMap* SimpleKeyMap::GetInstance() {
  static base::NoDestructor<SimpleKeyMap> provider;
  return provider.get();
}

void SimpleKeyMap::Associate(content::BrowserContext* browser_context,
                             SimpleFactoryKey* key) {
  DCHECK(browser_context);
  DCHECK(key);
  DCHECK(mapping_.find(browser_context) == mapping_.end());
  mapping_[browser_context] = key;
}

SimpleFactoryKey* SimpleKeyMap::GetForBrowserContext(
    content::BrowserContext* browser_context) {
  const auto& it = mapping_.find(browser_context);
  if (it == mapping_.end()) {
    DCHECK(false);
    return nullptr;
  }

  return it->second;
}

void SimpleKeyMap::Dissociate(content::BrowserContext* browser_context) {
  DCHECK(mapping_.find(browser_context) != mapping_.end());
  mapping_.erase(browser_context);
}
