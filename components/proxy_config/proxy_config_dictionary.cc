// Copyright 2011 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/proxy_config/proxy_config_dictionary.h"

#include <memory>
#include <utility>

#include "base/check.h"
#include "net/base/proxy_server.h"
#include "net/base/proxy_string_util.h"
#include "net/proxy_resolution/proxy_config.h"

namespace {

// Integer to specify the type of proxy settings.
// See ProxyPrefs for possible values and interactions with the other proxy
// preferences.
const char kProxyMode[] = "mode";
// String specifying the proxy server. For a specification of the expected
// syntax see net::ProxyConfig::ProxyRules::ParseFromString().
const char kProxyServer[] = "server";
// URL to the proxy .pac file.
const char kProxyPacUrl[] = "pac_url";
// Optional boolean flag indicating whether a valid PAC script is mandatory.
// If true, network traffic does not fall back to direct connections in case the
// PAC script is not available.
const char kProxyPacMandatory[] = "pac_mandatory";
// String containing proxy bypass rules. For a specification of the
// expected syntax see net::ProxyBypassRules::ParseFromString().
const char kProxyBypassList[] = "bypass_list";

}  // namespace

ProxyConfigDictionary::ProxyConfigDictionary(base::Value::Dict dict)
    : dict_(std::move(dict)) {}

ProxyConfigDictionary::ProxyConfigDictionary(ProxyConfigDictionary&& other) {
  dict_ = std::move(other.dict_);
}

ProxyConfigDictionary::~ProxyConfigDictionary() = default;

bool ProxyConfigDictionary::GetMode(ProxyPrefs::ProxyMode* out) const {
  const base::Value* mode_value = dict_.Find(kProxyMode);
  if (!mode_value || !mode_value->is_string())
    return false;
  std::string mode_str = mode_value->GetString();
  return StringToProxyMode(mode_str, out);
}

bool ProxyConfigDictionary::GetPacUrl(std::string* out) const {
  return GetString(kProxyPacUrl, out);
}

bool ProxyConfigDictionary::GetPacMandatory(bool* out) const {
  const base::Value* value = dict_.Find(kProxyPacMandatory);
  if (!value || !value->is_bool()) {
    *out = false;
    return false;
  }
  *out = value->GetBool();
  return true;
}

bool ProxyConfigDictionary::GetProxyServer(std::string* out) const {
  return GetString(kProxyServer, out);
}

bool ProxyConfigDictionary::GetBypassList(std::string* out) const {
  return GetString(kProxyBypassList, out);
}

bool ProxyConfigDictionary::HasBypassList() const {
  return dict_.Find(kProxyBypassList);
}

const base::Value::Dict& ProxyConfigDictionary::GetDictionary() const {
  return dict_;
}

// static
base::Value::Dict ProxyConfigDictionary::CreateDirect() {
  return CreateDictionary(ProxyPrefs::MODE_DIRECT, std::string(), false,
                          std::string(), std::string());
}

// static
base::Value::Dict ProxyConfigDictionary::CreateAutoDetect() {
  return CreateDictionary(ProxyPrefs::MODE_AUTO_DETECT, std::string(), false,
                          std::string(), std::string());
}

// static
base::Value::Dict ProxyConfigDictionary::CreatePacScript(
    const std::string& pac_url,
    bool pac_mandatory) {
  return CreateDictionary(ProxyPrefs::MODE_PAC_SCRIPT, pac_url, pac_mandatory,
                          std::string(), std::string());
}

// static
base::Value::Dict ProxyConfigDictionary::CreateFixedServers(
    const std::string& proxy_server,
    const std::string& bypass_list) {
  if (!proxy_server.empty()) {
    return CreateDictionary(ProxyPrefs::MODE_FIXED_SERVERS, std::string(),
                            false, proxy_server, bypass_list);
  } else {
    return CreateDirect();
  }
}

// static
base::Value::Dict ProxyConfigDictionary::CreateSystem() {
  return CreateDictionary(ProxyPrefs::MODE_SYSTEM, std::string(), false,
                          std::string(), std::string());
}

// static
base::Value::Dict ProxyConfigDictionary::CreateDictionary(
    ProxyPrefs::ProxyMode mode,
    const std::string& pac_url,
    bool pac_mandatory,
    const std::string& proxy_server,
    const std::string& bypass_list) {
  base::Value::Dict dict;
  dict.Set(kProxyMode, base::Value(ProxyModeToString(mode)));
  if (!pac_url.empty()) {
    dict.Set(kProxyPacUrl, base::Value(pac_url));
    dict.Set(kProxyPacMandatory, base::Value(pac_mandatory));
  }
  if (!proxy_server.empty())
    dict.Set(kProxyServer, base::Value(proxy_server));
  if (!bypass_list.empty())
    dict.Set(kProxyBypassList, base::Value(bypass_list));
  return dict;
}

// static
void ProxyConfigDictionary::EncodeAndAppendProxyServer(
    const std::string& url_scheme,
    const net::ProxyServer& server,
    std::string* spec) {
  if (!server.is_valid())
    return;

  if (!spec->empty())
    *spec += ';';

  if (!url_scheme.empty()) {
    *spec += url_scheme;
    *spec += "=";
  }
  *spec += net::ProxyServerToProxyUri(server);
}

bool ProxyConfigDictionary::GetString(const char* key, std::string* out) const {
  const base::Value* value = dict_.Find(key);
  if (!value || !value->is_string()) {
    *out = "";
    return false;
  }
  *out = value->GetString();
  return true;
}
