// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/dom_distiller/content/browser/test/test_util.h"

#include "base/base_paths.h"
#include "base/files/file_path.h"
#include "base/path_service.h"
#include "base/strings/strcat.h"
#include "build/build_config.h"
#include "components/dom_distiller/core/viewer.h"
#include "content/public/browser/web_contents.h"
#include "content/public/test/browser_test_utils.h"
#include "net/test/embedded_test_server/controllable_http_response.h"
#include "net/test/embedded_test_server/embedded_test_server.h"
#include "ui/base/resource/resource_bundle.h"
#include "ui/base/resource/resource_scale_factor.h"

namespace dom_distiller {
namespace {

using base::FilePath;
using base::PathService;
using base::StrAppend;
using content::JsReplace;
using content::Referrer;
using content::WaitForLoadStop;
using content::WebContents;
using net::test_server::ControllableHttpResponse;
using net::test_server::EmbeddedTestServer;
using viewer::GetArticleTemplateHtml;

// The path of the distilled page URL relative to the EmbeddedTestServer's base
// directory. This file's contents are generated at test runtime; it is not a
// real file in the repository.
const char* kDistilledPagePath = "/distilled_page.html";

void SetUpTestServerWithoutStarting(EmbeddedTestServer* server) {
  FilePath root_dir;
  PathService::Get(base::DIR_SRC_TEST_DATA_ROOT, &root_dir);

  server->ServeFilesFromDirectory(
      root_dir.AppendASCII("components/dom_distiller/core/javascript"));
  server->ServeFilesFromDirectory(
      root_dir.AppendASCII("components/test/data/dom_distiller"));
  server->ServeFilesFromDirectory(
      root_dir.AppendASCII("third_party/node/node_modules/chai"));
  server->ServeFilesFromDirectory(
      root_dir.AppendASCII("third_party/node/node_modules/mocha"));
}

}  // namespace

FakeDistilledPage::FakeDistilledPage(EmbeddedTestServer* server)
    : response_(
          std::make_unique<ControllableHttpResponse>(server,
                                                     kDistilledPagePath)) {
  CHECK(!server->Started());

  // The distilled page HTML does not contain a script element loading this
  // file. On a real distilled page, this file is executed by
  // DomDistillerRequestViewBase::SendCommonJavaScript(); however, this method
  // is impractical to use in testing.
  AppendScriptFile("dom_distiller_viewer.js");

  // Also load test helper scripts.
  AppendScriptFile("mocha.js");
  AppendScriptFile("test_util.js");
}

FakeDistilledPage::~FakeDistilledPage() = default;

void FakeDistilledPage::AppendScriptFile(const std::string& script_file) {
  scripts_.push_back(script_file);
}

void FakeDistilledPage::Load(EmbeddedTestServer* server,
                             WebContents* web_contents) {
  web_contents->GetController().LoadURL(server->GetURL(kDistilledPagePath),
                                        Referrer(), ui::PAGE_TRANSITION_TYPED,
                                        std::string());
  response_->WaitForRequest();
  response_->Send(net::HTTP_OK, "text/html", GetPageHtmlWithScripts());
  response_->Done();
  ASSERT_TRUE(WaitForLoadStop(web_contents));
}

std::string FakeDistilledPage::GetPageHtmlWithScripts() {
  std::string html = GetArticleTemplateHtml(
      mojom::Theme::kLight, mojom::FontFamily::kSansSerif, std::string());
  for (const std::string& file : scripts_) {
    StrAppend(&html, {JsReplace("<script src=$1></script>", file)});
  }
  return html;
}

void SetUpTestServer(EmbeddedTestServer* server) {
  SetUpTestServerWithoutStarting(server);
  ASSERT_TRUE(server->Start());
}

std::unique_ptr<FakeDistilledPage> SetUpTestServerWithDistilledPage(
    EmbeddedTestServer* server) {
  SetUpTestServerWithoutStarting(server);
  auto distilled_page = std::make_unique<FakeDistilledPage>(server);

  // CHECKs for server start instead of ASSERTs because ASSERT/EXPECT macros
  // only work in functions with a return type of void.
  CHECK(server->Start());
  return distilled_page;
}

void AddComponentsResources() {
  FilePath pak_file;
  FilePath pak_dir;
#if BUILDFLAG(IS_ANDROID)
  CHECK(PathService::Get(base::DIR_ANDROID_APP_DATA, &pak_dir));
  pak_dir = pak_dir.Append(FILE_PATH_LITERAL("paks"));
#elif BUILDFLAG(IS_MAC)
  PathService::Get(base::DIR_MODULE, &pak_dir);
#else
  PathService::Get(base::DIR_ASSETS, &pak_dir);
#endif  // BUILDFLAG(IS_ANDROID)
  pak_file =
      pak_dir.Append(FILE_PATH_LITERAL("components_tests_resources.pak"));
  ui::ResourceBundle::GetSharedInstance().AddDataPackFromPath(
      pak_file, ui::kScaleFactorNone);
}

}  // namespace dom_distiller
