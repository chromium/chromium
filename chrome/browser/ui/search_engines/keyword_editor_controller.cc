// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/search_engines/keyword_editor_controller.h"

#include "base/metrics/user_metrics.h"
#include "chrome/browser/autocomplete/aim_eligibility_service_factory.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/search_engines/template_url_service_factory.h"
#include "chrome/browser/ui/search_engines/template_url_table_model.h"
#include "components/omnibox/browser/omnibox_field_trial.h"
#include "components/optimization_guide/core/feature_registry/feature_registration.h"
#include "components/prefs/pref_registry_simple.h"
#include "components/search_engines/search_engines_switches.h"
#include "components/search_engines/template_url.h"
#include "components/search_engines/template_url_data.h"
#include "components/search_engines/template_url_service.h"
#include "components/search_engines/ui_utils.h"
#include "components/search_engines/util.h"

using base::UserMetricsAction;

namespace {

bool IsPrepopulatedEngine(const TemplateURL* url) {
  return url->prepopulate_id() > 0;
}

bool ShouldUpdateTemplateURL(TemplateURLService* url_model,
                             TemplateURL* template_url,
                             const std::u16string& title,
                             const std::u16string& keyword,
                             const std::string& fixed_up_url) {
  // Will happen if url was deleted while the user was editing it.
  if (!template_url) {
    return false;
  }

  // Compare against an extended version of the Template URL's url, because
  // `fixed_up_url` has gone through this as well.
  const std::string fixed_up_template_url = GetFixedUpSearchEngineUrl(
      template_url->url(), url_model->search_terms_data());

  // Don't do anything if the entry didn't change.
  return template_url->short_name() != title ||
         template_url->keyword() != keyword ||
         fixed_up_template_url != fixed_up_url;
}

}  // namespace

KeywordEditorController::KeywordEditorController(Profile* profile)
    : template_url_service_(TemplateURLServiceFactory::GetForProfile(profile)) {
  template_url_service_->Load();

  if (!base::FeatureList::IsEnabled(switches::kSearchSettingsUpdate)) {
    bool ai_mode_enabled = OmniboxFieldTrial::IsAimStarterPackEnabled(
        AimEligibilityServiceFactory::GetForProfile(profile));
    bool gemini_enabled =
        base::FeatureList::IsEnabled(omnibox::kStarterPackExpansion) &&
        profile->GetPrefs()->GetInteger(
            optimization_guide::prefs::kGeminiSettings) == 0;
    table_model_ = std::make_unique<TemplateURLTableModel>(
        template_url_service_,
        internal::GetDisabledStarterPackIds(ai_mode_enabled, gemini_enabled));
  }
}

KeywordEditorController::~KeywordEditorController() = default;

TemplateURLID KeywordEditorController::AddTemplateURL(
    const std::u16string& title,
    const std::u16string& keyword,
    const std::string& fixed_up_url) {
  CHECK(!fixed_up_url.empty());

  base::RecordAction(UserMetricsAction("KeywordEditor_AddKeyword"));

  TemplateURLData data;
  data.SetShortName(title);
  data.SetKeyword(keyword);
  data.SetURL(fixed_up_url);
  data.is_active = TemplateURLData::ActiveStatus::kTrue;
  TemplateURL* template_url =
      template_url_service_->Add(std::make_unique<TemplateURL>(data));
  return template_url->id();
}

void KeywordEditorController::ModifyTemplateURL(
    TemplateURL* template_url,
    const std::u16string& title,
    const std::u16string& keyword,
    const std::string& fixed_up_url) {
  CHECK(!fixed_up_url.empty());

  if (!ShouldUpdateTemplateURL(template_url_service_, template_url, title,
                               keyword, fixed_up_url)) {
    return;
  }

  // The default search provider should support replacement.
  CHECK(template_url_service_->GetDefaultSearchProvider() != template_url ||
        template_url->SupportsReplacement(
            template_url_service_->search_terms_data()));
  template_url_service_->ResetTemplateURL(template_url, title, keyword,
                                          fixed_up_url);

  base::RecordAction(UserMetricsAction("KeywordEditor_ModifiedKeyword"));
}

bool KeywordEditorController::CanEdit(const TemplateURL* url) const {
  return (url->type() == TemplateURL::NORMAL) &&
         (url != template_url_service_->GetDefaultSearchProvider() ||
          !template_url_service_->is_default_search_managed()) &&
         (!url->CreatedByNonDefaultSearchProviderPolicy() ||
          (url->CanPolicyBeOverridden() && !url->featured_by_policy()));
}

bool KeywordEditorController::CanMakeDefault(const TemplateURL* url) const {
  return template_url_service_->CanMakeDefault(url);
}

bool KeywordEditorController::CanRemove(const TemplateURL* url) const {
  return (url->type() == TemplateURL::NORMAL) &&
         (url != template_url_service_->GetDefaultSearchProvider()) &&
         (url->starter_pack_id() ==
          template_url_starter_pack_data::StarterPackId::kNone) &&
         (!url->CreatedByNonDefaultSearchProviderPolicy() ||
          (url->CanPolicyBeOverridden() && !url->featured_by_policy()));
}

bool KeywordEditorController::CanActivate(const TemplateURL* url) const {
  return (url->is_active() != TemplateURLData::ActiveStatus::kTrue) &&
         !IsPrepopulatedEngine(url);
}

bool KeywordEditorController::CanDeactivate(const TemplateURL* url) const {
  return url->is_active() == TemplateURLData::ActiveStatus::kTrue &&
         url != template_url_service_->GetDefaultSearchProvider() &&
         !IsPrepopulatedEngine(url) &&
         (!url->CreatedByNonDefaultSearchProviderPolicy() ||
          url->CanPolicyBeOverridden());
}

bool KeywordEditorController::ShouldConfirmRemoval(
    const TemplateURL* url) const {
  return url->RequiresRemovalConfirmation();
}

bool KeywordEditorController::IsManaged(const TemplateURL* url) const {
  return (url->CreatedByDefaultSearchProviderPolicy() &&
          url->enforced_by_policy()) ||
         url->CreatedByNonDefaultSearchProviderPolicy();
}

void KeywordEditorController::RemoveTemplateURL(TemplateURLID id) {
  TemplateURL* template_url = GetTemplateURL(id);
  if (!template_url) {
    return;
  }

  template_url_service_->Remove(template_url);
  base::RecordAction(UserMetricsAction("KeywordEditor_RemoveKeyword"));
}

const TemplateURL* KeywordEditorController::GetDefaultSearchProvider() {
  return template_url_service_->GetDefaultSearchProvider();
}

void KeywordEditorController::MakeDefaultTemplateURL(
    TemplateURLID id,
    search_engines::ChoiceMadeLocation choice_location) {
  TemplateURL* template_url = GetTemplateURL(id);
  if (!template_url ||
      template_url == template_url_service_->GetDefaultSearchProvider()) {
    return;
  }

  template_url_service_->SetUserSelectedDefaultSearchProvider(template_url,
                                                              choice_location);
}

void KeywordEditorController::SetIsActiveTemplateURL(TemplateURLID id,
                                                     bool is_active) {
  TemplateURL* template_url = GetTemplateURL(id);
  if (!template_url) {
    return;
  }

  template_url_service_->SetIsActiveTemplateURL(template_url, is_active);
}

bool KeywordEditorController::loaded() const {
  return template_url_service_->loaded();
}

TemplateURL* KeywordEditorController::GetTemplateURL(TemplateURLID id) {
  return template_url_service_->GetTemplateURLForId(id);
}

TemplateURL* KeywordEditorController::GetTemplateURLForIndex(int index) {
  CHECK(!base::FeatureList::IsEnabled(switches::kSearchSettingsUpdate));
  return table_model_->GetTemplateURL(index);
}

void KeywordEditorController::Refresh() {
  if (!base::FeatureList::IsEnabled(switches::kSearchSettingsUpdate)) {
    table_model_->Reload();
  }
}
