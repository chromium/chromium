// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/test/chromedriver/chrome/devtools_endpoint.h"

#include "chrome/test/chromedriver/chrome/devtools_http_client.h"
#include "chrome/test/chromedriver/net/net_util.h"

DevToolsEndpoint::DevToolsEndpoint(int port)
    : DevToolsEndpoint(NetAddress(port)) {}

DevToolsEndpoint::DevToolsEndpoint(const NetAddress& address)
    : server_url_(std::string("http://") + address.ToString()) {}

DevToolsEndpoint::DevToolsEndpoint(const std::string& url) : server_url_(url) {}

bool DevToolsEndpoint::IsValid() const {
  return server_url_.is_valid();
}

NetAddress DevToolsEndpoint::Address() const {
  return NetAddress(server_url_.host(), server_url_.EffectiveIntPort());
}

std::string DevToolsEndpoint::GetBrowserDebuggerUrl() const {
  GURL::Replacements replacements;
  replacements.SetSchemeStr(server_url_.SchemeIs("https") ? "wss" : "ws");

  return server_url_.Resolve("devtools/browser/")
      .ReplaceComponents(replacements)
      .spec();
}

std::string DevToolsEndpoint::GetDebuggerUrl(const std::string& id) const {
  GURL::Replacements replacements;
  replacements.SetSchemeStr(server_url_.SchemeIs("https") ? "wss" : "ws");

  return server_url_.Resolve("devtools/page/" + id)
      .ReplaceComponents(replacements)
      .spec();
}

std::string DevToolsEndpoint::GetVersionUrl() const {
  return server_url_.Resolve("json/version").spec();
}

std::string DevToolsEndpoint::GetListUrl() const {
  return server_url_.Resolve("json/list").spec();
}
