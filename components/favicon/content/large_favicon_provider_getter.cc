// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/favicon/content/large_favicon_provider_getter.h"

#include "base/no_destructor.h"

namespace favicon {

namespace {

LargeFaviconProviderGetter* GetGetter() {
  static base::NoDestructor<LargeFaviconProviderGetter> getter;
  return getter.get();
}

}  // namespace

void SetLargeFaviconProviderGetter(const LargeFaviconProviderGetter& getter) {
  *GetGetter() = getter;
}

// static
LargeFaviconProvider* GetLargeFaviconProvider(
    content::BrowserContext* context) {
  return GetGetter()->Run(context);
}

}  // namespace favicon
