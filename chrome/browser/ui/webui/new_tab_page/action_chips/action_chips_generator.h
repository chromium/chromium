// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_WEBUI_NEW_TAB_PAGE_ACTION_CHIPS_ACTION_CHIPS_GENERATOR_H_
#define CHROME_BROWSER_UI_WEBUI_NEW_TAB_PAGE_ACTION_CHIPS_ACTION_CHIPS_GENERATOR_H_

#include <vector>

#include "base/functional/callback_forward.h"
#include "base/types/optional_ref.h"
#include "chrome/browser/ui/webui/new_tab_page/action_chips/action_chips.mojom-forward.h"
#include "chrome/browser/ui/webui/new_tab_page/action_chips/tab_id_generator.h"

// An interface for the class responsible for generating the action chips given
// a tab.
class ActionChipsGenerator {
 public:
  virtual ~ActionChipsGenerator() = default;

  virtual void GenerateActionChips(
      base::optional_ref<const tabs::TabInterface> tab,
      base::OnceCallback<void(std::vector<action_chips::mojom::ActionChipPtr>)>
          callback) = 0;
};

// An implementation of the interface above.
class ActionChipsGeneratorImpl : public ActionChipsGenerator {
 public:
  explicit ActionChipsGeneratorImpl(const TabIdGenerator* tab_id_generator);
  ~ActionChipsGeneratorImpl() override;

  // Not copiable
  ActionChipsGeneratorImpl(const ActionChipsGeneratorImpl&) = delete;
  ActionChipsGeneratorImpl& operator=(const ActionChipsGeneratorImpl&) = delete;

  void GenerateActionChips(
      base::optional_ref<const tabs::TabInterface> tab,
      base::OnceCallback<void(std::vector<action_chips::mojom::ActionChipPtr>)>
          callback) override;

 private:
  raw_ptr<const TabIdGenerator> tab_id_generator_;
};

#endif  // CHROME_BROWSER_UI_WEBUI_NEW_TAB_PAGE_ACTION_CHIPS_ACTION_CHIPS_GENERATOR_H_
