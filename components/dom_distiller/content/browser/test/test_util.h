// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_DOM_DISTILLER_CONTENT_BROWSER_TEST_TEST_UTIL_H_
#define COMPONENTS_DOM_DISTILLER_CONTENT_BROWSER_TEST_TEST_UTIL_H_

#include <memory>
#include <string>
#include <vector>

namespace net {
namespace test_server {
class EmbeddedTestServer;
class ControllableHttpResponse;
}  // namespace test_server
}  // namespace net

namespace content {
class WebContents;
}

namespace dom_distiller {

// Wrapper for building and loading a fake distilled page for testing.
//
// This object MUST be constructed prior to starting |server|. This is enforced
// via CHECK.
class FakeDistilledPage {
 public:
  explicit FakeDistilledPage(net::test_server::EmbeddedTestServer* server);
  ~FakeDistilledPage();

  // Adds |script_file| to the list of scripts to load along with the page HTML.
  //
  // Scripts are loaded by appending script elements with src=|script_file| to
  // the end of the HTML template. This function must be called before Load() to
  // have an effect.
  void AppendScriptFile(const std::string& script_file);

  // Generates the distilled page HTML and loads it to |web_contents|.
  void Load(net::test_server::EmbeddedTestServer* server,
            content::WebContents* web_contents);

  // Disallow copy and assign because it's not obvious how these operations
  // should handle |response_|.
  FakeDistilledPage(const FakeDistilledPage&) = delete;
  FakeDistilledPage& operator=(const FakeDistilledPage&) = delete;

 private:
  // Generates the distilled page's HTML and appends script elements for scripts
  // added via AppendScriptFile().
  std::string GetPageHtmlWithScripts();

  std::unique_ptr<net::test_server::ControllableHttpResponse> response_;
  std::vector<const std::string> scripts_;
};

// Starts |server| after initializing it to load files from the following
// directories:
//   * components/test/data/dom_distiller
//   * components/dom_distiller/core/javascript
void SetUpTestServer(net::test_server::EmbeddedTestServer* server);

// Same as SetUpTestServer(), but allows the server to load a distilled page
// generated at runtime without going through the full distillation process.
std::unique_ptr<FakeDistilledPage> SetUpTestServerWithDistilledPage(
    net::test_server::EmbeddedTestServer* server);

// Sets the path to the .pak file to use when loading resources.
void AddComponentsResources();

}  // namespace dom_distiller

#endif  // COMPONENTS_DOM_DISTILLER_CONTENT_BROWSER_TEST_TEST_UTIL_H_
