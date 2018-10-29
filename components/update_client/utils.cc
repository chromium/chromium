// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/update_client/utils.h"

#include <stddef.h>

#include <algorithm>
#include <cmath>
#include <cstring>

#include "base/callback.h"
#include "base/files/file_path.h"
#include "base/files/file_util.h"
#include "base/files/memory_mapped_file.h"
#include "base/json/json_file_value_serializer.h"
#include "base/stl_util.h"
#include "base/strings/string_number_conversions.h"
#include "base/strings/string_piece.h"
#include "base/strings/string_util.h"
#include "base/values.h"
#include "components/crx_file/id_util.h"
#include "components/data_use_measurement/core/data_use_user_data.h"
#include "components/update_client/component.h"
#include "components/update_client/configurator.h"
#include "components/update_client/update_client.h"
#include "components/update_client/update_client_errors.h"
#include "crypto/secure_hash.h"
#include "crypto/sha2.h"
#include "net/base/load_flags.h"
#include "net/traffic_annotation/network_traffic_annotation.h"
#include "services/network/public/cpp/resource_request.h"
#include "services/network/public/cpp/simple_url_loader.h"
#include "url/gurl.h"

namespace update_client {

std::unique_ptr<network::SimpleURLLoader> SendProtocolRequest(
    const GURL& url,
    const base::flat_map<std::string, std::string>&
        protocol_request_extra_headers,
    const std::string& protocol_request,
    network::SimpleURLLoader::BodyAsStringCallback callback,
    scoped_refptr<network::SharedURLLoaderFactory> url_loader_factory) {
  net::NetworkTrafficAnnotationTag traffic_annotation =
      net::DefineNetworkTrafficAnnotation("component_updater_utils", R"(
        semantics {
          sender: "Component Updater"
          description:
            "The component updater in Chrome is responsible for updating code "
            "and data modules such as Flash, CrlSet, Origin Trials, etc. These "
            "modules are updated on cycles independent of the Chrome release "
            "tracks. It runs in the browser process and communicates with a "
            "set of servers using the Omaha protocol to find the latest "
            "versions of components, download them, and register them with the "
            "rest of Chrome."
          trigger: "Manual or automatic software updates."
          data:
            "Various OS and Chrome parameters such as version, bitness, "
            "release tracks, etc."
          destination: GOOGLE_OWNED_SERVICE
        }
        policy {
          cookies_allowed: NO
          setting: "This feature cannot be disabled."
          chrome_policy {
            ComponentUpdatesEnabled {
              policy_options {mode: MANDATORY}
              ComponentUpdatesEnabled: false
            }
          }
        })");
  // Create and initialize URL loader.
  auto resource_request = std::make_unique<network::ResourceRequest>();
  resource_request->url = url;
  resource_request->method = "POST";
  resource_request->load_flags = net::LOAD_DO_NOT_SEND_COOKIES |
                                 net::LOAD_DO_NOT_SAVE_COOKIES |
                                 net::LOAD_DISABLE_CACHE;
  for (const auto& header : protocol_request_extra_headers)
    resource_request->headers.SetHeader(header.first, header.second);

  auto simple_loader = network::SimpleURLLoader::Create(
      std::move(resource_request), traffic_annotation);
  const int max_retry_on_network_change = 3;
  simple_loader->SetRetryOptions(
      max_retry_on_network_change,
      network::SimpleURLLoader::RETRY_ON_NETWORK_CHANGE);
  simple_loader->AttachStringForUpload(protocol_request, "application/xml");
  simple_loader->DownloadToStringOfUnboundedSizeUntilCrashAndDie(
      url_loader_factory.get(), std::move(callback));
  return simple_loader;
}

bool HasDiffUpdate(const Component& component) {
  return !component.crx_diffurls().empty();
}

bool IsHttpServerError(int status_code) {
  return 500 <= status_code && status_code < 600;
}

bool DeleteFileAndEmptyParentDirectory(const base::FilePath& filepath) {
  if (!base::DeleteFile(filepath, false))
    return false;

  const base::FilePath dirname(filepath.DirName());
  if (!base::IsDirectoryEmpty(dirname))
    return true;

  return base::DeleteFile(dirname, false);
}

std::string GetCrxComponentID(const CrxComponent& component) {
  const std::string result = crx_file::id_util::GenerateIdFromHash(
      &component.pk_hash[0], component.pk_hash.size());
  DCHECK(crx_file::id_util::IdIsValid(result));
  return result;
}

bool VerifyFileHash256(const base::FilePath& filepath,
                       const std::string& expected_hash_str) {
  std::vector<uint8_t> expected_hash;
  if (!base::HexStringToBytes(expected_hash_str, &expected_hash) ||
      expected_hash.size() != crypto::kSHA256Length) {
    return false;
  }

  base::MemoryMappedFile mmfile;
  if (!mmfile.Initialize(filepath))
    return false;

  uint8_t actual_hash[crypto::kSHA256Length] = {0};
  std::unique_ptr<crypto::SecureHash> hasher(
      crypto::SecureHash::Create(crypto::SecureHash::SHA256));
  hasher->Update(mmfile.data(), mmfile.length());
  hasher->Finish(actual_hash, sizeof(actual_hash));

  return memcmp(actual_hash, &expected_hash[0], sizeof(actual_hash)) == 0;
}

bool IsValidBrand(const std::string& brand) {
  const size_t kMaxBrandSize = 4;
  if (!brand.empty() && brand.size() != kMaxBrandSize)
    return false;

  return std::find_if_not(brand.begin(), brand.end(), [](char ch) {
           return base::IsAsciiAlpha(ch);
         }) == brand.end();
}

// Helper function.
// Returns true if |part| matches the expression
// ^[<special_chars>a-zA-Z0-9]{min_length,max_length}$
bool IsValidInstallerAttributePart(const std::string& part,
                                   const std::string& special_chars,
                                   size_t min_length,
                                   size_t max_length) {
  if (part.size() < min_length || part.size() > max_length)
    return false;

  return std::find_if_not(part.begin(), part.end(), [&special_chars](char ch) {
           if (base::IsAsciiAlpha(ch) || base::IsAsciiDigit(ch))
             return true;

           for (auto c : special_chars) {
             if (c == ch)
               return true;
           }

           return false;
         }) == part.end();
}

// Returns true if the |name| parameter matches ^[-_a-zA-Z0-9]{1,256}$ .
bool IsValidInstallerAttributeName(const std::string& name) {
  return IsValidInstallerAttributePart(name, "-_", 1, 256);
}

// Returns true if the |value| parameter matches ^[-.,;+_=a-zA-Z0-9]{0,256}$ .
bool IsValidInstallerAttributeValue(const std::string& value) {
  return IsValidInstallerAttributePart(value, "-.,;+_=", 0, 256);
}

bool IsValidInstallerAttribute(const InstallerAttribute& attr) {
  return IsValidInstallerAttributeName(attr.first) &&
         IsValidInstallerAttributeValue(attr.second);
}

void RemoveUnsecureUrls(std::vector<GURL>* urls) {
  DCHECK(urls);
  base::EraseIf(*urls,
                [](const GURL& url) { return !url.SchemeIsCryptographic(); });
}

CrxInstaller::Result InstallFunctionWrapper(
    base::OnceCallback<bool()> callback) {
  return CrxInstaller::Result(std::move(callback).Run()
                                  ? InstallError::NONE
                                  : InstallError::GENERIC_ERROR);
}

// TODO(cpu): add a specific attribute check to a component json that the
// extension unpacker will reject, so that a component cannot be installed
// as an extension.
std::unique_ptr<base::DictionaryValue> ReadManifest(
    const base::FilePath& unpack_path) {
  base::FilePath manifest =
      unpack_path.Append(FILE_PATH_LITERAL("manifest.json"));
  if (!base::PathExists(manifest))
    return std::unique_ptr<base::DictionaryValue>();
  JSONFileValueDeserializer deserializer(manifest);
  std::string error;
  std::unique_ptr<base::Value> root = deserializer.Deserialize(nullptr, &error);
  if (!root)
    return std::unique_ptr<base::DictionaryValue>();
  if (!root->is_dict())
    return std::unique_ptr<base::DictionaryValue>();
  return std::unique_ptr<base::DictionaryValue>(
      static_cast<base::DictionaryValue*>(root.release()));
}

}  // namespace update_client
