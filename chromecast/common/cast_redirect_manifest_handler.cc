// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chromecast/common/cast_redirect_manifest_handler.h"

#include <vector>

#include "base/strings/utf_string_conversions.h"
#include "base/values.h"

namespace chromecast {

namespace {

const char kCastRedirect[] = "cast_redirect";
const char kCastUrl[] = "cast_url";

class Data : public extensions::Extension::ManifestData {
 public:
  ~Data() override {}

  std::string cast_url;
  std::vector<std::pair<std::string, std::string>> redirects;
};

}  // namespace

CastRedirectHandler::CastRedirectHandler() {}
CastRedirectHandler::~CastRedirectHandler() {}

bool CastRedirectHandler::Parse(extensions::Extension* extension,
                                std::u16string* error) {
  std::unique_ptr<Data> info(new Data);
  const base::DictionaryValue* dict;
  if (extension->manifest()->GetDictionary(kCastRedirect, &dict)) {
    for (const auto kv : dict->DictItems()) {
      if (kv.second.is_string()) {
        info->redirects.emplace_back(kv.first, kv.second.GetString());
      }
    }
  }

  if (const std::string* url =
          extension->manifest()->FindStringPath(kCastUrl)) {
    info->cast_url = *url;
  }

  if (!info->redirects.empty() || !info->cast_url.empty()) {
    extension->SetManifestData(kCastRedirect, std::move(info));
  }
  return true;
}

bool CastRedirectHandler::Validate(
    const extensions::Extension* extension,
    std::string* error,
    std::vector<extensions::InstallWarning>* warnings) const {
  return true;
}

bool CastRedirectHandler::ParseUrl(std::string* out_url,
                                   const extensions::Extension* extension,
                                   const GURL& url) {
  Data* info = static_cast<Data*>(extension->GetManifestData(kCastRedirect));
  if (!info)
    return false;

  std::string path = url.path();
  for (const auto& redirect : info->redirects) {
    const std::string& prefix = redirect.first;
    if (!path.compare(0, prefix.size(), prefix)) {
      std::string s = redirect.second;
      *out_url = redirect.second;
      out_url->append(path, prefix.size(), path.size());
      return true;
    }
  }

  if (!info->cast_url.empty()) {
    *out_url = info->cast_url;
    out_url->append(url.path());
    return true;
  }

  return false;
}

base::span<const char* const> CastRedirectHandler::Keys() const {
  static constexpr const char* kKeys[] = {kCastRedirect, kCastUrl};
  return kKeys;
}

}  // namespace chromecast
