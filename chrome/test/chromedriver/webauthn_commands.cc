// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/test/chromedriver/webauthn_commands.h"

#include <string>
#include <string_view>
#include <utility>
#include <vector>

#include "base/base64.h"
#include "base/base64url.h"
#include "base/containers/flat_map.h"
#include "base/functional/callback.h"
#include "base/strings/strcat.h"
#include "base/values.h"
#include "chrome/test/chromedriver/chrome/status.h"
#include "chrome/test/chromedriver/chrome/web_view.h"
#include "chrome/test/chromedriver/session.h"

namespace {

static constexpr char kArgumentMustBeAList[] = " must be a list";
static constexpr char kBase64UrlError[] = " must be a base64url encoded string";
static constexpr char kDevToolsDidNotReturnExpectedValue[] =
    "DevTools did not return the expected value";
static constexpr char kExtensionsMustBeList[] =
    "extensions must be a list of strings";
static constexpr char kItemsMustBeBase64UrlEncoded[] =
    " items must be base64url encoded strings";
static constexpr char kUnrecognizedExtension[] =
    " is not a recognized extension";
static constexpr char kUnrecognizedProtocol[] =
    " is not a recognized protocol version";

// Creates a base::DictValue by cloning the parameters specified by
// |mapping| from |params|.
base::DictValue MapParams(
    const base::flat_map<const char*, const char*>& mapping,
    const base::DictValue& params) {
  base::DictValue options;
  for (const std::pair<const char*, const char*>& pair : mapping) {
    const base::Value* value = params.Find(pair.second);
    if (value)
      options.SetByDottedPath(pair.first, value->Clone());
  }
  return options;
}

// Converts a single base64url encoded string |value| to base64. Returns a
// status error if the value is not a string or if conversion failed.
[[nodiscard]] Status ConvertValueBase64UrlToBase64(
    base::Value& value,
    std::string_view error_message) {
  if (!value.is_string()) {
    return Status(kInvalidArgument, std::string(error_message));
  }
  std::string& str = value.GetString();
  std::string temp;
  if (!Base64UrlDecode(str, base::Base64UrlDecodePolicy::IGNORE_PADDING,
                       &temp)) {
    return Status(kInvalidArgument, std::string(error_message));
  }
  str = base::Base64Encode(temp);
  return Status(kOk);
}

// Converts a single base64 encoded string |value| to base64url.
[[nodiscard]] Status ConvertValueBase64ToBase64Url(base::Value& value) {
  if (!value.is_string()) {
    return Status(kInvalidArgument, kDevToolsDidNotReturnExpectedValue);
  }
  std::string& str = value.GetString();
  std::string temp;
  if (!base::Base64Decode(str, &temp)) {
    return Status(kInvalidArgument, kDevToolsDidNotReturnExpectedValue);
  }
  base::Base64UrlEncode(temp, base::Base64UrlEncodePolicy::OMIT_PADDING, &str);
  return Status(kOk);
}

// Converts the string |keys| in |params| from base64url to base64. Returns a
// status error if conversion of one of the keys failed.
[[nodiscard]] Status ConvertBase64UrlToBase64(
    base::DictValue& params,
    const std::vector<std::string>& keys) {
  for (const std::string& key : keys) {
    base::Value* maybe_value = params.Find(key);
    if (!maybe_value) {
      continue;
    }

    Status status = ConvertValueBase64UrlToBase64(
        *maybe_value, base::StrCat({key, kBase64UrlError}));
    if (status.IsError()) {
      return status;
    }
  }

  return Status(kOk);
}

// Converts the string |keys| in |params| from base64 to base64url.
[[nodiscard]] Status ConvertBase64ToBase64Url(
    base::DictValue& params,
    const std::vector<std::string>& keys) {
  for (const std::string& key : keys) {
    base::Value* maybe_value = params.Find(key);
    if (!maybe_value) {
      continue;
    }

    Status status = ConvertValueBase64ToBase64Url(*maybe_value);
    if (status.IsError()) {
      return status;
    }
  }
  return Status(kOk);
}

// Maps the signCount parameter, handling null as -1.
void MapSignCount(const base::DictValue& params, base::DictValue& target) {
  const base::Value* sign_count = params.Find("signCount");
  if (sign_count) {
    if (sign_count->is_none()) {
      target.Set("signCount", -1);
    } else {
      target.Set("signCount", sign_count->Clone());
    }
  }
}

// Converts a list of base64url encoded strings in `params` under `key` to
// base64. Returns a status error if conversion of one of the items failed.
[[nodiscard]] Status ConvertListBase64UrlToBase64(base::DictValue& params,
                                                  std::string_view key) {
  base::Value* maybe_value = params.Find(key);
  if (!maybe_value) {
    return Status(kOk);
  }

  if (!maybe_value->is_list()) {
    return Status(kInvalidArgument, base::StrCat({key, kArgumentMustBeAList}));
  }

  for (base::Value& item : maybe_value->GetList()) {
    Status status = ConvertValueBase64UrlToBase64(
        item, base::StrCat({key, kItemsMustBeBase64UrlEncoded}));
    if (status.IsError()) {
      return status;
    }
  }
  return Status(kOk);
}

// Converts a list of base64 encoded strings in `params` under `key` to
// base64url.
[[nodiscard]] Status ConvertListBase64ToBase64Url(base::DictValue& params,
                                                  std::string_view key) {
  base::Value* maybe_value = params.Find(key);
  if (!maybe_value) {
    return Status(kOk);
  }
  if (!maybe_value->is_list()) {
    return Status(kInvalidArgument, kDevToolsDidNotReturnExpectedValue);
  }

  for (base::Value& item : maybe_value->GetList()) {
    Status status = ConvertValueBase64ToBase64Url(item);
    if (status.IsError()) {
      return status;
    }
  }
  return Status(kOk);
}

}  // namespace

Status ExecuteWebAuthnCommand(const WebAuthnCommand& command,
                              Session* session,
                              const base::DictValue& params,
                              std::unique_ptr<base::Value>* value) {
  WebView* web_view = nullptr;
  Status status = session->GetTargetWindow(&web_view);
  if (status.IsError())
    return status;

  std::unique_ptr<WebViewHolder> scoped_web_view_lock = web_view->GetHolder();

  status = web_view->SendCommand("WebAuthn.enable", base::DictValue());
  if (status.IsError())
    return status;

  return command.Run(web_view, params, value);
}

Status ExecuteAddVirtualAuthenticator(WebView* web_view,
                                      const base::DictValue& params,
                                      std::unique_ptr<base::Value>* value) {
  base::DictValue mapped_params = MapParams(
      {
          {"options.protocol", "protocol"},
          {"options.transport", "transport"},
          {"options.hasResidentKey", "hasResidentKey"},
          {"options.hasUserVerification", "hasUserVerification"},
          {"options.automaticPresenceSimulation", "isUserConsenting"},
          {"options.isUserVerified", "isUserVerified"},
          {"options.defaultBackupState", "defaultBackupState"},
          {"options.defaultBackupEligibility", "defaultBackupEligibility"},
      },
      params);

  const base::Value* extensions = params.Find("extensions");
  if (extensions) {
    if (!extensions->is_list())
      return Status(kInvalidArgument, kExtensionsMustBeList);
    for (const base::Value& extension : extensions->GetList()) {
      if (!extension.is_string())
        return Status(kInvalidArgument, kExtensionsMustBeList);
      const std::string& extension_string = extension.GetString();
      if (extension_string == "largeBlob") {
        mapped_params.SetByDottedPath("options.hasLargeBlob", true);
      } else if (extension_string == "credBlob") {
        mapped_params.SetByDottedPath("options.hasCredBlob", true);
      } else if (extension_string == "minPinLength") {
        mapped_params.SetByDottedPath("options.hasMinPinLength", true);
      } else if (extension_string == "prf") {
        mapped_params.SetByDottedPath("options.hasPrf", true);
      } else if (extension_string == "cmtgKey") {
        mapped_params.SetByDottedPath("options.hasCmtgKey", true);
      } else {
        return Status(kUnsupportedOperation,
                      extension_string + kUnrecognizedExtension);
      }
    }
  }

  // The spec calls u2f "ctap1/u2f", convert the value here since devtools does
  // not support slashes on enums.
  std::string* protocol =
      mapped_params.FindStringByDottedPath("options.protocol");
  if (protocol) {
    if (*protocol == "ctap1/u2f") {
      *protocol = "u2f";
    } else if (*protocol == "ctap2") {
      mapped_params.SetByDottedPath("options.ctap2Version", "ctap2_0");
    } else if (*protocol == "ctap2_1") {
      *protocol = "ctap2";
      mapped_params.SetByDottedPath("options.ctap2Version", "ctap2_1");
    } else {
      return Status(kInvalidArgument, *protocol + kUnrecognizedProtocol);
    }
  }

  std::unique_ptr<base::Value> result;
  Status status = web_view->SendCommandAndGetResult(
      "WebAuthn.addVirtualAuthenticator", mapped_params, &result);
  if (status.IsError())
    return status;

  std::optional<base::Value> authenticator_id =
      result->GetDict().Extract("authenticatorId");
  if (!authenticator_id)
    return Status(kUnknownError, kDevToolsDidNotReturnExpectedValue);

  *value = std::make_unique<base::Value>(std::move(*authenticator_id));
  return status;
}

Status ExecuteRemoveVirtualAuthenticator(WebView* web_view,
                                         const base::DictValue& params,
                                         std::unique_ptr<base::Value>* value) {
  return web_view->SendCommandAndGetResult(
      "WebAuthn.removeVirtualAuthenticator",
      MapParams({{"authenticatorId", "authenticatorId"}}, params), value);
}

Status ExecuteAddCredential(WebView* web_view,
                            const base::DictValue& params,
                            std::unique_ptr<base::Value>* value) {
  base::DictValue mapped_params = MapParams(
      {
          {"authenticatorId", "authenticatorId"},
          {"credential.credentialId", "credentialId"},
          {"credential.isResidentCredential", "isResidentCredential"},
          {"credential.rpId", "rpId"},
          {"credential.privateKey", "privateKey"},
          {"credential.userHandle", "userHandle"},
          {"credential.largeBlob", "largeBlob"},
          {"credential.backupEligibility", "backupEligibility"},
          {"credential.backupState", "backupState"},
          {"credential.userName", "userName"},
          {"credential.userDisplayName", "userDisplayName"},
          {"credential.cmtgKeys", "cmtgKeys"},
          {"credential.activeCmtgKeyIndex", "activeCmtgKeyIndex"},
          {"credential.generateCmtgKeyOnNextOperation",
           "generateCmtgKeyOnNextOperation"},
      },
      params);
  base::DictValue* credential = mapped_params.FindDict("credential");
  if (!credential)
    return Status(kInvalidArgument, "'credential' must be a JSON object");
  Status status = ConvertBase64UrlToBase64(
      *credential, {"credentialId", "privateKey", "userHandle", "largeBlob"});
  if (status.IsError())
    return status;

  MapSignCount(params, *credential);
  status = ConvertListBase64UrlToBase64(*credential, "cmtgKeys");
  if (status.IsError()) {
    return status;
  }

  return web_view->SendCommandAndGetResult("WebAuthn.addCredential",
                                           mapped_params, value);
}

Status ExecuteGetCredentials(WebView* web_view,
                             const base::DictValue& params,
                             std::unique_ptr<base::Value>* value) {
  std::unique_ptr<base::Value> result;
  Status status = web_view->SendCommandAndGetResult(
      "WebAuthn.getCredentials",
      MapParams({{"authenticatorId", "authenticatorId"}}, params), &result);
  if (status.IsError())
    return status;

  std::optional<base::Value> credentials =
      result->GetDict().Extract("credentials");
  if (!credentials)
    return Status(kUnknownError, kDevToolsDidNotReturnExpectedValue);

  for (base::Value& credential : credentials->GetList()) {
    if (!credential.is_dict())
      return Status(kUnknownError, kDevToolsDidNotReturnExpectedValue);
    const base::Value* sign_count = credential.GetDict().Find("signCount");
    if (sign_count && sign_count->is_int() && sign_count->GetInt() == -1) {
      credential.GetDict().Set("signCount", base::Value());
    }
    status = ConvertBase64ToBase64Url(
        credential.GetDict(),
        {"credentialId", "privateKey", "userHandle", "largeBlob"});
    if (status.IsError()) {
      return status;
    }
    status = ConvertListBase64ToBase64Url(credential.GetDict(), "cmtgKeys");
    if (status.IsError()) {
      return status;
    }
  }
  *value = std::make_unique<base::Value>(std::move(*credentials));
  return Status(kOk);
}

Status ExecuteRemoveCredential(WebView* web_view,
                               const base::DictValue& params,
                               std::unique_ptr<base::Value>* value) {
  base::DictValue mapped_params = MapParams(
      {
          {"authenticatorId", "authenticatorId"},
          {"credentialId", "credentialId"},
      },
      params);
  Status status = ConvertBase64UrlToBase64(mapped_params, {"credentialId"});
  if (status.IsError())
    return status;

  return web_view->SendCommandAndGetResult("WebAuthn.removeCredential",
                                           std::move(mapped_params), value);
}

Status ExecuteRemoveAllCredentials(WebView* web_view,
                                   const base::DictValue& params,
                                   std::unique_ptr<base::Value>* value) {
  return web_view->SendCommandAndGetResult(
      "WebAuthn.clearCredentials",
      MapParams({{"authenticatorId", "authenticatorId"}}, params), value);
}

Status ExecuteSetUserVerified(WebView* web_view,
                              const base::DictValue& params,
                              std::unique_ptr<base::Value>* value) {
  return web_view->SendCommandAndGetResult(
      "WebAuthn.setUserVerified",
      MapParams(
          {
              {"authenticatorId", "authenticatorId"},
              {"isUserVerified", "isUserVerified"},
          },
          params),
      value);
}

Status ExecuteSetCredentialProperties(WebView* web_view,
                                      const base::DictValue& params,
                                      std::unique_ptr<base::Value>* value) {
  base::DictValue mapped_params = MapParams(
      {
          {"authenticatorId", "authenticatorId"},
          {"credentialId", "credentialId"},
          {"backupEligibility", "backupEligibility"},
          {"backupState", "backupState"},
          {"activeCmtgKeyIndex", "activeCmtgKeyIndex"},
          {"generateCmtgKeyOnNextOperation", "generateCmtgKeyOnNextOperation"},
      },
      params);
  Status status = ConvertBase64UrlToBase64(mapped_params, {"credentialId"});
  if (status.IsError()) {
    return status;
  }

  MapSignCount(params, mapped_params);

  return web_view->SendCommandAndGetResult("WebAuthn.setCredentialProperties",
                                           mapped_params, value);
}
