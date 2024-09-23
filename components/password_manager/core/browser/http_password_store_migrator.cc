// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/password_manager/core/browser/http_password_store_migrator.h"

#include <string>
#include <utility>
#include <vector>

#include "base/functional/bind.h"
#include "base/memory/weak_ptr.h"
#include "base/metrics/histogram_functions.h"
#include "base/strings/strcat.h"
#include "components/password_manager/core/browser/password_manager_metrics_util.h"
#include "components/password_manager/core/browser/password_manager_util.h"
#include "components/password_manager/core/browser/password_store/password_store_interface.h"
#include "components/password_manager/core/browser/password_store/smart_bubble_stats_store.h"
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
    const url::Origin& https_origin,
    PasswordStoreInterface* store,
    network::mojom::NetworkContext* network_context,
    Consumer* consumer)
    : store_(store), consumer_(consumer) {
  DCHECK(store_);
  DCHECK(!https_origin.opaque());
  DCHECK_EQ(https_origin.scheme(), url::kHttpsScheme) << https_origin;

  GURL::Replacements rep;
  rep.SetSchemeStr(url::kHttpScheme);
  GURL http_origin = https_origin.GetURL().ReplaceComponents(rep);
  PasswordFormDigest form(PasswordForm::Scheme::kHtml,
                          http_origin.DeprecatedGetOriginAsURL().spec(),
                          http_origin);
  http_origin_domain_ = url::Origin::Create(http_origin);
  store_->GetLogins(form, weak_ptr_factory_.GetWeakPtr());

  PostHSTSQueryForHostAndNetworkContext(
      https_origin, network_context,
      base::BindOnce(&OnHSTSQueryResultHelper, weak_ptr_factory_.GetWeakPtr()));
}

HttpPasswordStoreMigrator::~HttpPasswordStoreMigrator() = default;

PasswordForm HttpPasswordStoreMigrator::MigrateHttpFormToHttps(
    const PasswordForm& http_form) {
  DCHECK(http_form.url.SchemeIs(url::kHttpScheme));

  PasswordForm https_form = http_form;
  GURL::Replacements rep;
  rep.SetSchemeStr(url::kHttpsScheme);
  https_form.url = http_form.url.ReplaceComponents(rep);

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
  if (!http_form.action.SchemeIs(url::kHttpsScheme)) {
    https_form.action = https_form.url;
  }
  https_form.form_data = autofill::FormData();
  https_form.generation_upload_status =
      PasswordForm::GenerationUploadStatus::kNoSignalSent;
  https_form.skip_zero_click = false;
  return https_form;
}

void HttpPasswordStoreMigrator::OnGetPasswordStoreResults(
    std::vector<std::unique_ptr<PasswordForm>> results) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  results_ = std::move(results);
  got_password_store_results_ = true;

  if (got_hsts_query_result_) {
    ProcessPasswordStoreResults();
  }
}

void HttpPasswordStoreMigrator::OnHSTSQueryResult(HSTSResult is_hsts) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  mode_ = (is_hsts == HSTSResult::kYes) ? HttpPasswordMigrationMode::kMove
                                        : HttpPasswordMigrationMode::kCopy;
  got_hsts_query_result_ = true;

  if (is_hsts == HSTSResult::kYes) {
    SmartBubbleStatsStore* stats_store = store_->GetSmartBubbleStatsStore();
    if (stats_store) {
      stats_store->RemoveSiteStats(http_origin_domain_.GetURL());
    }
  }

  if (got_password_store_results_) {
    ProcessPasswordStoreResults();
  }
}

void HttpPasswordStoreMigrator::ProcessPasswordStoreResults() {
  // Ignore PSL, affiliated, grouped and other matches.
  std::erase_if(results_, [](const std::unique_ptr<PasswordForm>& form) {
    return password_manager_util::GetMatchType(*form) !=
           password_manager_util::GetLoginMatchType::kExact;
  });

  // Add the new credentials to the password store. The HTTP forms are
  // removed iff |mode_| == MigrationMode::MOVE.
  for (const auto& form : results_) {
    PasswordForm new_form =
        HttpPasswordStoreMigrator::MigrateHttpFormToHttps(*form);
    store_->AddLogin(new_form);

    if (mode_ == HttpPasswordMigrationMode::kMove) {
      store_->RemoveLogin(FROM_HERE, *form);
    }
    *form = std::move(new_form);
  }

  // Only log data if there was at least one migrated password.
  if (!results_.empty()) {
    base::UmaHistogramCounts100("PasswordManager.HttpPasswordMigrationCount2",
                                results_.size());
    base::UmaHistogramEnumeration("PasswordManager.HttpPasswordMigrationMode2",
                                  mode_);
  }

  if (consumer_) {
    consumer_->ProcessMigratedForms(std::move(results_));
  }
}

}  // namespace password_manager
