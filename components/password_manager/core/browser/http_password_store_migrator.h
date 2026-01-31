// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_PASSWORD_MANAGER_CORE_BROWSER_HTTP_PASSWORD_STORE_MIGRATOR_H_
#define COMPONENTS_PASSWORD_MANAGER_CORE_BROWSER_HTTP_PASSWORD_STORE_MIGRATOR_H_

#include <memory>
#include <vector>

#include "base/memory/raw_ptr.h"
#include "base/memory/weak_ptr.h"
#include "base/sequence_checker.h"
#include "components/password_manager/core/browser/hsts_query.h"
#include "components/password_manager/core/browser/password_store/password_store_consumer.h"
#include "url/origin.h"

namespace password_manager {

class PasswordStoreInterface;
struct PasswordForm;

// These values are persisted to logs. Entries should not be renumbered and
// numeric values should never be reused.
//
// Needs to stay in sync with HttpPasswordMigrationMode in enums.xml.
enum class HttpPasswordMigrationMode {
  // HTTP credentials are deleted after migration to HTTPS.
  kMove = 0,
  // HTTP credentials are kept after migration to HTTPS.
  kCopy = 1,
  kMaxValue = kCopy,
};

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
        std::vector<std::unique_ptr<PasswordForm>> forms) = 0;
  };

  // |https_origin| should specify a valid HTTPS URL.
  HttpPasswordStoreMigrator(const url::Origin& https_origin,
                            PasswordStoreInterface* store,
                            network::mojom::NetworkContext* network_context,
                            Consumer* consumer);

  HttpPasswordStoreMigrator(const HttpPasswordStoreMigrator&) = delete;
  HttpPasswordStoreMigrator& operator=(const HttpPasswordStoreMigrator&) =
      delete;

  ~HttpPasswordStoreMigrator() override;

  // Creates HTTPS version of |http_form|.
  static PasswordForm MigrateHttpFormToHttps(const PasswordForm& http_form);

  // PasswordStoreConsumer:
  void OnGetPasswordStoreResults(
      std::vector<std::unique_ptr<PasswordForm>> results) override;

  // Callback for PostHSTSQueryForHostAndNetworkContext.
  void OnHSTSQueryResult(HSTSResult is_hsts);

 private:
  void ProcessPasswordStoreResults();

  const raw_ptr<PasswordStoreInterface> store_;
  raw_ptr<Consumer> consumer_;

  // |ProcessPasswordStoreResults| requires that both |OnHSTSQueryResult| and
  // |OnGetPasswordStoreResults| have returned. Since this can happen in an
  // arbitrary order, boolean flags are introduced to indicate completion. Only
  // if both are set to true |ProcessPasswordStoreResults| gets called.
  bool got_hsts_query_result_ = false;
  bool got_password_store_results_ = false;
  HttpPasswordMigrationMode mode_ = HttpPasswordMigrationMode::kMove;
  std::vector<std::unique_ptr<PasswordForm>> results_;
  url::Origin http_origin_domain_;
  SEQUENCE_CHECKER(sequence_checker_);
  base::WeakPtrFactory<HttpPasswordStoreMigrator> weak_ptr_factory_{this};
};

}  // namespace password_manager

#endif  // COMPONENTS_PASSWORD_MANAGER_CORE_BROWSER_HTTP_PASSWORD_STORE_MIGRATOR_H_
