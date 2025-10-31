// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/password_manager/core/browser/actor_login/internal/actor_login_form_finder.h"

#include <ranges>

#include "base/strings/utf_string_conversions.h"
#include "components/autofill/core/common/form_data.h"
#include "components/autofill/core/common/form_field_data.h"
#include "components/password_manager/core/browser/features/password_features.h"
#include "components/password_manager/core/browser/password_form.h"
#include "components/password_manager/core/browser/password_form_cache.h"
#include "components/password_manager/core/browser/password_form_manager.h"
#include "components/password_manager/core/browser/password_manager_client.h"
#include "components/password_manager/core/browser/password_manager_driver.h"
#include "components/password_manager/core/browser/password_manager_interface.h"
#include "net/base/registry_controlled_domains/registry_controlled_domain.h"
#include "url/origin.h"

namespace {

bool IsElementFocusable(autofill::FieldRendererId renderer_id,
                        const autofill::FormData& form_data) {
  auto field = std::ranges::find(form_data.fields(), renderer_id,
                                 &autofill::FormFieldData::renderer_id);
  CHECK(field != form_data.fields().end());
  return field->is_focusable();
}

bool IsLoginForm(const password_manager::PasswordForm& form) {
  const bool has_focusable_username =
      form.HasUsernameElement() &&
      IsElementFocusable(form.username_element_renderer_id, form.form_data);
  const bool has_focusable_password =
      form.HasPasswordElement() &&
      IsElementFocusable(form.password_element_renderer_id, form.form_data);
  const bool has_focusable_new_password =
      form.HasNewPasswordElement() &&
      IsElementFocusable(form.new_password_element_renderer_id, form.form_data);

  return (has_focusable_username || has_focusable_password) &&
         !has_focusable_new_password;
}

bool IsFormOriginSupported(const url::Origin& form_origin,
                           const url::Origin& main_frame_origin) {
  if (base::FeatureList::IsEnabled(
          password_manager::features::kActorLoginSameSiteIframeSupport)) {
    return net::registry_controlled_domains::SameDomainOrHost(
        form_origin, main_frame_origin,
        net::registry_controlled_domains::INCLUDE_PRIVATE_REGISTRIES);
  }
  return form_origin.IsSameOriginWith(main_frame_origin);
}

bool IsValidFrameAndOriginToFill(
    base::WeakPtr<password_manager::PasswordManagerDriver> driver,
    const url::Origin& main_frame_origin) {
  if (driver->IsNestedWithinFencedFrame()) {
    // Fenced frames should not be filled.
    return false;
  }

  if (!IsFormOriginSupported(driver->GetLastCommittedOrigin(),
                             main_frame_origin)) {
    return false;
  }

  bool is_same_origin =
      driver->GetLastCommittedOrigin().IsSameOriginWith(main_frame_origin);
  // We can fill a form if its frame context is considered safe and not overly
  // nested. A "fillable context" is either the primary main frame itself, or
  // a direct child of the primary main frame that is not a fenced frame.
  return is_same_origin || driver->IsInPrimaryMainFrame() ||
         driver->IsDirectChildOfPrimaryMainFrame();
}

}  // namespace

ActorLoginFormFinder::ActorLoginFormFinder(
    password_manager::PasswordManagerClient* client)
    : client_(client) {}
ActorLoginFormFinder::~ActorLoginFormFinder() = default;

// static
std::u16string ActorLoginFormFinder::GetSourceSiteOrAppFromUrl(
    const GURL& url) {
  return base::UTF8ToUTF16(url.GetWithEmptyPath().spec());
}

password_manager::PasswordFormManager*
ActorLoginFormFinder::GetSigninFormManager(const url::Origin& origin) {
  password_manager::PasswordFormCache* form_cache =
      client_->GetPasswordManager()->GetPasswordFormCache();
  if (!form_cache) {
    return nullptr;
  }

  std::vector<password_manager::PasswordFormManager*>
      eligible_login_form_managers = GetEligibleLoginFormManagers(origin);
  password_manager::PasswordFormManager* signin_form_manager = nullptr;
  for (const auto& manager : eligible_login_form_managers) {
    // Prefer filling the primary main frame form if one exists, but
    // also prefer more recently-parsed forms.
    if (manager->GetDriver()->IsInPrimaryMainFrame()) {
      signin_form_manager = manager;
    }

    // Otherwise, store this form manager and look to see if there is a primary
    // main frame form later.
    if (!signin_form_manager) {
      signin_form_manager = manager;
    }
  }
  return signin_form_manager;
}

std::vector<password_manager::PasswordFormManager*>
ActorLoginFormFinder::GetEligibleLoginFormManagers(const url::Origin& origin) {
  std::vector<password_manager::PasswordFormManager*> eligible_form_managers;
  password_manager::PasswordFormCache* form_cache =
      client_->GetPasswordManager()->GetPasswordFormCache();
  if (!form_cache) {
    return eligible_form_managers;
  }
  for (const auto& manager : form_cache->GetFormManagers()) {
    if (!manager->GetDriver()) {
      continue;
    }

    if (!IsValidFrameAndOriginToFill(manager->GetDriver(), origin)) {
      continue;
    }

    const password_manager::PasswordForm* parsed_form =
        manager->GetParsedObservedForm();
    if (!parsed_form || !IsLoginForm(*parsed_form)) {
      continue;
    }
    eligible_form_managers.emplace_back(manager.get());
  }
  return eligible_form_managers;
}
