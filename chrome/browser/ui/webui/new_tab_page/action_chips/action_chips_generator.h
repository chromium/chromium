// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_WEBUI_NEW_TAB_PAGE_ACTION_CHIPS_ACTION_CHIPS_GENERATOR_H_
#define CHROME_BROWSER_UI_WEBUI_NEW_TAB_PAGE_ACTION_CHIPS_ACTION_CHIPS_GENERATOR_H_

#include <vector>

#include "base/functional/callback_forward.h"
#include "base/memory/weak_ptr.h"
#include "base/types/optional_ref.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/ui/webui/new_tab_page/action_chips/action_chips.mojom-forward.h"
#include "chrome/browser/ui/webui/new_tab_page/action_chips/remote_suggestions_service_simple.h"
#include "chrome/browser/ui/webui/new_tab_page/action_chips/tab_id_generator.h"
#include "components/omnibox/browser/aim_eligibility_service.h"
#include "components/omnibox/browser/autocomplete_provider_client.h"
#include "services/network/public/cpp/simple_url_loader.h"

class OptimizationGuideKeyedService;

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
  // ctor for production
  explicit ActionChipsGeneratorImpl(Profile* profile);
  // ctor for testing
  explicit ActionChipsGeneratorImpl(
      const TabIdGenerator* tab_id_generator,
      OptimizationGuideKeyedService* optimization_guide_decider,
      const AimEligibilityService* aim_eligibility_service,
      std::unique_ptr<AutocompleteProviderClient> client,
      std::unique_ptr<action_chips::RemoteSuggestionsServiceSimple>
          remote_suggestions_service_simple);
  ~ActionChipsGeneratorImpl() override;

  // Not copyable nor movable.
  ActionChipsGeneratorImpl(const ActionChipsGeneratorImpl&) = delete;
  ActionChipsGeneratorImpl& operator=(const ActionChipsGeneratorImpl&) = delete;

  void GenerateActionChips(
      base::optional_ref<const tabs::TabInterface> tab,
      base::OnceCallback<void(std::vector<action_chips::mojom::ActionChipPtr>)>
          callback) override;

 private:
  void GenerateDeepDiveChipsFromRemoteResponse(
      action_chips::mojom::TabInfoPtr tab,
      base::OnceCallback<void(std::vector<action_chips::mojom::ActionChipPtr>)>
          callback,
      action_chips::RemoteSuggestionsServiceSimple::
          ActionChipSuggestionsResult&& result);

  raw_ptr<const TabIdGenerator> tab_id_generator_;
  raw_ptr<OptimizationGuideKeyedService> optimization_guide_decider_;
  raw_ptr<const AimEligibilityService> aim_eligibility_service_;
  std::unique_ptr<AutocompleteProviderClient> client_;
  std::unique_ptr<action_chips::RemoteSuggestionsServiceSimple>
      remote_suggestions_service_simple_;
  std::unique_ptr<network::SimpleURLLoader> loader_;
  base::WeakPtrFactory<ActionChipsGeneratorImpl> weak_factory_{this};
};

#endif  // CHROME_BROWSER_UI_WEBUI_NEW_TAB_PAGE_ACTION_CHIPS_ACTION_CHIPS_GENERATOR_H_
