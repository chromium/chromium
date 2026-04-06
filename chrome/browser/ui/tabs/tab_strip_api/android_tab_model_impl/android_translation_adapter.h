// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_TABS_TAB_STRIP_API_ANDROID_TAB_MODEL_IMPL_ANDROID_TRANSLATION_ADAPTER_H_
#define CHROME_BROWSER_UI_TABS_TAB_STRIP_API_ANDROID_TAB_MODEL_IMPL_ANDROID_TRANSLATION_ADAPTER_H_

#include "base/memory/raw_ref.h"
#include "chrome/browser/ui/tabs/tab_strip_api/adapters/translation_adapter.h"

class TabModel;

namespace tabs_api {

class AndroidTabStripModelAdapter;

class AndroidTranslationAdapter : public TranslationAdapter {
 public:
  explicit AndroidTranslationAdapter(TabModel* model,
                                     AndroidTabStripModelAdapter& adapter);
  AndroidTranslationAdapter(const AndroidTranslationAdapter&) = delete;
  AndroidTranslationAdapter operator=(const AndroidTranslationAdapter&) =
      delete;
  ~AndroidTranslationAdapter() override;

  // TranslationAdapter:
  base::expected<mojom::TabPtr, mojo_base::mojom::ErrorPtr> ToMojoTab(
      tabs::TabHandle handle) override;
  base::expected<mojom::DataPtr, mojo_base::mojom::ErrorPtr> ToMojoData(
      tabs::TabCollectionHandle handle) override;

 private:
  raw_ref<TabModel> model_;
  raw_ref<AndroidTabStripModelAdapter> adapter_;
};

}  // namespace tabs_api

#endif  // CHROME_BROWSER_UI_TABS_TAB_STRIP_API_ANDROID_TAB_MODEL_IMPL_ANDROID_TRANSLATION_ADAPTER_H_
