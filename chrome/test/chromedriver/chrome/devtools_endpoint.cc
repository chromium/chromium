// Copyright (c) 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/test/chromedriver/chrome/devtools_endpoint.h"

#include "base/strings/stringprintf.h"
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
  const std::string scheme = server_url_.SchemeIs("https") ? "wss" : "ws";

  url::Replacements<char> replacements;
  replacements.SetScheme(scheme.c_str(), url::Component(0, scheme.length()));

  return server_url_.Resolve("devtools/browser/")
      .ReplaceComponents(replacements)
      .spec();
}

std::string DevToolsEndpoint::GetDebuggerUrl(const std::string& id) const {
  const std::string scheme = server_url_.SchemeIs("https") ? "wss" : "ws";

  url::Replacements<char> replacements;
  replacements.SetScheme(scheme.c_str(), url::Component(0, scheme.length()));

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

std::string DevToolsEndpoint::GetCloseUrl(const std::string& id) const {
  return server_url_.Resolve("json/close/" + id).spec();
}

std::string DevToolsEndpoint::GetActivateUrl(const std::string& id) const {
  return server_url_.Resolve("json/activate/" + id).spec();
}
