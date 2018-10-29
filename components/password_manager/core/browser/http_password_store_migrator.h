// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_PASSWORD_MANAGER_CORE_BROWSER_HTTP_PASSWORD_STORE_MIGRATOR_H_
#define COMPONENTS_PASSWORD_MANAGER_CORE_BROWSER_HTTP_PASSWORD_STORE_MIGRATOR_H_

#include <memory>
#include <vector>

#include "base/macros.h"
#include "base/sequence_checker.h"
#include "components/password_manager/core/browser/hsts_query.h"
#include "components/password_manager/core/browser/password_store_consumer.h"
#include "url/gurl.h"

namespace autofill {
struct PasswordForm;
}

namespace password_manager {

class PasswordManagerClient;

// The class is responsible for migrating the passwords saved on HTTP to HTTPS
// origin. It automatically determines whether HTTP passwords should be moved or
// copied depending on the site's HSTS status. If a site has HSTS enabled, the
// HTTP password is considered obsolete and will be replaced by an HTTPS
// version. If HSTS is not enabled, some parts of the site might still be served
// via HTTP, which is why the password is copied in this case.
// Furthermore, if a site has migrated to HTTPS and HSTS is enabled, the
// corresponding HTTP site statistics are cleared as well, since they are
// obsolete.
class HttpPasswordStoreMigrator : public PasswordStoreConsumer {
 public:
  // API to be implemented by an embedder of HttpPasswordStoreMigrator.
  class Consumer {
   public:
    virtual ~Consumer() = default;

    // Notify the embedder that |forms| were migrated to HTTPS. |forms| contain
    // the updated HTTPS scheme.
    virtual void ProcessMigratedForms(
        std::vector<std::unique_ptr<autofill::PasswordForm>> forms) = 0;
  };

  // |https_origin| should specify a valid HTTPS URL.
  HttpPasswordStoreMigrator(const GURL& https_origin,
                            const PasswordManagerClient* client,
                            Consumer* consumer);
  ~HttpPasswordStoreMigrator() override;

  // Creates HTTPS version of |http_form|.
  static autofill::PasswordForm MigrateHttpFormToHttps(
      const autofill::PasswordForm& http_form);

  // PasswordStoreConsumer:
  void OnGetPasswordStoreResults(
      std::vector<std::unique_ptr<autofill::PasswordForm>> results) override;

  // Callback for |PasswordManagerClient::PostHSTSQueryForHost|.
  void OnHSTSQueryResult(HSTSResult is_hsts);

 private:
  enum class MigrationMode {
    MOVE,  // HTTP credentials are deleted after migration to HTTPS.
    COPY,  // HTTP credentials are kept after migration to HTTPS.
  };

  void ProcessPasswordStoreResults();

  const PasswordManagerClient* const client_;
  Consumer* consumer_;

  // |ProcessPasswordStoreResults| requires that both |OnHSTSQueryResult| and
  // |OnGetPasswordStoreResults| have returned. Since this can happen in an
  // arbitrary order, boolean flags are introduced to indicate completion. Only
  // if both are set to true |ProcessPasswordStoreResults| gets called.
  bool got_hsts_query_result_ = false;
  bool got_password_store_results_ = false;
  MigrationMode mode_;
  std::vector<std::unique_ptr<autofill::PasswordForm>> results_;
  GURL http_origin_domain_;
  SEQUENCE_CHECKER(sequence_checker_);

  DISALLOW_COPY_AND_ASSIGN(HttpPasswordStoreMigrator);
};

}  // namespace password_manager

#endif  // COMPONENTS_PASSWORD_MANAGER_CORE_BROWSER_HTTP_PASSWORD_STORE_MIGRATOR_H_
