// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_BROWSER_APIS_TAB_STRIP_TESTING_TOY_TAB_STRIP_TRANSLATION_ADAPTER_H_
#define COMPONENTS_BROWSER_APIS_TAB_STRIP_TESTING_TOY_TAB_STRIP_TRANSLATION_ADAPTER_H_

#include "components/browser_apis/tab_strip/adapters/translation_adapter.h"
#include "components/browser_apis/tab_strip/testing/toy_tab_strip.h"

namespace tabs_api::testing {

class ToyTabStripTranslationAdapter : public TranslationAdapter {
 public:
  explicit ToyTabStripTranslationAdapter(ToyTabStrip* tab_strip);
  ~ToyTabStripTranslationAdapter() override;

  // TranslationAdapter:
  base::expected<mojom::TabPtr, mojo_base::mojom::ErrorPtr> ToMojoTab(
      tabs::TabHandle handle) override;
  base::expected<mojom::DataPtr, mojo_base::mojom::ErrorPtr> ToMojoData(
      tabs::TabCollectionHandle handle) override;

 private:
  raw_ptr<ToyTabStrip> tab_strip_;
};

}  // namespace tabs_api::testing

#endif  // COMPONENTS_BROWSER_APIS_TAB_STRIP_TESTING_TOY_TAB_STRIP_TRANSLATION_ADAPTER_H_
