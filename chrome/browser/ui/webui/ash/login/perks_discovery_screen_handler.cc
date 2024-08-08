// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/webui/ash/login/perks_discovery_screen_handler.h"

#include "chrome/browser/ui/webui/ash/login/base_screen_handler.h"
#include "chrome/grit/generated_resources.h"
#include "components/login/localized_values_builder.h"

namespace ash {

PerksDiscoveryScreenHandler::PerksDiscoveryScreenHandler()
    : BaseScreenHandler(kScreenId) {}

PerksDiscoveryScreenHandler::~PerksDiscoveryScreenHandler() = default;

void PerksDiscoveryScreenHandler::DeclareLocalizedValues(
    ::login::LocalizedValuesBuilder* builder) {
  builder->Add("perksLoading", IDS_LOGIN_PERKS_SCREEN_LOADING);
}

void PerksDiscoveryScreenHandler::Show() {
  ShowInWebUI();
}

void PerksDiscoveryScreenHandler::SetPerksData(
    const std::vector<SinglePerkDiscoveryPayload>& perks) {
  base::Value::List perks_list;
  for (const auto& perk : perks) {
    base::Value::Dict perk_dict;
    perk_dict.Set("perkId", base::Value(perk.id));
    perk_dict.Set("title", base::Value(perk.title));
    perk_dict.Set("subtitle", base::Value(perk.subtitle));
    perk_dict.Set("iconUrl", base::Value(perk.icon_url));
    if (perk.additional_text.has_value()) {
      perk_dict.Set("additionalText",
                    base::Value(perk.additional_text.value()));
    }
    if (perk.content.illustration.has_value()) {
      perk_dict.Set("illustrationUrl", base::Value(perk.content.illustration->url));
      perk_dict.Set("illustrationWidth", base::Value(perk.content.illustration->width));
      perk_dict.Set("illustrationHeight",
                    base::Value(perk.content.illustration->height));
    }
    perk_dict.Set("primaryButtonLabel",
                  base::Value(*perk.primary_button.FindString("label")));
    perk_dict.Set("secondaryButtonLabel",
                  base::Value(*perk.secondary_button.FindString("label")));
    perks_list.Append(std::move(perk_dict));
  }
  CallExternalAPI("setPerksData", std::move(perks_list));
}

void PerksDiscoveryScreenHandler::SetOverviewStep() {
  CallExternalAPI("setOverviewStep");
}

base::WeakPtr<PerksDiscoveryScreenView>
PerksDiscoveryScreenHandler::AsWeakPtr() {
  return weak_ptr_factory_.GetWeakPtr();
}

}  // namespace ash
