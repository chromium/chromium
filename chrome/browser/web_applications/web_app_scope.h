// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_WEB_APPLICATIONS_WEB_APP_SCOPE_H_
#define CHROME_BROWSER_WEB_APPLICATIONS_WEB_APP_SCOPE_H_

#include "base/containers/flat_set.h"
#include "base/types/pass_key.h"
#include "chrome/browser/web_applications/scope_extension_info.h"
#include "components/webapps/common/web_app_id.h"
#include "url/gurl.h"

namespace web_app {
class WebApp;

struct WebAppScopeOptions {
  // Whether to allow an http app scope to match an https url.
  bool allow_http_to_https_upgrade = false;
};

struct WebAppScopeScoreOptions {
  // Scope extensions allow a web app to have other origins be considered part
  // if it's scope. It is often normal to keep this feature, but sometimes it is
  // desirable to ignore scope extensions, so this option allows them to be
  // ignored for score calculations.
  bool exclude_scope_extensions = false;
};

// Represents the 'scope' of a web app / manifest entity. This class contains
// the business logic used to determine if a given URL is in-scope or
// out-of-scope of a web app.
class WebAppScope {
 public:
  WebAppScope(
      const webapps::AppId& app_id,
      const GURL& scope,
      const base::flat_set<ScopeExtensionInfo>& validated_scope_extensions,
      base::PassKey<WebApp>);
  ~WebAppScope();
  WebAppScope(const WebAppScope&);
  WebAppScope(WebAppScope&&);
  WebAppScope& operator=(const WebAppScope&);
  WebAppScope& operator=(WebAppScope&&);

  // Returns if the given `url` is considered 'in-scope' of this web app scope.
  bool IsInScope(const GURL& url,
                 WebAppScopeOptions options = WebAppScopeOptions()) const;

  // If the url is 'in-scope' of this web app scope, then this returns a
  // numerical score allowing callers to rank multiple apps that may have this
  // url in-scope. The higher the score, the more applicable the web app is to
  // the url. If the url is NOT within the scope, this returns 0.
  int GetScopeScore(
      const GURL& url,
      WebAppScopeScoreOptions options = WebAppScopeScoreOptions()) const;

  const GURL& scope() const { return scope_; }

  const base::flat_set<ScopeExtensionInfo>& validated_scope_extensions() const {
    return validated_scope_extensions_;
  }

  bool operator==(const WebAppScope& other) const;

 private:
  webapps::AppId app_id_;
  GURL scope_;
  base::flat_set<ScopeExtensionInfo> validated_scope_extensions_;
};

}  // namespace web_app

#endif  // CHROME_BROWSER_WEB_APPLICATIONS_WEB_APP_SCOPE_H_
