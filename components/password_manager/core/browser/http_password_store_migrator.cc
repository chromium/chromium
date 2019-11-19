// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/password_manager/core/browser/http_password_store_migrator.h"

#include <string>
#include <utility>

#include "base/bind.h"
#include "base/memory/weak_ptr.h"
#include "base/stl_util.h"
#include "base/strings/strcat.h"
#include "components/password_manager/core/browser/password_manager_client.h"
#include "components/password_manager/core/browser/password_manager_metrics_util.h"
#include "components/password_manager/core/browser/password_manager_util.h"
#include "components/password_manager/core/browser/password_store.h"
#include "url/gurl.h"
#include "url/url_constants.h"

namespace password_manager {

namespace {

// Helper method that allows us to pass WeakPtrs to |PasswordStoreConsumer|
// obtained via |GetWeakPtr|. This is not possible otherwise.
void OnHSTSQueryResultHelper(
    const base::WeakPtr<PasswordStoreConsumer>& migrator,
    HSTSResult is_hsts) {
  if (migrator) {
    static_cast<HttpPasswordStoreMigrator*>(migrator.get())
        ->OnHSTSQueryResult(is_hsts);
  }
}

}  // namespace

HttpPasswordStoreMigrator::HttpPasswordStoreMigrator(
    const GURL& https_origin,
    const PasswordManagerClient* client,
    Consumer* consumer)
    : client_(client), consumer_(consumer) {
  DCHECK(client_);
  DCHECK(https_origin.is_valid());
  DCHECK(https_origin.SchemeIs(url::kHttpsScheme)) << https_origin;

  GURL::Replacements rep;
  rep.SetSchemeStr(url::kHttpScheme);
  GURL http_origin = https_origin.ReplaceComponents(rep);
  PasswordStore::FormDigest form(autofill::PasswordForm::Scheme::kHtml,
                                 http_origin.GetOrigin().spec(), http_origin);
  http_origin_domain_ = http_origin.GetOrigin();
  client_->GetProfilePasswordStore()->GetLogins(form, this);
  client_->PostHSTSQueryForHost(
      https_origin, base::Bind(&OnHSTSQueryResultHelper, GetWeakPtr()));
}

HttpPasswordStoreMigrator::~HttpPasswordStoreMigrator() = default;

autofill::PasswordForm HttpPasswordStoreMigrator::MigrateHttpFormToHttps(
    const autofill::PasswordForm& http_form) {
  DCHECK(http_form.origin.SchemeIs(url::kHttpScheme));

  autofill::PasswordForm https_form = http_form;
  GURL::Replacements rep;
  rep.SetSchemeStr(url::kHttpsScheme);
  https_form.origin = http_form.origin.ReplaceComponents(rep);

  // Only replace the scheme of the signon_realm in case it is HTTP. Do not
  // change the signon_realm for federated credentials.
  if (GURL(http_form.signon_realm).SchemeIs(url::kHttpScheme)) {
    https_form.signon_realm =
        base::StrCat({url::kHttpsScheme, url::kStandardSchemeSeparator,
                      password_manager_util::GetSignonRealmWithProtocolExcluded(
                          https_form)});
  }
  // If |action| is not HTTPS then it's most likely obsolete. Otherwise, it
  // may still be valid.
  if (!http_form.action.SchemeIs(url::kHttpsScheme))
    https_form.action = https_form.origin;
  https_form.form_data = autofill::FormData();
  https_form.generation_upload_status =
      autofill::PasswordForm::GenerationUploadStatus::kNoSignalSent;
  https_form.skip_zero_click = false;
  return https_form;
}

void HttpPasswordStoreMigrator::OnGetPasswordStoreResults(
    std::vector<std::unique_ptr<autofill::PasswordForm>> results) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  results_ = std::move(results);
  got_password_store_results_ = true;

  if (got_hsts_query_result_)
    ProcessPasswordStoreResults();
}

void HttpPasswordStoreMigrator::OnHSTSQueryResult(HSTSResult is_hsts) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  mode_ =
      (is_hsts == HSTSResult::kYes) ? MigrationMode::MOVE : MigrationMode::COPY;
  got_hsts_query_result_ = true;

  if (is_hsts == HSTSResult::kYes)
    client_->GetProfilePasswordStore()->RemoveSiteStats(http_origin_domain_);

  if (got_password_store_results_)
    ProcessPasswordStoreResults();
}

void HttpPasswordStoreMigrator::ProcessPasswordStoreResults() {
  // Android and PSL matches are ignored.
  base::EraseIf(
      results_, [](const std::unique_ptr<autofill::PasswordForm>& form) {
        return form->is_affiliation_based_match || form->is_public_suffix_match;
      });

  // Add the new credentials to the password store. The HTTP forms are
  // removed iff |mode_| == MigrationMode::MOVE.
  for (const auto& form : results_) {
    autofill::PasswordForm new_form =
        HttpPasswordStoreMigrator::MigrateHttpFormToHttps(*form);
    client_->GetProfilePasswordStore()->AddLogin(new_form);

    if (mode_ == MigrationMode::MOVE)
      client_->GetProfilePasswordStore()->RemoveLogin(*form);
    *form = std::move(new_form);
  }

  if (!results_.empty()) {
    // Only log data if there was at least one migrated password.
    metrics_util::LogCountHttpMigratedPasswords(results_.size());
    metrics_util::LogHttpPasswordMigrationMode(
        mode_ == MigrationMode::MOVE
            ? metrics_util::HTTP_PASSWORD_MIGRATION_MODE_MOVE
            : metrics_util::HTTP_PASSWORD_MIGRATION_MODE_COPY);
  }

  if (consumer_)
    consumer_->ProcessMigratedForms(std::move(results_));
}

}  // namespace password_manager
