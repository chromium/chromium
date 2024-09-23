// Copyright 2013 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifdef UNSAFE_BUFFERS_BUILD
// TODO(crbug.com/40285824): Remove this and convert code to safer constructs.
#pragma allow_unsafe_buffers
#endif

#include "chrome/browser/ui/webui/identity_internals_ui.h"

#include <memory>
#include <set>
#include <string>

#include "base/functional/bind.h"
#include "base/i18n/time_formatting.h"
#include "base/memory/raw_ptr.h"
#include "base/strings/utf_string_conversions.h"
#include "base/values.h"
#include "chrome/browser/extensions/api/identity/identity_api.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/common/url_constants.h"
#include "chrome/grit/generated_resources.h"
#include "chrome/grit/identity_internals_resources.h"
#include "chrome/grit/identity_internals_resources_map.h"
#include "content/public/browser/web_ui.h"
#include "content/public/browser/web_ui_controller.h"
#include "content/public/browser/web_ui_data_source.h"
#include "content/public/browser/web_ui_message_handler.h"
#include "extensions/browser/extension_registry.h"
#include "extensions/common/extension_id.h"
#include "google_apis/gaia/gaia_auth_fetcher.h"
#include "services/network/public/cpp/shared_url_loader_factory.h"

IdentityInternalsUIConfig::IdentityInternalsUIConfig()
    : DefaultWebUIConfig(content::kChromeUIScheme,
                         chrome::kChromeUIIdentityInternalsHost) {}

namespace {

// RevokeToken message parameter offsets.
const int kRevokeTokenExtensionOffset = 1;
const int kRevokeTokenTokenOffset = 2;

class IdentityInternalsTokenRevoker;

// Class acting as a controller of the chrome://identity-internals WebUI.
class IdentityInternalsUIMessageHandler : public content::WebUIMessageHandler {
 public:
  IdentityInternalsUIMessageHandler();
  ~IdentityInternalsUIMessageHandler() override;

  // Ensures that a proper clean up happens after a token is revoked. That
  // includes deleting the |token_revoker|, removing the token from Identity API
  // cache and updating the UI that the token is gone.
  void OnTokenRevokerDone(IdentityInternalsTokenRevoker* token_revoker,
                          const std::string& callback_id);

  // WebUIMessageHandler implementation.
  void RegisterMessages() override;

 private:
  // Gets the name of an extension referred to by |access_tokens_key| as a
  // string.
  const std::string GetExtensionName(
      const extensions::IdentityTokenCache::AccessTokensKey& access_tokens_key);

  // Gets a list of scopes specified in |token_cache_value| and returns a
  // base::Value::List containing the scopes.
  base::Value::List GetScopes(
      const extensions::IdentityTokenCacheValue& token_cache_value);

  // Gets a status of the access token in |token_cache_value|.
  std::string GetStatus(
      const extensions::IdentityTokenCacheValue& token_cache_value);

  // Gets a string representation of an expiration time of the access token in
  // |token_cache_value|.
  std::u16string GetExpirationTime(
      const extensions::IdentityTokenCacheValue& token_cache_value);

  // Converts a pair of |access_tokens_key| and |token_cache_value| to a
  // base::Value::Dict object with corresponding information.
  base::Value::Dict GetInfoForToken(
      const extensions::IdentityTokenCache::AccessTokensKey& access_tokens_key,
      const extensions::IdentityTokenCacheValue& token_cache_value);

  // Gets all of the tokens stored in IdentityAPI token cache and returns them
  // to the caller using Javascript callback function
  // |identity_internals.returnTokens()|.
  void GetInfoForAllTokens(const base::Value::List& args);

  // Initiates revoking of the token, based on the extension ID and token
  // passed as entries in the |args| list. Updates the caller of completion
  // using Javascript callback function |identity_internals.tokenRevokeDone()|.
  void RevokeToken(const base::Value::List& args);

  // A vector of token revokers that are currently revoking tokens.
  std::vector<std::unique_ptr<IdentityInternalsTokenRevoker>> token_revokers_;
};

// Handles the revoking of an access token and helps performing the clean up
// after it is revoked by holding information about the access token and related
// extension ID.
class IdentityInternalsTokenRevoker : public GaiaAuthConsumer {
 public:
  // Revokes |access_token| from extension with |extension_id|.
  // |profile| is required for its request context. |consumer| will be
  // notified when revocation succeeds via |OnTokenRevokerDone()|.
  IdentityInternalsTokenRevoker(const std::string& extension_id,
                                const std::string& access_token,
                                const std::string& callback_id,
                                Profile* profile,
                                IdentityInternalsUIMessageHandler* consumer);

  IdentityInternalsTokenRevoker(const IdentityInternalsTokenRevoker&) = delete;
  IdentityInternalsTokenRevoker& operator=(
      const IdentityInternalsTokenRevoker&) = delete;

  ~IdentityInternalsTokenRevoker() override;

  // Returns the access token being revoked.
  const std::string& access_token() const { return access_token_; }

  // Returns the ID of the extension the access token is related to.
  const extensions::ExtensionId& extension_id() const { return extension_id_; }

  // GaiaAuthConsumer implementation.
  void OnOAuth2RevokeTokenCompleted(
      GaiaAuthConsumer::TokenRevocationStatus status) override;

 private:
  // An object used to start a token revoke request.
  GaiaAuthFetcher fetcher_;
  // An ID of an extension the access token is related to.
  const extensions::ExtensionId extension_id_;
  // The access token to revoke.
  const std::string access_token_;
  // The JS callback to resolve when revoking is done.
  const std::string callback_id_;
  // An object that needs to be notified once the access token is revoked.
  raw_ptr<IdentityInternalsUIMessageHandler> consumer_;  // weak.
};

IdentityInternalsUIMessageHandler::IdentityInternalsUIMessageHandler() {}

IdentityInternalsUIMessageHandler::~IdentityInternalsUIMessageHandler() {}

void IdentityInternalsUIMessageHandler::OnTokenRevokerDone(
    IdentityInternalsTokenRevoker* token_revoker,
    const std::string& callback_id) {
  extensions::IdentityAPI* api =
      extensions::IdentityAPI::GetFactoryInstance()->Get(
          Profile::FromWebUI(web_ui()));
  // API can be null in incognito, but then we shouldn't be in this function.
  // This case is handled because this is called from a renderer process
  // which could conceivably be compromised.
  CHECK(api);

  // Remove token from the cache.
  api->token_cache()->EraseAccessToken(token_revoker->extension_id(),
                                       token_revoker->access_token());

  // Update view about the token being removed.
  ResolveJavascriptCallback(base::Value(callback_id),
                            base::Value(token_revoker->access_token()));

  // Erase the revoker.
  for (auto iter = token_revokers_.begin(); iter != token_revokers_.end();
       ++iter) {
    if (iter->get() == token_revoker) {
      token_revokers_.erase(iter);
      return;
    }
  }
  DCHECK(false) << "revoker should have been in the list";
}

const std::string IdentityInternalsUIMessageHandler::GetExtensionName(
    const extensions::IdentityTokenCache::AccessTokensKey& access_tokens_key) {
  const extensions::ExtensionRegistry* registry =
      extensions::ExtensionRegistry::Get(Profile::FromWebUI(web_ui()));
  const extensions::Extension* extension =
      registry->enabled_extensions().GetByID(access_tokens_key.extension_id);
  if (!extension)
    return std::string();
  return extension->name();
}

base::Value::List IdentityInternalsUIMessageHandler::GetScopes(
    const extensions::IdentityTokenCacheValue& token_cache_value) {
  base::Value::List scopes_value;
  for (const auto& scope : token_cache_value.granted_scopes()) {
    scopes_value.Append(scope);
  }
  return scopes_value;
}

std::string IdentityInternalsUIMessageHandler::GetStatus(
    const extensions::IdentityTokenCacheValue& token_cache_value) {
  switch (token_cache_value.status()) {
    case extensions::IdentityTokenCacheValue::CACHE_STATUS_REMOTE_CONSENT:
    case extensions::IdentityTokenCacheValue::
        CACHE_STATUS_REMOTE_CONSENT_APPROVED:
      // Fallthrough to NOT FOUND case, as ADVICE, REMOTE_CONSENT and
      // REMOTE_CONSENT_APPROVED are short lived.
    case extensions::IdentityTokenCacheValue::CACHE_STATUS_NOTFOUND:
      return "Not Found";
    case extensions::IdentityTokenCacheValue::CACHE_STATUS_TOKEN:
      return "Token Present";
  }
  NOTREACHED_IN_MIGRATION();
  return std::string();
}

std::u16string IdentityInternalsUIMessageHandler::GetExpirationTime(
    const extensions::IdentityTokenCacheValue& token_cache_value) {
  return base::TimeFormatFriendlyDateAndTime(
      token_cache_value.expiration_time());
}

base::Value::Dict IdentityInternalsUIMessageHandler::GetInfoForToken(
    const extensions::IdentityTokenCache::AccessTokensKey& access_tokens_key,
    const extensions::IdentityTokenCacheValue& token_cache_value) {
  base::Value::Dict token_data;
  token_data.Set("extensionId", access_tokens_key.extension_id);
  token_data.Set("accountId", access_tokens_key.account_id.ToString());
  token_data.Set("extensionName", GetExtensionName(access_tokens_key));
  token_data.Set("scopes", GetScopes(token_cache_value));
  token_data.Set("status", GetStatus(token_cache_value));
  token_data.Set("accessToken", token_cache_value.token());
  token_data.Set("expirationTime", GetExpirationTime(token_cache_value));
  return token_data;
}

void IdentityInternalsUIMessageHandler::GetInfoForAllTokens(
    const base::Value::List& args) {
  const std::string& callback_id = args[0].GetString();
  CHECK(!callback_id.empty());

  AllowJavascript();
  base::Value::List results;
  extensions::IdentityTokenCache::AccessTokensCache tokens;
  // The API can be null in incognito.
  extensions::IdentityAPI* api =
      extensions::IdentityAPI::GetFactoryInstance()->Get(
          Profile::FromWebUI(web_ui()));
  if (api)
    tokens = api->token_cache()->access_tokens_cache();
  for (const auto& key_tokens : tokens) {
    for (const auto& token : key_tokens.second) {
      results.Append(GetInfoForToken(key_tokens.first, token));
    }
  }
  ResolveJavascriptCallback(base::Value(callback_id), results);
}

void IdentityInternalsUIMessageHandler::RegisterMessages() {
  web_ui()->RegisterMessageCallback(
      "identityInternalsGetTokens",
      base::BindRepeating(
          &IdentityInternalsUIMessageHandler::GetInfoForAllTokens,
          base::Unretained(this)));
  web_ui()->RegisterMessageCallback(
      "identityInternalsRevokeToken",
      base::BindRepeating(&IdentityInternalsUIMessageHandler::RevokeToken,
                          base::Unretained(this)));
}

void IdentityInternalsUIMessageHandler::RevokeToken(
    const base::Value::List& list) {
  const std::string& callback_id = list[0].GetString();
  CHECK(!callback_id.empty());
  std::string extension_id;
  std::string access_token;
  if (!list.empty() && list[kRevokeTokenExtensionOffset].is_string()) {
    extension_id = list[kRevokeTokenExtensionOffset].GetString();
  }
  if (list.size() > kRevokeTokenTokenOffset &&
      list[kRevokeTokenTokenOffset].is_string()) {
    access_token = list[kRevokeTokenTokenOffset].GetString();
  }

  token_revokers_.push_back(std::make_unique<IdentityInternalsTokenRevoker>(
      extension_id, access_token, callback_id, Profile::FromWebUI(web_ui()),
      this));
}

IdentityInternalsTokenRevoker::IdentityInternalsTokenRevoker(
    const std::string& extension_id,
    const std::string& access_token,
    const std::string& callback_id,
    Profile* profile,
    IdentityInternalsUIMessageHandler* consumer)
    : fetcher_(this, gaia::GaiaSource::kChrome, profile->GetURLLoaderFactory()),
      extension_id_(extension_id),
      access_token_(access_token),
      callback_id_(callback_id),
      consumer_(consumer) {
  DCHECK(consumer_);
  fetcher_.StartRevokeOAuth2Token(access_token);
}

IdentityInternalsTokenRevoker::~IdentityInternalsTokenRevoker() {}

void IdentityInternalsTokenRevoker::OnOAuth2RevokeTokenCompleted(
    GaiaAuthConsumer::TokenRevocationStatus status) {
  consumer_->OnTokenRevokerDone(this, callback_id_);
}

}  // namespace

IdentityInternalsUI::IdentityInternalsUI(content::WebUI* web_ui)
  : content::WebUIController(web_ui) {
  // chrome://identity-internals source.
  content::WebUIDataSource* html_source =
      content::WebUIDataSource::CreateAndAdd(
          Profile::FromWebUI(web_ui), chrome::kChromeUIIdentityInternalsHost);

  // Required resources
  html_source->AddResourcePaths(base::make_span(
      kIdentityInternalsResources, kIdentityInternalsResourcesSize));
  html_source->SetDefaultResource(
      IDR_IDENTITY_INTERNALS_IDENTITY_INTERNALS_HTML);

  html_source->OverrideContentSecurityPolicy(
      network::mojom::CSPDirectiveName::TrustedTypes,
      "trusted-types static-types;");
  html_source->OverrideContentSecurityPolicy(
      network::mojom::CSPDirectiveName::ScriptSrc,
      "script-src chrome://resources chrome://webui-test 'self';");

  web_ui->AddMessageHandler(
      std::make_unique<IdentityInternalsUIMessageHandler>());
}

IdentityInternalsUI::~IdentityInternalsUI() {}
