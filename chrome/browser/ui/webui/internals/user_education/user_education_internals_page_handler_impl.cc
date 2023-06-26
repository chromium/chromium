// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/webui/internals/user_education/user_education_internals_page_handler_impl.h"
#include <stdint.h>
#include <sstream>

#include "base/feature_list.h"
#include "base/strings/utf_string_conversions.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/browser/ui/browser_finder.h"
#include "chrome/browser/ui/browser_window.h"
#include "chrome/browser/ui/user_education/user_education_service.h"
#include "chrome/browser/ui/user_education/user_education_service_factory.h"
#include "components/user_education/common/feature_promo_registry.h"
#include "components/user_education/common/feature_promo_specification.h"
#include "components/user_education/common/tutorial_description.h"
#include "content/public/browser/web_ui.h"
#include "ui/base/interaction/element_identifier.h"
#include "ui/base/l10n/l10n_util.h"
#include "ui/base/webui/resource_path.h"

namespace {

user_education::TutorialService* GetTutorialService(Profile* profile) {
  auto* service = UserEducationServiceFactory::GetForProfile(profile);
  return service ? &service->tutorial_service() : nullptr;
}

user_education::FeaturePromoRegistry* GetFeaturePromoRegistry(
    Profile* profile) {
  auto* service = UserEducationServiceFactory::GetForProfile(profile);
  return service ? &service->feature_promo_registry() : nullptr;
}

std::string GetPromoTypeString(
    const user_education::FeaturePromoSpecification& spec) {
  switch (spec.promo_type()) {
    case user_education::FeaturePromoSpecification::PromoType::kUnspecified:
      return "Unknown";
    case user_education::FeaturePromoSpecification::PromoType::kCustomAction:
      return "Custom Action";
    case user_education::FeaturePromoSpecification::PromoType::kLegacy:
      return "Legacy Promo";
    case user_education::FeaturePromoSpecification::PromoType::kSnooze:
      return "Snooze";
    case user_education::FeaturePromoSpecification::PromoType::kToast:
      return "Toast";
    case user_education::FeaturePromoSpecification::PromoType::kTutorial:
      return "Tutorial";
  }
}

int64_t GetCreatedTimestampMs(
    const user_education::FeaturePromoSpecification& spec) {
  return 0;
}

std::vector<std::string> GetSupportedPlatforms(
    const user_education::FeaturePromoSpecification& spec) {
  return std::vector<std::string>{"All"};
}

std::vector<std::string> GetPromoInstructions(
    const user_education::FeaturePromoSpecification& spec) {
  std::vector<std::string> instructions;
  if (spec.bubble_title_string_id()) {
    instructions.emplace_back(
        l10n_util::GetStringUTF8(spec.bubble_title_string_id()));
  }
  instructions.emplace_back(
      l10n_util::GetStringUTF8(spec.bubble_body_string_id()));
  return instructions;
}

std::string GetTutorialDescription(
    const user_education::TutorialDescription& desc) {
  return std::string();
}

std::vector<std::string> GetTutorialInstructions(
    const user_education::TutorialDescription& desc) {
  std::vector<std::string> instructions;
  for (const auto& step : desc.steps) {
    if (step.body_text_id()) {
      instructions.emplace_back(l10n_util::GetStringUTF8(step.body_text_id()));
    }
  }
  return instructions;
}

std::string GetTutorialTypeString(
    const user_education::TutorialDescription& desc) {
  return desc.can_be_restarted ? "Restartable Tutorial" : "Tutorial";
}

int64_t GetCreatedTimestampMs(const user_education::TutorialDescription& desc) {
  return 0;
}

std::vector<std::string> GetSupportedPlatforms(
    const user_education::TutorialDescription& desc) {
  return std::vector<std::string>{"All"};
}

}  // namespace

UserEducationInternalsPageHandlerImpl::UserEducationInternalsPageHandlerImpl(
    content::WebUI* web_ui,
    Profile* profile,
    mojo::PendingReceiver<
        mojom::user_education_internals::UserEducationInternalsPageHandler>
        receiver)
    : tutorial_service_(GetTutorialService(profile)),
      web_ui_(web_ui),
      profile_(profile),
      receiver_(this, std::move(receiver)) {}

UserEducationInternalsPageHandlerImpl::
    ~UserEducationInternalsPageHandlerImpl() = default;

void UserEducationInternalsPageHandlerImpl::GetTutorials(
    GetTutorialsCallback callback) {
  std::vector<std::string> ids;
  if (tutorial_service_)
    ids = tutorial_service_->tutorial_registry()->GetTutorialIdentifiers();

  std::vector<mojom::user_education_internals::FeaturePromoDemoPageInfoPtr>
      info_list;
  for (const auto& id : ids) {
    auto* description =
        tutorial_service_->tutorial_registry()->GetTutorialDescription(id);
    if (description) {
      info_list.emplace_back(
          mojom::user_education_internals::FeaturePromoDemoPageInfo::New(
              id, GetTutorialDescription(*description), id,
              GetTutorialTypeString(*description),
              GetCreatedTimestampMs(*description),
              GetSupportedPlatforms(*description),
              GetTutorialInstructions(*description)));
    } else {
      NOTREACHED();
    }
  }
  std::move(callback).Run(std::move(info_list));
}

void UserEducationInternalsPageHandlerImpl::StartTutorial(
    const std::string& tutorial_id,
    StartTutorialCallback callback) {
  CHECK(tutorial_service_);
  const ui::ElementContext context =
      chrome::FindBrowserWithProfile(profile_)->window()->GetElementContext();
  std::string result;
  tutorial_service_->StartTutorial(tutorial_id, context);
  if (!tutorial_service_->IsRunningTutorial()) {
    result = "Failed to start tutorial " + tutorial_id;
  }
  std::move(callback).Run(result);
}

void UserEducationInternalsPageHandlerImpl::GetFeaturePromos(
    GetFeaturePromosCallback callback) {
  std::vector<mojom::user_education_internals::FeaturePromoDemoPageInfoPtr>
      info_list;

  auto* const registry = GetFeaturePromoRegistry(profile_);
  if (registry) {
    const auto& feature_promo_specifications =
        registry->GetRegisteredFeaturePromoSpecifications();
    for (const auto& [feature, spec] : feature_promo_specifications) {
      info_list.emplace_back(
          mojom::user_education_internals::FeaturePromoDemoPageInfo::New(
              GetTitleFromFeaturePromoData(feature, spec),
              spec.demo_page_info().display_description, feature->name,
              GetPromoTypeString(spec), GetCreatedTimestampMs(spec),
              GetSupportedPlatforms(spec), GetPromoInstructions(spec)));
    }
  }

  return std::move(callback).Run(std::move(info_list));
}

void UserEducationInternalsPageHandlerImpl::ShowFeaturePromo(
    const std::string& title,
    ShowFeaturePromoCallback callback) {
  const base::Feature* feature = nullptr;
  auto* const registry = GetFeaturePromoRegistry(profile_);
  if (registry) {
    const auto& feature_promo_specifications =
        registry->GetRegisteredFeaturePromoSpecifications();
    for (const auto& [key, value] : feature_promo_specifications) {
      if (title == GetTitleFromFeaturePromoData(key, value)) {
        feature = key;
        break;
      }
    }
  }

  if (!feature) {
    std::move(callback).Run(std::string("Can not find IPH"));
    return;
  }

  user_education::FeaturePromoController* feature_promo_controller =
      chrome::FindBrowserWithWebContents(web_ui_->GetWebContents())
          ->window()
          ->GetFeaturePromoController();

  bool showed_promo =
      feature_promo_controller->MaybeShowPromoForDemoPage(feature);

  if (showed_promo) {
    std::move(callback).Run(std::string());
  } else {
    std::move(callback).Run(std::string("Failed to show IPH"));
  }
}

const std::string
UserEducationInternalsPageHandlerImpl::GetTitleFromFeaturePromoData(
    const base::Feature* feature,
    const user_education::FeaturePromoSpecification& spec) {
  return (!spec.demo_page_info().display_title.empty()
              ? spec.demo_page_info().display_title
              : feature->name);
}
