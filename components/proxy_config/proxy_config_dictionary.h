// Copyright 2011 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_PROXY_CONFIG_PROXY_CONFIG_DICTIONARY_H_
#define COMPONENTS_PROXY_CONFIG_PROXY_CONFIG_DICTIONARY_H_

#include <memory>
#include <string>

#include "base/values.h"
#include "components/proxy_config/proxy_config_export.h"
#include "components/proxy_config/proxy_prefs.h"

namespace net {
class ProxyServer;
}

// Factory and wrapper for proxy config dictionaries that are stored
// in the user preferences. The dictionary has the following structure:
// {
//   mode: string,
//   server: string,
//   pac_url: string,
//   bypass_list: string
// }
// See proxy_config_dictionary.cc for the structure of the respective strings.
class PROXY_CONFIG_EXPORT ProxyConfigDictionary {
 public:
  // Takes ownership of |dict| (|dict| will be moved to |dict_|).
  explicit ProxyConfigDictionary(base::Value::Dict dict);
  ProxyConfigDictionary(ProxyConfigDictionary&& other);

  ProxyConfigDictionary(const ProxyConfigDictionary&) = delete;
  ProxyConfigDictionary& operator=(const ProxyConfigDictionary&) = delete;

  ~ProxyConfigDictionary();

  bool GetMode(ProxyPrefs::ProxyMode* out) const;
  bool GetPacUrl(std::string* out) const;
  bool GetPacMandatory(bool* out) const;
  bool GetProxyServer(std::string* out) const;
  bool GetBypassList(std::string* out) const;
  bool HasBypassList() const;

  const base::Value::Dict& GetDictionary() const;

  static base::Value::Dict CreateDirect();
  static base::Value::Dict CreateAutoDetect();
  static base::Value::Dict CreatePacScript(const std::string& pac_url,
                                           bool pac_mandatory);
  static base::Value::Dict CreateFixedServers(const std::string& proxy_server,
                                              const std::string& bypass_list);
  static base::Value::Dict CreateSystem();

  // Encodes the proxy server as "<url-scheme>=<proxy-scheme>://<proxy>".
  // Used to generate the |proxy_server| arg for CreateFixedServers().
  static void EncodeAndAppendProxyServer(const std::string& url_scheme,
                                         const net::ProxyServer& server,
                                         std::string* spec);

 private:
  bool GetString(const char* key, std::string* out) const;

  static base::Value::Dict CreateDictionary(ProxyPrefs::ProxyMode mode,
                                            const std::string& pac_url,
                                            bool pac_mandatory,
                                            const std::string& proxy_server,
                                            const std::string& bypass_list);

  base::Value::Dict dict_;
};

#endif  // COMPONENTS_PROXY_CONFIG_PROXY_CONFIG_DICTIONARY_H_
