// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_TABS_TAB_STRIP_API_TESTING_TOY_TAB_STRIP_TRANSLATION_ADAPTER_H_
#define CHROME_BROWSER_UI_TABS_TAB_STRIP_API_TESTING_TOY_TAB_STRIP_TRANSLATION_ADAPTER_H_

#include "chrome/browser/ui/tabs/tab_strip_api/adapters/translation_adapter.h"
#include "chrome/browser/ui/tabs/tab_strip_api/testing/toy_tab_strip.h"

namespace tabs_api::testing {

class ToyTabStripTranslationAdapter : public TranslationAdapter {
 public:
  explicit ToyTabStripTranslationAdapter(ToyTabStrip* tab_strip);
  ~ToyTabStripTranslationAdapter() override;

  // TranslationAdapter:
  base::expected<mojom::TabPtr, mojo_base::mojom::ErrorPtr> ToMojoTab(
      tabs::TabHandle handle) override;

 private:
  raw_ptr<ToyTabStrip> tab_strip_;
};

}  // namespace tabs_api::testing

#endif  // CHROME_BROWSER_UI_TABS_TAB_STRIP_API_TESTING_TOY_TAB_STRIP_TRANSLATION_ADAPTER_H_
