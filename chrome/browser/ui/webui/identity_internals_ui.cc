// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/webui/identity_internals_ui.h"

#include <memory>
#include <set>
#include <string>

#include "base/bind.h"
#include "base/i18n/time_formatting.h"
#include "base/macros.h"
#include "base/strings/utf_string_conversions.h"
#include "base/values.h"
#include "chrome/browser/extensions/api/identity/identity_api.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/common/url_constants.h"
#include "chrome/grit/browser_resources.h"
#include "chrome/grit/generated_resources.h"
#include "content/public/browser/web_ui.h"
#include "content/public/browser/web_ui_controller.h"
#include "content/public/browser/web_ui_data_source.h"
#include "content/public/browser/web_ui_message_handler.h"
#include "extensions/browser/extension_registry.h"
#include "google_apis/gaia/gaia_auth_fetcher.h"
#include "google_apis/gaia/gaia_constants.h"
#include "ui/base/l10n/l10n_util.h"

namespace {

// Properties of the Javascript object representing a token.
const char kExtensionId[] = "extensionId";
const char kExtensionName[] = "extensionName";
const char kScopes[] = "scopes";
const char kStatus[] = "status";
const char kTokenExpirationTime[] = "expirationTime";
const char kAccessToken[] = "accessToken";

// RevokeToken message parameter offsets.
const int kRevokeTokenExtensionOffset = 0;
const int kRevokeTokenTokenOffset = 1;

class IdentityInternalsTokenRevoker;

// Class acting as a controller of the chrome://identity-internals WebUI.
class IdentityInternalsUIMessageHandler : public content::WebUIMessageHandler {
 public:
  IdentityInternalsUIMessageHandler();
  ~IdentityInternalsUIMessageHandler() override;

  // Ensures that a proper clean up happens after a token is revoked. That
  // includes deleting the |token_revoker|, removing the token from Identity API
  // cache and updating the UI that the token is gone.
  void OnTokenRevokerDone(IdentityInternalsTokenRevoker* token_revoker);

  // WebUIMessageHandler implementation.
  void RegisterMessages() override;

 private:
  // Gets the name of an extension referred to by |token_cache_key| as a string.
  const std::string GetExtensionName(
      const extensions::ExtensionTokenKey& token_cache_key);

  // Gets a list of scopes specified in |token_cache_key| and returns a pointer
  // to a ListValue containing the scopes. The caller gets ownership of the
  // returned object.
  std::unique_ptr<base::ListValue> GetScopes(
      const extensions::ExtensionTokenKey& token_cache_key);

  // Gets a localized status of the access token in |token_cache_value|.
  const base::string16 GetStatus(
      const extensions::IdentityTokenCacheValue& token_cache_value);

  // Gets a string representation of an expiration time of the access token in
  // |token_cache_value|.
  const std::string GetExpirationTime(
      const extensions::IdentityTokenCacheValue& token_cache_value);

  // Converts a pair of |token_cache_key| and |token_cache_value| to a
  // DictionaryValue object with corresponding information in a localized and
  // readable form and returns a pointer to created object.
  std::unique_ptr<base::DictionaryValue> GetInfoForToken(
      const extensions::ExtensionTokenKey& token_cache_key,
      const extensions::IdentityTokenCacheValue& token_cache_value);

  // Gets all of the tokens stored in IdentityAPI token cache and returns them
  // to the caller using Javascript callback function
  // |identity_internals.returnTokens()|.
  void GetInfoForAllTokens(const base::ListValue* args);

  // Initiates revoking of the token, based on the extension ID and token
  // passed as entries in the |args| list. Updates the caller of completion
  // using Javascript callback function |identity_internals.tokenRevokeDone()|.
  void RevokeToken(const base::ListValue* args);

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
                                Profile* profile,
                                IdentityInternalsUIMessageHandler* consumer);
  ~IdentityInternalsTokenRevoker() override;

  // Returns the access token being revoked.
  const std::string& access_token() const { return access_token_; }

  // Returns the ID of the extension the access token is related to.
  const std::string& extension_id() const { return extension_id_; }

  // GaiaAuthConsumer implementation.
  void OnOAuth2RevokeTokenCompleted(
      GaiaAuthConsumer::TokenRevocationStatus status) override;

 private:
  // An object used to start a token revoke request.
  GaiaAuthFetcher fetcher_;
  // An ID of an extension the access token is related to.
  const std::string extension_id_;
  // The access token to revoke.
  const std::string access_token_;
  // An object that needs to be notified once the access token is revoked.
  IdentityInternalsUIMessageHandler* consumer_;  // weak.

  DISALLOW_COPY_AND_ASSIGN(IdentityInternalsTokenRevoker);
};

IdentityInternalsUIMessageHandler::IdentityInternalsUIMessageHandler() {}

IdentityInternalsUIMessageHandler::~IdentityInternalsUIMessageHandler() {}

void IdentityInternalsUIMessageHandler::OnTokenRevokerDone(
    IdentityInternalsTokenRevoker* token_revoker) {
  extensions::IdentityAPI* api =
      extensions::IdentityAPI::GetFactoryInstance()->Get(
          Profile::FromWebUI(web_ui()));
  // API can be null in incognito, but then we shouldn't be in this function.
  // This case is handled because this is called from a renderer process
  // which could conceivably be compromised.
  CHECK(api);

  // Remove token from the cache.
  api->EraseCachedToken(token_revoker->extension_id(),
                        token_revoker->access_token());

  // Update view about the token being removed.
  base::ListValue result;
  result.AppendString(token_revoker->access_token());
  web_ui()->CallJavascriptFunctionUnsafe("identity_internals.tokenRevokeDone",
                                         result);

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
    const extensions::ExtensionTokenKey& token_cache_key) {
  const extensions::ExtensionRegistry* registry =
      extensions::ExtensionRegistry::Get(Profile::FromWebUI(web_ui()));
  const extensions::Extension* extension =
      registry->enabled_extensions().GetByID(token_cache_key.extension_id);
  if (!extension)
    return std::string();
  return extension->name();
}

std::unique_ptr<base::ListValue> IdentityInternalsUIMessageHandler::GetScopes(
    const extensions::ExtensionTokenKey& token_cache_key) {
  auto scopes_value = std::make_unique<base::ListValue>();
  for (auto iter = token_cache_key.scopes.begin();
       iter != token_cache_key.scopes.end(); ++iter) {
    scopes_value->AppendString(*iter);
  }
  return scopes_value;
}

const base::string16 IdentityInternalsUIMessageHandler::GetStatus(
    const extensions::IdentityTokenCacheValue& token_cache_value) {
  switch (token_cache_value.status()) {
    case extensions::IdentityTokenCacheValue::CACHE_STATUS_ADVICE:
      // Fallthrough to NOT FOUND case, as ADVICE is short lived.
    case extensions::IdentityTokenCacheValue::CACHE_STATUS_NOTFOUND:
      return l10n_util::GetStringUTF16(
          IDS_IDENTITY_INTERNALS_TOKEN_NOT_FOUND);
    case extensions::IdentityTokenCacheValue::CACHE_STATUS_TOKEN:
      return l10n_util::GetStringUTF16(
          IDS_IDENTITY_INTERNALS_TOKEN_PRESENT);
  }
  NOTREACHED();
  return base::string16();
}

const std::string IdentityInternalsUIMessageHandler::GetExpirationTime(
    const extensions::IdentityTokenCacheValue& token_cache_value) {
  return base::UTF16ToUTF8(base::TimeFormatFriendlyDateAndTime(
      token_cache_value.expiration_time()));
}

std::unique_ptr<base::DictionaryValue>
IdentityInternalsUIMessageHandler::GetInfoForToken(
    const extensions::ExtensionTokenKey& token_cache_key,
    const extensions::IdentityTokenCacheValue& token_cache_value) {
  std::unique_ptr<base::DictionaryValue> token_data(
      new base::DictionaryValue());
  token_data->SetString(kExtensionId, token_cache_key.extension_id);
  token_data->SetString(kExtensionName, GetExtensionName(token_cache_key));
  token_data->Set(kScopes, GetScopes(token_cache_key));
  token_data->SetString(kStatus, GetStatus(token_cache_value));
  token_data->SetString(kAccessToken, token_cache_value.token());
  token_data->SetString(kTokenExpirationTime,
                        GetExpirationTime(token_cache_value));
  return token_data;
}

void IdentityInternalsUIMessageHandler::GetInfoForAllTokens(
    const base::ListValue* args) {
  base::ListValue results;
  extensions::IdentityAPI::CachedTokens tokens;
  // The API can be null in incognito.
  extensions::IdentityAPI* api =
      extensions::IdentityAPI::GetFactoryInstance()->Get(
          Profile::FromWebUI(web_ui()));
  if (api)
    tokens = api->GetAllCachedTokens();
  for (extensions::IdentityAPI::CachedTokens::const_iterator
           iter = tokens.begin(); iter != tokens.end(); ++iter) {
    results.Append(GetInfoForToken(iter->first, iter->second));
  }

  web_ui()->CallJavascriptFunctionUnsafe("identity_internals.returnTokens",
                                         results);
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
    const base::ListValue* args) {
  std::string extension_id;
  std::string access_token;
  args->GetString(kRevokeTokenExtensionOffset, &extension_id);
  args->GetString(kRevokeTokenTokenOffset, &access_token);
  token_revokers_.push_back(std::make_unique<IdentityInternalsTokenRevoker>(
      extension_id, access_token, Profile::FromWebUI(web_ui()), this));
}

IdentityInternalsTokenRevoker::IdentityInternalsTokenRevoker(
    const std::string& extension_id,
    const std::string& access_token,
    Profile* profile,
    IdentityInternalsUIMessageHandler* consumer)
    : fetcher_(this,
               GaiaConstants::kChromeSource,
               profile->GetURLLoaderFactory()),
      extension_id_(extension_id),
      access_token_(access_token),
      consumer_(consumer) {
  DCHECK(consumer_);
  fetcher_.StartRevokeOAuth2Token(access_token);
}

IdentityInternalsTokenRevoker::~IdentityInternalsTokenRevoker() {}

void IdentityInternalsTokenRevoker::OnOAuth2RevokeTokenCompleted(
    GaiaAuthConsumer::TokenRevocationStatus status) {
  consumer_->OnTokenRevokerDone(this);
}

}  // namespace

IdentityInternalsUI::IdentityInternalsUI(content::WebUI* web_ui)
  : content::WebUIController(web_ui) {
  // chrome://identity-internals source.
  content::WebUIDataSource* html_source =
    content::WebUIDataSource::Create(chrome::kChromeUIIdentityInternalsHost);

  // Localized strings
  html_source->AddLocalizedString("tokenCacheHeader",
      IDS_IDENTITY_INTERNALS_TOKEN_CACHE_TEXT);
  html_source->AddLocalizedString("accessToken",
      IDS_IDENTITY_INTERNALS_ACCESS_TOKEN);
  html_source->AddLocalizedString("extensionName",
      IDS_IDENTITY_INTERNALS_EXTENSION_NAME);
  html_source->AddLocalizedString("extensionId",
      IDS_IDENTITY_INTERNALS_EXTENSION_ID);
  html_source->AddLocalizedString("tokenStatus",
      IDS_IDENTITY_INTERNALS_TOKEN_STATUS);
  html_source->AddLocalizedString("expirationTime",
      IDS_IDENTITY_INTERNALS_EXPIRATION_TIME);
  html_source->AddLocalizedString("scopes",
      IDS_IDENTITY_INTERNALS_SCOPES);
  html_source->AddLocalizedString("revoke",
      IDS_IDENTITY_INTERNALS_REVOKE);
  html_source->SetJsonPath("strings.js");

  // Required resources
  html_source->AddResourcePath("identity_internals.css",
      IDR_IDENTITY_INTERNALS_CSS);
  html_source->AddResourcePath("identity_internals.js",
      IDR_IDENTITY_INTERNALS_JS);
  html_source->SetDefaultResource(IDR_IDENTITY_INTERNALS_HTML);

  content::WebUIDataSource::Add(Profile::FromWebUI(web_ui), html_source);

  web_ui->AddMessageHandler(
      std::make_unique<IdentityInternalsUIMessageHandler>());
}

IdentityInternalsUI::~IdentityInternalsUI() {}
