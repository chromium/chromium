// Copyright 2014 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/update_client/utils.h"

#include <stddef.h>

#include <cmath>
#include <cstring>
#include <memory>
#include <utility>
#include <vector>

#include "base/check.h"
#include "base/containers/contains.h"
#include "base/files/file_path.h"
#include "base/files/file_util.h"
#include "base/files/memory_mapped_file.h"
#include "base/functional/callback.h"
#include "base/json/json_file_value_serializer.h"
#include "base/path_service.h"
#include "base/ranges/algorithm.h"
#include "base/strings/string_number_conversions.h"
#include "base/strings/string_util.h"
#include "base/system/sys_info.h"
#include "base/threading/platform_thread.h"
#include "base/threading/scoped_blocking_call.h"
#include "base/time/time.h"
#include "base/values.h"
#include "components/crx_file/id_util.h"
#include "components/update_client/component.h"
#include "components/update_client/configurator.h"
#include "components/update_client/network.h"
#include "components/update_client/update_client.h"
#include "components/update_client/update_client_errors.h"
#include "crypto/secure_hash.h"
#include "crypto/sha2.h"
#include "url/gurl.h"

#if BUILDFLAG(IS_WIN)
#include <shlobj.h>

#include "base/win/windows_version.h"
#endif  // BUILDFLAG(IS_WIN)

namespace update_client {

const char kArchAmd64[] = "x86_64";
const char kArchIntel[] = "x86";
const char kArchArm64[] = "arm64";

bool IsHttpServerError(int status_code) {
  return 500 <= status_code && status_code < 600;
}

bool DeleteFileAndEmptyParentDirectory(const base::FilePath& filepath) {
  if (!base::DeleteFile(filepath)) {
    return false;
  }

  return DeleteEmptyDirectory(filepath.DirName());
}

bool DeleteEmptyDirectory(const base::FilePath& dir_path) {
  if (!base::IsDirectoryEmpty(dir_path)) {
    return true;
  }

  return base::DeleteFile(dir_path);
}

std::string GetCrxComponentID(const CrxComponent& component) {
  return component.app_id.empty() ? GetCrxIdFromPublicKeyHash(component.pk_hash)
                                  : component.app_id;
}

std::string GetCrxIdFromPublicKeyHash(base::span<const uint8_t> pk_hash) {
  const std::string result = crx_file::id_util::GenerateIdFromHash(pk_hash);
  CHECK(crx_file::id_util::IdIsValid(result));
  return result;
}

bool VerifyFileHash256(const base::FilePath& filepath,
                       const std::string& expected_hash_str) {
  std::vector<uint8_t> expected_hash;
  if (!base::HexStringToBytes(expected_hash_str, &expected_hash) ||
      expected_hash.size() != crypto::kSHA256Length) {
    return false;
  }

  std::unique_ptr<crypto::SecureHash> hasher(
      crypto::SecureHash::Create(crypto::SecureHash::SHA256));

  int64_t file_size = 0;
  if (!base::GetFileSize(filepath, &file_size)) {
    return false;
  }
  if (file_size > 0) {
    base::MemoryMappedFile mmfile;
    if (!mmfile.Initialize(filepath)) {
      return false;
    }
    hasher->Update(mmfile.data(), mmfile.length());
  }

  uint8_t actual_hash[crypto::kSHA256Length] = {0};
  hasher->Finish(actual_hash, sizeof(actual_hash));

  return memcmp(actual_hash, &expected_hash[0], sizeof(actual_hash)) == 0;
}

bool IsValidBrand(const std::string& brand) {
  const size_t kMaxBrandSize = 4;
  return brand.empty() ||
         (brand.size() == kMaxBrandSize &&
          base::ranges::all_of(brand, &base::IsAsciiAlpha<char>));
}

// Helper function.
// Returns true if |part| matches the expression
// ^[<special_chars>a-zA-Z0-9]{min_length,max_length}$
bool IsValidInstallerAttributePart(const std::string& part,
                                   const std::string& special_chars,
                                   size_t min_length,
                                   size_t max_length) {
  return part.size() >= min_length && part.size() <= max_length &&
         base::ranges::all_of(part, [&special_chars](char ch) {
           return base::IsAsciiAlpha(ch) || base::IsAsciiDigit(ch) ||
                  base::Contains(special_chars, ch);
         });
}

// Returns true if the |name| parameter matches ^[-_a-zA-Z0-9]{1,256}$ .
bool IsValidInstallerAttributeName(const std::string& name) {
  return IsValidInstallerAttributePart(name, "-_", 1, 256);
}

// Returns true if the |value| parameter matches ^[-.,;+_=$a-zA-Z0-9]{0,256}$ .
bool IsValidInstallerAttributeValue(const std::string& value) {
  return IsValidInstallerAttributePart(value, "-.,;+_=$", 0, 256);
}

bool IsValidInstallerAttribute(const InstallerAttribute& attr) {
  return IsValidInstallerAttributeName(attr.first) &&
         IsValidInstallerAttributeValue(attr.second);
}

void RemoveUnsecureUrls(std::vector<GURL>* urls) {
  CHECK(urls);
  std::erase_if(*urls,
                [](const GURL& url) { return !url.SchemeIsCryptographic(); });
}

CrxInstaller::Result InstallFunctionWrapper(
    base::OnceCallback<bool()> callback) {
  return CrxInstaller::Result(std::move(callback).Run()
                                  ? InstallError::NONE
                                  : InstallError::GENERIC_ERROR);
}

std::optional<base::Value::Dict> ReadManifest(
    const base::FilePath& unpack_path) {
  base::FilePath manifest =
      unpack_path.Append(FILE_PATH_LITERAL("manifest.json"));
  if (!base::PathExists(manifest)) {
    return std::nullopt;
  }
  JSONFileValueDeserializer deserializer(manifest);
  std::string error;
  std::unique_ptr<base::Value> root = deserializer.Deserialize(nullptr, &error);
  if (!root || !root->is_dict()) {
    return std::nullopt;
  }
  return std::move(root->GetDict());
}

std::string GetArchitecture() {
#if BUILDFLAG(IS_WIN)
  const base::win::OSInfo* os_info = base::win::OSInfo::GetInstance();
  return (os_info->IsWowX86OnARM64() || os_info->IsWowAMD64OnARM64())
             ? kArchArm64
             : base::SysInfo().OperatingSystemArchitecture();
#else   // BUILDFLAG(IS_WIN)
  return base::SysInfo().OperatingSystemArchitecture();
#endif  // BUILDFLAG(IS_WIN)
}

bool RetryDeletePathRecursively(const base::FilePath& path) {
  return RetryDeletePathRecursivelyCustom(
      path, /*tries=*/5,
      /*seconds_between_tries=*/base::Seconds(1));
}

bool RetryDeletePathRecursivelyCustom(const base::FilePath& path,
                                      size_t tries,
                                      base::TimeDelta seconds_between_tries) {
  base::ScopedBlockingCall scoped_blocking_call(FROM_HERE,
                                                base::BlockingType::WILL_BLOCK);
  for (size_t i = 0;;) {
    if (base::DeletePathRecursively(path)) {
      return true;
    }
    if (++i >= tries) {
      break;
    }
    base::PlatformThread::Sleep(seconds_between_tries);
  }
  return false;
}

}  // namespace update_client
