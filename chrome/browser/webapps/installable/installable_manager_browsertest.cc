// Copyright 2016 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/webapps/browser/installable/installable_manager.h"

#include <memory>
#include <optional>
#include <string>
#include <tuple>

#include "base/auto_reset.h"
#include "base/command_line.h"
#include "base/functional/bind.h"
#include "base/memory/raw_ptr.h"
#include "base/memory/scoped_refptr.h"
#include "base/run_loop.h"
#include "base/strings/utf_string_conversions.h"
#include "base/task/sequenced_task_runner.h"
#include "base/test/bind.h"
#include "base/test/metrics/histogram_tester.h"
#include "base/test/test_simple_task_runner.h"
#include "build/build_config.h"
#include "build/buildflag.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/test/base/chrome_test_utils.h"
#include "chrome/test/base/platform_browser_test.h"
#include "components/webapps/browser/banners/app_banner_manager.h"
#include "components/webapps/browser/features.h"
#include "components/webapps/browser/installable/installable_data.h"
#include "components/webapps/browser/installable/installable_evaluator.h"
#include "components/webapps/browser/installable/installable_icon_fetcher.h"
#include "components/webapps/browser/installable/installable_logging.h"
#include "components/webapps/browser/installable/installable_metrics.h"
#include "content/public/test/browser_test.h"
#include "content/public/test/browser_test_utils.h"
#include "content/public/test/prerender_test_util.h"
#include "net/test/embedded_test_server/embedded_test_server.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/blink/public/common/features.h"
#include "third_party/blink/public/common/manifest/manifest_util.h"
#include "third_party/blink/public/mojom/manifest/manifest.mojom.h"

#if !BUILDFLAG(IS_ANDROID)
#include "chrome/browser/ui/browser.h"
#include "chrome/test/base/ui_test_utils.h"
#endif

namespace webapps {

namespace {

#if !BUILDFLAG(IS_ANDROID)
const char kInsecureOrigin[] = "http://www.google.com";
const char kOtherInsecureOrigin[] = "http://maps.google.com";
const char kUnsafeSecureOriginFlag[] =
    "unsafely-treat-insecure-origin-as-secure";
#endif

InstallableParams GetManifestParams() {
  InstallableParams params;
  params.check_eligibility = true;
  return params;
}

InstallableParams GetWebAppParams() {
  InstallableParams params = GetManifestParams();
  params.installable_criteria = InstallableCriteria::kValidManifestWithIcons;
  params.valid_primary_icon = true;
  return params;
}

InstallableParams GetPrimaryIconParams() {
  InstallableParams params = GetManifestParams();
  params.valid_primary_icon = true;
  return params;
}

InstallableParams GetPrimaryIconPreferMaskableParams() {
  InstallableParams params = GetManifestParams();
  params.valid_primary_icon = true;
  params.prefer_maskable_icon = true;
  return params;
}

}  // anonymous namespace

// Used only for testing pages where the manifest URL is changed. This class
// will dispatch a RunLoop::QuitClosure when internal state is reset.
class ResetDataInstallableManager : public InstallableManager {
 public:
  explicit ResetDataInstallableManager(content::WebContents* web_contents)
      : InstallableManager(web_contents) {}
  ~ResetDataInstallableManager() override {}

  void SetQuitClosure(base::RepeatingClosure quit_closure) {
    quit_closure_ = quit_closure;
  }

  bool GetOnResetData() { return is_reset_data_; }
  void ClearOnResetData() { is_reset_data_ = false; }

 protected:
  void OnResetData() override {
    is_reset_data_ = true;
    if (quit_closure_)
      quit_closure_.Run();
  }

 private:
  base::RepeatingClosure quit_closure_;
  bool is_reset_data_ = false;
};

class CallbackTester {
 public:
  explicit CallbackTester(
      base::RepeatingClosure quit_closure,
      scoped_refptr<base::SequencedTaskRunner> test_task_runner =
          base::SequencedTaskRunner::GetCurrentDefault())
      : quit_closure_(quit_closure), test_task_runner_(test_task_runner) {}

  void OnDidFinishInstallableCheck(const InstallableData& data) {
    errors_ = data.errors;
    manifest_url_ = *data.manifest_url;
    manifest_ = data.manifest->Clone();
    metadata_ = data.web_page_metadata->Clone();
    primary_icon_url_ = *data.primary_icon_url;
    if (data.primary_icon)
      primary_icon_ = std::make_unique<SkBitmap>(*data.primary_icon);
    has_maskable_primary_icon_ = data.has_maskable_primary_icon;
    installable_check_passed_ = data.installable_check_passed;
    screenshots_ = *data.screenshots;
    test_task_runner_->PostTask(FROM_HERE, quit_closure_);
  }

  const std::vector<InstallableStatusCode>& errors() const { return errors_; }
  const GURL& manifest_url() const { return manifest_url_; }
  const blink::mojom::Manifest& manifest() const {
    DCHECK(manifest_);
    return *manifest_;
  }
  const mojom::WebPageMetadata& metadata() const { return *metadata_; }
  const GURL& primary_icon_url() const { return primary_icon_url_; }
  const SkBitmap* primary_icon() const { return primary_icon_.get(); }
  bool has_maskable_primary_icon() const { return has_maskable_primary_icon_; }
  const std::vector<Screenshot>& screenshots() const { return screenshots_; }
  bool installable_check_passed() const { return installable_check_passed_; }

 private:
  std::vector<InstallableStatusCode> errors_;
  GURL manifest_url_;
  blink::mojom::ManifestPtr manifest_ = blink::mojom::Manifest::New();
  mojom::WebPageMetadataPtr metadata_ = mojom::WebPageMetadata::New();
  GURL primary_icon_url_;
  std::unique_ptr<SkBitmap> primary_icon_;
  bool has_maskable_primary_icon_;
  std::vector<Screenshot> screenshots_;
  bool installable_check_passed_;
  base::RepeatingClosure quit_closure_;
  scoped_refptr<base::SequencedTaskRunner> test_task_runner_;
};

class NestedCallbackTester {
 public:
  NestedCallbackTester(InstallableManager* manager,
                       const InstallableParams& params,
                       base::OnceClosure quit_closure)
      : manager_(manager),
        params_(params),
        quit_closure_(std::move(quit_closure)) {}

  void Run() {
    manager_->GetData(
        params_, base::BindOnce(&NestedCallbackTester::OnDidFinishFirstCheck,
                                base::Unretained(this)));
  }

  void OnDidFinishFirstCheck(const InstallableData& data) {
    errors_ = data.errors;
    manifest_url_ = *data.manifest_url;
    manifest_ = data.manifest->Clone();
    primary_icon_url_ = *data.primary_icon_url;
    if (data.primary_icon)
      primary_icon_ = std::make_unique<SkBitmap>(*data.primary_icon);
    installable_check_passed_ = data.installable_check_passed;

    manager_->GetData(
        params_, base::BindOnce(&NestedCallbackTester::OnDidFinishSecondCheck,
                                base::Unretained(this)));
  }

  void OnDidFinishSecondCheck(const InstallableData& data) {
    EXPECT_EQ(errors_, data.errors);
    EXPECT_EQ(manifest_url_, *data.manifest_url);
    EXPECT_EQ(primary_icon_url_, *data.primary_icon_url);
    EXPECT_EQ(primary_icon_.get(), data.primary_icon);
    EXPECT_EQ(installable_check_passed_, data.installable_check_passed);
    EXPECT_EQ(blink::IsEmptyManifest(*manifest_),
              blink::IsEmptyManifest(*data.manifest));
    EXPECT_EQ(manifest_->start_url, data.manifest->start_url);
    EXPECT_EQ(manifest_->display, data.manifest->display);
    EXPECT_EQ(manifest_->name, data.manifest->name);
    EXPECT_EQ(manifest_->short_name, data.manifest->short_name);
    EXPECT_EQ(manifest_->display_override, data.manifest->display_override);

    std::move(quit_closure_).Run();
  }

 private:
  raw_ptr<InstallableManager> manager_;
  InstallableParams params_;
  base::OnceClosure quit_closure_;
  std::vector<InstallableStatusCode> errors_;
  GURL manifest_url_;
  blink::mojom::ManifestPtr manifest_;
  GURL primary_icon_url_;
  std::unique_ptr<SkBitmap> primary_icon_;
  bool installable_check_passed_;
};

class InstallableManagerBrowserTest : public PlatformBrowserTest {
 public:
  InstallableManagerBrowserTest()
      : disable_banner_trigger_(&test::g_disable_banner_triggering_for_testing,
                                true),
        scoped_min_favicon_size_(&test::g_minimum_favicon_size_for_testing,
                                 32) {
  }

  void SetUpOnMainThread() override {
    embedded_test_server()->ServeFilesFromSourceDirectory(
        "chrome/test/data/banners");
    ASSERT_TRUE(embedded_test_server()->Start());
  }

  // Returns a test server URL to a page controlled by a |manifest_url| injected
  // as the manifest tag.
  std::string GetURLOfPageWithManifest(const std::string& manifest_url) {
    return "/banners/manifest_test_page.html?manifest=" +
           embedded_test_server()->GetURL(manifest_url).spec();
  }

  std::string GetUrlOfPageWithTags(
      const std::string& base_page,
      const std::map<std::string, std::string> tags) {
    std::string test_url = base_page + "?";
    for (const auto& [key, value] : tags) {
      test_url.append("&" + key + "=" + value);
    }
    return test_url;
  }

  std::string GetUrlOfPageWithManifestAndTags(
      const std::string& manifest_url,
      std::map<std::string, std::string> tags) {
    std::string test_url = "/banners/manifest_test_page.html";
    tags["manifest"] = manifest_url;
    return GetUrlOfPageWithTags(test_url, tags);
  }

  void NavigateToPath(const std::string& path) {
    GURL test_url = embedded_test_server()->GetURL(path);
    ASSERT_TRUE(content::NavigateToURL(web_contents(), test_url));
    content::RunAllTasksUntilIdle();
  }

  void NavigateAndRunInstallableManager(CallbackTester* tester,
                                        const InstallableParams& params,
                                        const std::string& url) {
    NavigateToPath(url);
    RunInstallableManager(tester, params);
  }

  std::vector<content::InstallabilityError>
  NavigateAndGetAllInstallabilityErrors(const std::string& url) {
    NavigateToPath(url);
    return GetAllInstallabilityErrors();
  }

  void RunInstallableManager(CallbackTester* tester,
                             const InstallableParams& params) {
    InstallableManager* manager = GetManager();
    manager->GetData(
        params, base::BindOnce(&CallbackTester::OnDidFinishInstallableCheck,
                               base::Unretained(tester)));
  }

  InstallableManager* GetManager() {
    InstallableManager::CreateForWebContents(web_contents());
    InstallableManager* manager =
        InstallableManager::FromWebContents(web_contents());
    CHECK(manager);

    return manager;
  }

  std::vector<content::InstallabilityError> GetAllInstallabilityErrors() {
    InstallableManager* manager = GetManager();

    base::RunLoop run_loop;
    std::vector<content::InstallabilityError> result;

    manager->GetAllErrors(base::BindLambdaForTesting(
        [&](std::vector<content::InstallabilityError> installability_errors) {
          result = std::move(installability_errors);
          run_loop.Quit();
        }));
    run_loop.Run();
    return result;
  }

  content::WebContents* web_contents() {
    return chrome_test_utils::GetActiveWebContents(this);
  }

 private:
  // Disable the banners in the browser so it won't interfere with the test.
  base::AutoReset<bool> disable_banner_trigger_;
  // Set a min favicon size for testing.
  base::AutoReset<int> scoped_min_favicon_size_;
};

IN_PROC_BROWSER_TEST_F(InstallableManagerBrowserTest,
                       ManagerBeginsInEmptyState) {
  // Ensure that the InstallableManager starts off with everything null.
  InstallableManager* manager = GetManager();

  EXPECT_TRUE(blink::IsEmptyManifest(manager->manifest()));
  EXPECT_TRUE(manager->manifest_url().is_empty());
  EXPECT_FALSE(manager->page_data_->primary_icon_fetched());

  EXPECT_EQ(InstallableStatusCode::NO_ERROR_DETECTED,
            manager->manifest_error());
  EXPECT_TRUE(!manager->task_queue_.HasCurrent());
}

// Skip ManagerInIncognito on Android. Android launches incognito differently.
#if !BUILDFLAG(IS_ANDROID)
IN_PROC_BROWSER_TEST_F(InstallableManagerBrowserTest, ManagerInIncognito) {
  // Ensure that the InstallableManager returns an error if called in an
  // incognito profile.
  Browser* incognito_browser =
      OpenURLOffTheRecord(browser()->profile(), GURL("about:blank"));
  content::WebContents* web_contents =
      incognito_browser->tab_strip_model()->GetActiveWebContents();
  auto manager = std::make_unique<InstallableManager>(web_contents);

  base::RunLoop run_loop;
  std::unique_ptr<CallbackTester> tester(
      new CallbackTester(run_loop.QuitClosure()));

  // NavigateToPath
  GURL test_url =
      embedded_test_server()->GetURL("/banners/manifest_test_page.html");
  ASSERT_TRUE(ui_test_utils::NavigateToURL(incognito_browser, test_url));

  manager->GetData(GetManifestParams(),
                   base::BindOnce(&CallbackTester::OnDidFinishInstallableCheck,
                                  base::Unretained(tester.get())));
  run_loop.Run();

  EXPECT_TRUE(blink::IsEmptyManifest(manager->manifest()));
  EXPECT_TRUE(manager->manifest_url().is_empty());
  EXPECT_FALSE(manager->page_data_->primary_icon_fetched());

  EXPECT_EQ(
      std::vector<InstallableStatusCode>{InstallableStatusCode::IN_INCOGNITO},
      tester->errors());
  EXPECT_EQ(InstallableStatusCode::NO_ERROR_DETECTED,
            manager->manifest_error());
  EXPECT_TRUE(!manager->task_queue_.HasCurrent());
}
#endif

IN_PROC_BROWSER_TEST_F(InstallableManagerBrowserTest, CheckNoManifest) {
  // Ensure that a page with no manifest returns the appropriate error and with
  // null fields for everything.
  base::HistogramTester histograms;
  base::RunLoop run_loop;
  std::unique_ptr<CallbackTester> tester(
      new CallbackTester(run_loop.QuitClosure()));

  // Navigating resets histogram state, so do it before recording a histogram.
  GURL url =
      embedded_test_server()->GetURL("/banners/no_manifest_test_page.html");
  ASSERT_TRUE(content::NavigateToURL(web_contents(), url));
  RunInstallableManager(tester.get(), GetManifestParams());
  run_loop.Run();

  // If there is no manifest, it should be the default one.
  EXPECT_FALSE(blink::IsEmptyManifest(tester->manifest()));
  EXPECT_TRUE(blink::IsDefaultManifest(tester->manifest(), url));
  EXPECT_TRUE(tester->manifest_url().is_empty());
  EXPECT_TRUE(tester->primary_icon_url().is_empty());
  EXPECT_EQ(nullptr, tester->primary_icon());
  EXPECT_FALSE(tester->has_maskable_primary_icon());
  EXPECT_FALSE(tester->installable_check_passed());
  EXPECT_THAT(tester->errors(), testing::IsEmpty());
}

IN_PROC_BROWSER_TEST_F(InstallableManagerBrowserTest, CheckManifest404) {
  base::RunLoop run_loop;
  std::unique_ptr<CallbackTester> tester(
      new CallbackTester(run_loop.QuitClosure()));

  std::string path = GetURLOfPageWithManifest("/banners/manifest_missing.json");
  NavigateAndRunInstallableManager(tester.get(), GetManifestParams(), path);
  run_loop.Run();

  // The installable manager should return a manifest URL even if it 404s.
  // Additionally a default manifest should be returned.
  EXPECT_FALSE(blink::IsEmptyManifest(tester->manifest()));
  EXPECT_TRUE(blink::IsDefaultManifest(tester->manifest(),
                                       embedded_test_server()->GetURL(path)));
  EXPECT_FALSE(tester->manifest_url().is_empty());
  EXPECT_TRUE(tester->primary_icon_url().is_empty());
  EXPECT_EQ(nullptr, tester->primary_icon());
  EXPECT_FALSE(tester->has_maskable_primary_icon());
  EXPECT_FALSE(tester->installable_check_passed());
  EXPECT_EQ(
      std::vector<InstallableStatusCode>{
          InstallableStatusCode::MANIFEST_PARSING_OR_NETWORK_ERROR},
      tester->errors());
}

IN_PROC_BROWSER_TEST_F(InstallableManagerBrowserTest, CheckManifestOnly) {
  // Verify that asking for just the manifest works as expected.
  base::RunLoop run_loop;
  std::unique_ptr<CallbackTester> tester(
      new CallbackTester(run_loop.QuitClosure()));

  NavigateAndRunInstallableManager(tester.get(), GetManifestParams(),
                                   "/banners/manifest_test_page.html");
  run_loop.Run();

  EXPECT_FALSE(blink::IsEmptyManifest(tester->manifest()));
  EXPECT_FALSE(tester->manifest_url().is_empty());

  EXPECT_TRUE(tester->primary_icon_url().is_empty());
  EXPECT_EQ(nullptr, tester->primary_icon());
  EXPECT_FALSE(tester->has_maskable_primary_icon());
  EXPECT_FALSE(tester->installable_check_passed());
  EXPECT_EQ(std::vector<InstallableStatusCode>{}, tester->errors());
}

IN_PROC_BROWSER_TEST_F(InstallableManagerBrowserTest,
                       CheckInstallableParamsDefaultConstructor) {
  // Verify that using InstallableParams' default constructor is equivalent to
  // just asking for the manifest alone.
  base::RunLoop run_loop;
  std::unique_ptr<CallbackTester> tester(
      new CallbackTester(run_loop.QuitClosure()));

  InstallableParams params;
  NavigateAndRunInstallableManager(tester.get(), params,
                                   "/banners/manifest_test_page.html");
  run_loop.Run();

  EXPECT_FALSE(blink::IsEmptyManifest(tester->manifest()));
  EXPECT_FALSE(tester->manifest_url().is_empty());

  EXPECT_TRUE(tester->primary_icon_url().is_empty());
  EXPECT_EQ(nullptr, tester->primary_icon());
  EXPECT_FALSE(tester->has_maskable_primary_icon());
  EXPECT_FALSE(tester->installable_check_passed());
  EXPECT_EQ(std::vector<InstallableStatusCode>{}, tester->errors());
}

IN_PROC_BROWSER_TEST_F(InstallableManagerBrowserTest, FetchWebPageMetaData) {
  InstallableParams params;
  params.check_eligibility = true;
  params.fetch_metadata = true;

  const std::map<std::string, std::string> meta_tags = {
      {"application-name", "Test App Name"}, {"description", "description"}};

  // Test fetch web page metadata.
  {
    base::RunLoop run_loop;
    std::unique_ptr<CallbackTester> tester(
        new CallbackTester(run_loop.QuitClosure()));

    NavigateAndRunInstallableManager(
        tester.get(), params,
        GetUrlOfPageWithTags("/banners/manifest_test_page.html", meta_tags));

    run_loop.Run();

    EXPECT_FALSE(blink::IsEmptyManifest(tester->manifest()));
    EXPECT_FALSE(tester->manifest_url().is_empty());

    EXPECT_EQ(u"Test App Name", tester->metadata().application_name);
    EXPECT_EQ(u"Web app banner test page", tester->metadata().title);
    EXPECT_EQ(u"description", tester->metadata().description);

    EXPECT_TRUE(tester->primary_icon_url().is_empty());
    EXPECT_EQ(nullptr, tester->primary_icon());
    EXPECT_EQ(std::vector<InstallableStatusCode>{}, tester->errors());
  }

  // Test fetch web page metadata when no manifest.
  {
    base::RunLoop run_loop;
    std::unique_ptr<CallbackTester> tester(
        new CallbackTester(run_loop.QuitClosure()));

    std::string path =
        GetUrlOfPageWithTags("/banners/no_manifest_test_page.html", meta_tags);
    NavigateAndRunInstallableManager(tester.get(), params, path);

    run_loop.Run();

    EXPECT_FALSE(tester->metadata().application_name.empty());

    EXPECT_TRUE(blink::IsDefaultManifest(tester->manifest(),
                                         embedded_test_server()->GetURL(path)));
    EXPECT_TRUE(tester->manifest_url().is_empty());
    EXPECT_THAT(tester->errors(), testing::IsEmpty());
  }
}

IN_PROC_BROWSER_TEST_F(InstallableManagerBrowserTest, CheckImplicitAppName) {
  InstallableParams params;
  params.fetch_metadata = true;
  params.installable_criteria =
      InstallableCriteria::kImplicitManifestFieldsHTML;

  base::RunLoop run_loop;
  std::unique_ptr<CallbackTester> tester(
      new CallbackTester(run_loop.QuitClosure()));

  std::map<std::string, std::string> meta_tags = {
      {"application-name", "Test App Name"}};
  NavigateAndRunInstallableManager(
      tester.get(), params,
      GetUrlOfPageWithManifestAndTags("/banners/manifest_empty_name.json",
                                      meta_tags));

  run_loop.Run();

  EXPECT_FALSE(blink::IsEmptyManifest(tester->manifest()));
  EXPECT_FALSE(tester->manifest_url().is_empty());
  EXPECT_EQ(std::u16string(),
            tester->manifest().name.value_or(std::u16string()));
  EXPECT_EQ(u"Test App Name", tester->metadata().application_name);
  EXPECT_TRUE(tester->installable_check_passed());
  EXPECT_EQ(std::vector<InstallableStatusCode>{}, tester->errors());
}

IN_PROC_BROWSER_TEST_F(InstallableManagerBrowserTest, CheckImplicitIcons) {
  InstallableParams params;
  params.fetch_metadata = true;
  params.installable_criteria =
      InstallableCriteria::kImplicitManifestFieldsHTML;

  base::RunLoop run_loop;
  std::unique_ptr<CallbackTester> tester(
      new CallbackTester(run_loop.QuitClosure()));

  std::map<std::string, std::string> meta_tags = {
      {"application-name", "Test App Name"},
      {"icon", "/banners/256x256-red.png"}};
  NavigateAndRunInstallableManager(
      tester.get(), params,
      GetUrlOfPageWithManifestAndTags("/banners/manifest_no_icon.json",
                                      meta_tags));

  run_loop.Run();

  EXPECT_FALSE(blink::IsEmptyManifest(tester->manifest()));
  EXPECT_FALSE(tester->manifest_url().is_empty());
  EXPECT_EQ(u"Manifest test app", tester->manifest().name);
  EXPECT_EQ(u"Test App Name", tester->metadata().application_name);
  EXPECT_TRUE(tester->manifest().icons.empty());
  EXPECT_FALSE(web_contents()->GetFaviconURLs().empty());
  EXPECT_TRUE(tester->installable_check_passed());
  EXPECT_EQ(std::vector<InstallableStatusCode>{}, tester->errors());
}

IN_PROC_BROWSER_TEST_F(InstallableManagerBrowserTest,
                       CheckManifestWithIconThatIsTooSmall) {
  // This page has a manifest with only a 48x48 icon which is too small to be
  // installable. Asking for a primary icon should fail with NO_ACCEPTABLE_ICON.
  {
    base::RunLoop run_loop;
    std::unique_ptr<CallbackTester> tester(
        new CallbackTester(run_loop.QuitClosure()));

    NavigateAndRunInstallableManager(
        tester.get(), GetPrimaryIconParams(),
        GetURLOfPageWithManifest("/banners/manifest_too_small_icon.json"));
    run_loop.Run();

    EXPECT_FALSE(blink::IsEmptyManifest(tester->manifest()));
    EXPECT_FALSE(tester->manifest_url().is_empty());

    EXPECT_TRUE(tester->primary_icon_url().is_empty());
    EXPECT_EQ(nullptr, tester->primary_icon());
    EXPECT_FALSE(tester->installable_check_passed());
    EXPECT_EQ(
        std::vector<InstallableStatusCode>{
            InstallableStatusCode::NO_ACCEPTABLE_ICON},
        tester->errors());
  }

  // Ask for everything. This should fail with NO_ACCEPTABLE_ICON - the primary
  // icon fetch has already failed, so that cached error stops the installable
  // check from being performed.
  {
    base::RunLoop run_loop;
    std::unique_ptr<CallbackTester> tester(
        new CallbackTester(run_loop.QuitClosure()));

    RunInstallableManager(tester.get(), GetPrimaryIconParams());
    run_loop.Run();

    EXPECT_FALSE(blink::IsEmptyManifest(tester->manifest()));
    EXPECT_FALSE(tester->manifest_url().is_empty());

    EXPECT_TRUE(tester->primary_icon_url().is_empty());
    EXPECT_EQ(nullptr, tester->primary_icon());
    EXPECT_FALSE(tester->installable_check_passed());
    EXPECT_EQ(
        std::vector<InstallableStatusCode>{
            InstallableStatusCode::NO_ACCEPTABLE_ICON},
        tester->errors());
  }
}

IN_PROC_BROWSER_TEST_F(InstallableManagerBrowserTest,
                       CheckManifestWithOnlyRelatedApplications) {
  // This page has a manifest with only related applications specified. Asking
  // for just the manifest should succeed.
  {
    base::RunLoop run_loop;
    std::unique_ptr<CallbackTester> tester(
        new CallbackTester(run_loop.QuitClosure()));

    NavigateAndRunInstallableManager(
        tester.get(), GetManifestParams(),
        GetURLOfPageWithManifest("/banners/play_app_manifest.json"));
    run_loop.Run();

    EXPECT_FALSE(blink::IsEmptyManifest(tester->manifest()));
    EXPECT_FALSE(tester->manifest_url().is_empty());
    EXPECT_TRUE(tester->manifest().prefer_related_applications);

    EXPECT_TRUE(tester->primary_icon_url().is_empty());
    EXPECT_EQ(nullptr, tester->primary_icon());
    EXPECT_FALSE(tester->installable_check_passed());
    EXPECT_EQ(std::vector<InstallableStatusCode>{}, tester->errors());
  }

  // Ask for a primary icon (but don't navigate). This should fail with
  // NO_ACCEPTABLE_ICON.
  {
    base::RunLoop run_loop;
    std::unique_ptr<CallbackTester> tester(
        new CallbackTester(run_loop.QuitClosure()));

    RunInstallableManager(tester.get(), GetPrimaryIconParams());
    run_loop.Run();

    EXPECT_FALSE(blink::IsEmptyManifest(tester->manifest()));
    EXPECT_FALSE(tester->manifest_url().is_empty());
    EXPECT_TRUE(tester->manifest().prefer_related_applications);

    EXPECT_TRUE(tester->primary_icon_url().is_empty());
    EXPECT_EQ(nullptr, tester->primary_icon());
    EXPECT_FALSE(tester->installable_check_passed());
    EXPECT_EQ(
        std::vector<InstallableStatusCode>{
            InstallableStatusCode::NO_ACCEPTABLE_ICON},
        tester->errors());
  }

  // Ask for everything. This should fail with NO_ACCEPTABLE_ICON - the primary
  // icon fetch has already failed, so that cached error stops the installable
  // check from being performed.
  {
    base::RunLoop run_loop;
    std::unique_ptr<CallbackTester> tester(
        new CallbackTester(run_loop.QuitClosure()));
    auto params = GetWebAppParams();
    params.installable_criteria = InstallableCriteria::kDoNotCheck;
    RunInstallableManager(tester.get(), params);
    run_loop.Run();

    EXPECT_FALSE(blink::IsEmptyManifest(tester->manifest()));
    EXPECT_FALSE(tester->manifest_url().is_empty());
    EXPECT_TRUE(tester->manifest().prefer_related_applications);

    EXPECT_TRUE(tester->primary_icon_url().is_empty());
    EXPECT_EQ(nullptr, tester->primary_icon());
    EXPECT_FALSE(tester->installable_check_passed());
    EXPECT_EQ(
        std::vector<InstallableStatusCode>{
            InstallableStatusCode::NO_ACCEPTABLE_ICON},
        tester->errors());
  }

  // Do not ask for primary icon. This should fail with several validity
  // errors.
  {
    base::RunLoop run_loop;
    std::unique_ptr<CallbackTester> tester(
        new CallbackTester(run_loop.QuitClosure()));

    InstallableParams params = GetWebAppParams();
    params.valid_primary_icon = false;
    RunInstallableManager(tester.get(), params);
    run_loop.Run();

    EXPECT_FALSE(blink::IsEmptyManifest(tester->manifest()));
    EXPECT_FALSE(tester->manifest_url().is_empty());
    EXPECT_TRUE(tester->manifest().prefer_related_applications);

    EXPECT_TRUE(tester->primary_icon_url().is_empty());
    EXPECT_EQ(nullptr, tester->primary_icon());
    EXPECT_FALSE(tester->installable_check_passed());
    EXPECT_EQ(std::vector<InstallableStatusCode>(
                  {InstallableStatusCode::START_URL_NOT_VALID,
                   InstallableStatusCode::MANIFEST_MISSING_NAME_OR_SHORT_NAME,
                   InstallableStatusCode::MANIFEST_DISPLAY_NOT_SUPPORTED,
                   InstallableStatusCode::MANIFEST_MISSING_SUITABLE_ICON}),
              tester->errors());
  }
}

IN_PROC_BROWSER_TEST_F(InstallableManagerBrowserTest, CheckManifestAndIcon) {
  // Add to homescreen checks for manifest + primary icon.
  {
    base::RunLoop run_loop;
    std::unique_ptr<CallbackTester> tester(
        new CallbackTester(run_loop.QuitClosure()));

    NavigateAndRunInstallableManager(tester.get(), GetPrimaryIconParams(),
                                     "/banners/manifest_test_page.html");
    run_loop.Run();

    EXPECT_FALSE(blink::IsEmptyManifest(tester->manifest()));
    EXPECT_FALSE(tester->manifest_url().is_empty());

    EXPECT_FALSE(tester->primary_icon_url().is_empty());
    EXPECT_NE(nullptr, tester->primary_icon());
    EXPECT_FALSE(tester->installable_check_passed());
    EXPECT_EQ(std::vector<InstallableStatusCode>{}, tester->errors());
  }

  // Navigate to a page with a good maskable icon and a bad any
  // icon. The maskable icon is fetched for primary icon.
  {
    base::RunLoop run_loop;
    std::unique_ptr<CallbackTester> tester(
        new CallbackTester(run_loop.QuitClosure()));
    NavigateAndRunInstallableManager(
        tester.get(), GetPrimaryIconPreferMaskableParams(),
        GetURLOfPageWithManifest(
            "/banners/manifest_bad_non_maskable_icon.json"));
    run_loop.Run();
    EXPECT_FALSE(blink::IsEmptyManifest(tester->manifest()));
    EXPECT_FALSE(tester->manifest_url().is_empty());
    EXPECT_FALSE(tester->primary_icon_url().is_empty());
    EXPECT_NE(nullptr, tester->primary_icon());
    EXPECT_TRUE(tester->has_maskable_primary_icon());

    EXPECT_FALSE(tester->installable_check_passed());
    EXPECT_EQ(std::vector<InstallableStatusCode>{}, tester->errors());
  }
}

IN_PROC_BROWSER_TEST_F(InstallableManagerBrowserTest, CheckWebapp) {
  // Request everything.
  {
    base::HistogramTester histograms;
    base::RunLoop run_loop;
    std::unique_ptr<CallbackTester> tester(
        new CallbackTester(run_loop.QuitClosure()));

    // Navigating resets histogram state, so do it before recording a histogram.
    NavigateToPath("/banners/manifest_test_page.html");
    RunInstallableManager(tester.get(), GetWebAppParams());
    run_loop.Run();

    EXPECT_FALSE(blink::IsEmptyManifest(tester->manifest()));
    EXPECT_FALSE(tester->manifest_url().is_empty());
    EXPECT_FALSE(tester->primary_icon_url().is_empty());
    EXPECT_NE(nullptr, tester->primary_icon());
    EXPECT_TRUE(tester->installable_check_passed());
    EXPECT_EQ(std::vector<InstallableStatusCode>{}, tester->errors());

    // Verify that the returned state matches manager internal state.
    InstallableManager* manager = GetManager();

    EXPECT_FALSE(blink::IsEmptyManifest(manager->manifest()));
    EXPECT_FALSE(manager->manifest_url().is_empty());
    EXPECT_TRUE(manager->page_data_->primary_icon_fetched());
    EXPECT_FALSE((manager->icon_url().is_empty()));
    EXPECT_NE(nullptr, (manager->icon()));
    EXPECT_EQ(InstallableStatusCode::NO_ERROR_DETECTED,
              manager->manifest_error());
    EXPECT_EQ(InstallableStatusCode::NO_ERROR_DETECTED,
              (manager->icon_error()));
    EXPECT_TRUE(!manager->task_queue_.HasCurrent());
  }

  // Request everything again without navigating away. This should work fine.
  {
    base::RunLoop run_loop;
    std::unique_ptr<CallbackTester> tester(
        new CallbackTester(run_loop.QuitClosure()));
    auto params = GetWebAppParams();
    // Make sure check_installability check is run.
    params.is_debug_mode = true;
    RunInstallableManager(tester.get(), params);
    run_loop.Run();

    EXPECT_FALSE(blink::IsEmptyManifest(tester->manifest()));
    EXPECT_FALSE(tester->manifest_url().is_empty());
    EXPECT_FALSE(tester->primary_icon_url().is_empty());
    EXPECT_NE(nullptr, tester->primary_icon());
    EXPECT_TRUE(tester->installable_check_passed());
    EXPECT_EQ(std::vector<InstallableStatusCode>{}, tester->errors());

    // Verify that the returned state matches manager internal state.
    InstallableManager* manager = GetManager();

    EXPECT_FALSE(blink::IsEmptyManifest(manager->manifest()));
    EXPECT_FALSE(manager->manifest_url().is_empty());
    EXPECT_TRUE(manager->page_data_->primary_icon_fetched());
    EXPECT_FALSE((manager->icon_url().is_empty()));
    EXPECT_NE(nullptr, (manager->icon()));
    EXPECT_EQ(InstallableStatusCode::NO_ERROR_DETECTED,
              manager->manifest_error());
    EXPECT_EQ(InstallableStatusCode::NO_ERROR_DETECTED,
              (manager->icon_error()));
    EXPECT_TRUE(!manager->task_queue_.HasCurrent());
  }

  {
    // Check that a subsequent navigation resets state.
    ASSERT_TRUE(content::NavigateToURL(web_contents(), GURL("about:blank")));
    InstallableManager* manager = GetManager();

    EXPECT_TRUE(blink::IsEmptyManifest(manager->manifest()));
    EXPECT_TRUE(manager->manifest_url().is_empty());
    EXPECT_FALSE(manager->page_data_->primary_icon_fetched());
    EXPECT_EQ(InstallableStatusCode::NO_ERROR_DETECTED,
              manager->manifest_error());
    EXPECT_TRUE(!manager->task_queue_.HasCurrent());
  }
}

IN_PROC_BROWSER_TEST_F(InstallableManagerBrowserTest, CheckMaskableIcon) {
  // Checks that InstallableManager chooses the correct primary icon when the
  // manifest contains maskable icons.

  // Checks that if a MASKABLE icon is specified, it is used as primary icon.
  {
    base::RunLoop run_loop;
    std::unique_ptr<CallbackTester> tester(
        new CallbackTester(run_loop.QuitClosure()));

    NavigateAndRunInstallableManager(
        tester.get(), GetPrimaryIconPreferMaskableParams(),
        GetURLOfPageWithManifest("/banners/manifest_maskable.json"));

    run_loop.Run();

    EXPECT_FALSE(blink::IsEmptyManifest(tester->manifest()));
    EXPECT_FALSE(tester->manifest_url().is_empty());

    EXPECT_FALSE(tester->primary_icon_url().is_empty());
    EXPECT_NE(nullptr, tester->primary_icon());
    EXPECT_TRUE(tester->has_maskable_primary_icon());

    EXPECT_FALSE(tester->installable_check_passed());

    EXPECT_EQ(std::vector<InstallableStatusCode>{}, tester->errors());
  }

  // Checks that we don't pick a MASKABLE icon if it was not requested.
  {
    base::RunLoop run_loop;
    std::unique_ptr<CallbackTester> tester(
        new CallbackTester(run_loop.QuitClosure()));

    NavigateAndRunInstallableManager(
        tester.get(), GetPrimaryIconParams(),
        GetURLOfPageWithManifest("/banners/manifest_maskable.json"));

    run_loop.Run();

    EXPECT_FALSE(blink::IsEmptyManifest(tester->manifest()));
    EXPECT_FALSE(tester->manifest_url().is_empty());

    EXPECT_FALSE(tester->primary_icon_url().is_empty());
    EXPECT_NE(nullptr, tester->primary_icon());
    EXPECT_FALSE(tester->has_maskable_primary_icon());

    EXPECT_FALSE(tester->installable_check_passed());
    EXPECT_EQ(std::vector<InstallableStatusCode>{}, tester->errors());
  }

  // Checks that we fall back to using an ANY icon if a MASKABLE icon is
  // requested but not in the manifest.
  {
    base::RunLoop run_loop;
    std::unique_ptr<CallbackTester> tester(
        new CallbackTester(run_loop.QuitClosure()));

    NavigateAndRunInstallableManager(tester.get(),
                                     GetPrimaryIconPreferMaskableParams(),
                                     "/banners/manifest_test_page.html");

    run_loop.Run();

    EXPECT_FALSE(blink::IsEmptyManifest(tester->manifest()));
    EXPECT_FALSE(tester->manifest_url().is_empty());

    EXPECT_FALSE(tester->primary_icon_url().is_empty());
    EXPECT_NE(nullptr, tester->primary_icon());
    EXPECT_FALSE(tester->has_maskable_primary_icon());

    EXPECT_FALSE(tester->installable_check_passed());
    EXPECT_EQ(std::vector<InstallableStatusCode>{}, tester->errors());
  }

  // Checks that we fall back to using an ANY icon if a MASKABLE icon is
  // requested but the maskable icon is bad.
  {
    base::RunLoop run_loop;
    std::unique_ptr<CallbackTester> tester(
        new CallbackTester(run_loop.QuitClosure()));

    NavigateAndRunInstallableManager(
        tester.get(), GetPrimaryIconPreferMaskableParams(),
        GetURLOfPageWithManifest("/banners/manifest_bad_maskable.json"));

    run_loop.Run();

    EXPECT_FALSE(blink::IsEmptyManifest(tester->manifest()));
    EXPECT_FALSE(tester->manifest_url().is_empty());

    EXPECT_FALSE(tester->primary_icon_url().is_empty());
    EXPECT_NE(nullptr, tester->primary_icon());
    EXPECT_FALSE(tester->has_maskable_primary_icon());

    EXPECT_FALSE(tester->installable_check_passed());
    EXPECT_EQ(std::vector<InstallableStatusCode>{}, tester->errors());
  }
}

// Flaky on Mac. TODO(crbug.com/333331507): Re-enable once the issue is fixed.
#if BUILDFLAG(IS_MAC)
#define MAYBE_CheckFavicon DISABLED_CheckFavicon
#else
#define MAYBE_CheckFavicon CheckFavicon
#endif
IN_PROC_BROWSER_TEST_F(InstallableManagerBrowserTest, MAYBE_CheckFavicon) {
  // Checks that InstallableManager chooses the correct primary icon when
  // fetching favicon.

  InstallableParams installableParams = GetPrimaryIconPreferMaskableParams();
  installableParams.fetch_favicon = true;

  // Checks that favicon is fetched when no other icon provided.
  {
    base::RunLoop run_loop;
    std::unique_ptr<CallbackTester> tester(
        new CallbackTester(run_loop.QuitClosure()));

    NavigateAndRunInstallableManager(
        tester.get(), installableParams,
        GetUrlOfPageWithManifestAndTags(
            "/banners/manifest_no_icon.json",
            {{"icon", "/banners/256x256-red.png"}}));
    run_loop.Run();

    EXPECT_FALSE(blink::IsEmptyManifest(tester->manifest()));
    EXPECT_FALSE(tester->manifest_url().is_empty());

    EXPECT_EQ(tester->primary_icon_url(),
              embedded_test_server()->GetURL("/banners/256x256-red.png"));
    EXPECT_NE(nullptr, tester->primary_icon());
    EXPECT_FALSE(tester->has_maskable_primary_icon());
  }

  // Checks NOT fetching favicon when there is a manifest icon.
  {
    base::RunLoop run_loop;
    std::unique_ptr<CallbackTester> tester(
        new CallbackTester(run_loop.QuitClosure()));

    NavigateAndRunInstallableManager(
        tester.get(), installableParams,
        GetUrlOfPageWithManifestAndTags(
            "/banners/manifest_one_icon.json",
            {{"icon", "/banners/256x256-red.png"}}));
    run_loop.Run();

    EXPECT_FALSE(blink::IsEmptyManifest(tester->manifest()));
    EXPECT_FALSE(tester->manifest_url().is_empty());

    EXPECT_EQ(tester->primary_icon_url(),
              embedded_test_server()->GetURL("/banners/image-512px.png"));
    EXPECT_NE(nullptr, tester->primary_icon());
    EXPECT_FALSE(tester->has_maskable_primary_icon());
  }

  // Checks that we do not use favicon smaller than min size.
  {
    // Set a large min size so the icon will not be big enough.
    base::AutoReset<int> scoped_min_favicon_size(
        &test::g_minimum_favicon_size_for_testing, 1000);

    base::RunLoop run_loop;
    std::unique_ptr<CallbackTester> tester(
        new CallbackTester(run_loop.QuitClosure()));

    NavigateAndRunInstallableManager(
        tester.get(), installableParams,
        GetUrlOfPageWithManifestAndTags(
            "/banners/manifest_no_icon.json",
            {{"icon", "/banners/256x256-red.png"}}));

    run_loop.Run();

    EXPECT_FALSE(blink::IsEmptyManifest(tester->manifest()));
    EXPECT_FALSE(tester->manifest_url().is_empty());

    EXPECT_TRUE(tester->primary_icon_url().is_empty());
    EXPECT_EQ(nullptr, tester->primary_icon());
    EXPECT_FALSE(tester->has_maskable_primary_icon());
  }
}

IN_PROC_BROWSER_TEST_F(InstallableManagerBrowserTest,
                       CheckNavigationWithoutRunning) {
  {
    // Expect the call to ManifestAndIconTimeout to kick off an installable
    // check and fail it on a not installable page.
    base::HistogramTester histograms;
    ASSERT_TRUE(content::NavigateToURL(
        web_contents(),
        embedded_test_server()->GetURL("/banners/no_manifest_test_page.html")));

    InstallableManager* manager = GetManager();

    base::RunLoop run_loop;
    std::unique_ptr<CallbackTester> tester(
        new CallbackTester(run_loop.QuitClosure()));

    // Set up a GetData call which will not record an installable metric to
    // ensure we wait until the previous check has finished.
    manager->GetData(
        GetManifestParams(),
        base::BindOnce(&CallbackTester::OnDidFinishInstallableCheck,
                       base::Unretained(tester.get())));
    run_loop.Run();

    ASSERT_TRUE(content::NavigateToURL(web_contents(), GURL("about:blank")));
  }

  {
    // Expect the call to ManifestAndIconTimeout to kick off an installable
    // check and pass it on an installable page.
    base::HistogramTester histograms;
    NavigateToPath("/banners/manifest_test_page.html");

    InstallableManager* manager = GetManager();

    base::RunLoop run_loop;
    std::unique_ptr<CallbackTester> tester(
        new CallbackTester(run_loop.QuitClosure()));

    // Set up a GetData call which will not record an installable metric to
    // ensure we wait until the previous check has finished.
    manager->GetData(
        GetManifestParams(),
        base::BindOnce(&CallbackTester::OnDidFinishInstallableCheck,
                       base::Unretained(tester.get())));
    run_loop.Run();

    ASSERT_TRUE(content::NavigateToURL(web_contents(), GURL("about:blank")));
  }
}

IN_PROC_BROWSER_TEST_F(InstallableManagerBrowserTest, CheckWebappInIframe) {
  base::RunLoop run_loop;
  std::unique_ptr<CallbackTester> tester(
      new CallbackTester(run_loop.QuitClosure()));

  NavigateAndRunInstallableManager(tester.get(), GetWebAppParams(),
                                   "/banners/iframe_test_page.html");
  run_loop.Run();

  // The installable manager should only retrieve items in the main frame.
  // The manifest should be the default one for the main frame, and not the
  // one in the iframe inside of iframe_test_page.html (which points to the
  // `manifest_test_page.html`).
  EXPECT_TRUE(blink::IsDefaultManifest(
      tester->manifest(),
      embedded_test_server()->GetURL("/banners/iframe_test_page.html")));
  EXPECT_TRUE(tester->manifest_url().is_empty());
  EXPECT_TRUE(tester->primary_icon_url().is_empty());
  EXPECT_EQ(nullptr, tester->primary_icon());
  EXPECT_FALSE(tester->installable_check_passed());
  EXPECT_EQ(
      std::vector<InstallableStatusCode>{InstallableStatusCode::NO_MANIFEST},
      tester->errors());
}

IN_PROC_BROWSER_TEST_F(InstallableManagerBrowserTest, CheckDataUrlIcon) {
  // Verify that InstallableManager can handle data URL icons.
  base::RunLoop run_loop;
  std::unique_ptr<CallbackTester> tester(
      new CallbackTester(run_loop.QuitClosure()));

  NavigateAndRunInstallableManager(
      tester.get(), GetWebAppParams(),
      GetURLOfPageWithManifest("/banners/manifest_data_url_icon.json"));
  run_loop.Run();

  EXPECT_FALSE(blink::IsEmptyManifest(tester->manifest()));
  EXPECT_FALSE(tester->manifest_url().is_empty());

  EXPECT_FALSE(tester->primary_icon_url().is_empty());
  EXPECT_NE(nullptr, tester->primary_icon());
  EXPECT_GT(tester->primary_icon()->width(), 0);
  EXPECT_TRUE(tester->installable_check_passed());
  EXPECT_EQ(std::vector<InstallableStatusCode>{}, tester->errors());
}

IN_PROC_BROWSER_TEST_F(InstallableManagerBrowserTest,
                       CheckManifestCorruptedIcon) {
  // Verify that the returned InstallableData::primary_icon is null if the web
  // manifest points to a corrupt primary icon.
  base::RunLoop run_loop;
  std::unique_ptr<CallbackTester> tester(
      new CallbackTester(run_loop.QuitClosure()));

  NavigateAndRunInstallableManager(
      tester.get(), GetPrimaryIconParams(),
      GetURLOfPageWithManifest("/banners/manifest_bad_icon.json"));
  run_loop.Run();

  EXPECT_FALSE(blink::IsEmptyManifest(tester->manifest()));
  EXPECT_FALSE(tester->manifest_url().is_empty());
  EXPECT_TRUE(tester->primary_icon_url().is_empty());
  EXPECT_EQ(nullptr, tester->primary_icon());
  EXPECT_FALSE(tester->installable_check_passed());
  EXPECT_EQ(
      std::vector<InstallableStatusCode>{
          InstallableStatusCode::NO_ACCEPTABLE_ICON},
      tester->errors());
}

IN_PROC_BROWSER_TEST_F(InstallableManagerBrowserTest,
                       CheckChangeInIconDimensions) {
  // Verify that a follow-up request for a primary icon with a different size
  // works.
  {
    base::RunLoop run_loop;
    std::unique_ptr<CallbackTester> tester(
        new CallbackTester(run_loop.QuitClosure()));

    NavigateAndRunInstallableManager(tester.get(), GetWebAppParams(),
                                     "/banners/manifest_test_page.html");
    run_loop.Run();

    EXPECT_FALSE(tester->manifest_url().is_empty());
    EXPECT_FALSE(blink::IsEmptyManifest(tester->manifest()));
    EXPECT_FALSE(tester->primary_icon_url().is_empty());
    EXPECT_NE(nullptr, tester->primary_icon());
    EXPECT_TRUE(tester->installable_check_passed());
    EXPECT_EQ(std::vector<InstallableStatusCode>{}, tester->errors());
  }

  {
    base::RunLoop run_loop;
    std::unique_ptr<CallbackTester> tester(
        new CallbackTester(run_loop.QuitClosure()));
    auto params = GetWebAppParams();
    // Make sure installable_check_passed check is run.
    params.is_debug_mode = true;
    RunInstallableManager(tester.get(), params);

    run_loop.Run();

    // The smaller primary icon requirements should allow this to pass.
    EXPECT_FALSE(tester->manifest_url().is_empty());
    EXPECT_FALSE(blink::IsEmptyManifest(tester->manifest()));
    EXPECT_FALSE(tester->primary_icon_url().is_empty());
    EXPECT_NE(nullptr, tester->primary_icon());
    EXPECT_TRUE(tester->installable_check_passed());
    EXPECT_EQ(std::vector<InstallableStatusCode>{}, tester->errors());
  }
}

IN_PROC_BROWSER_TEST_F(InstallableManagerBrowserTest,
                       CheckNestedCallsToGetData) {
  // Verify that we can call GetData while in a callback from GetData.
  base::RunLoop run_loop;
  InstallableParams params = GetWebAppParams();
  std::unique_ptr<NestedCallbackTester> tester(
      new NestedCallbackTester(GetManager(), params, run_loop.QuitClosure()));

  tester->Run();
  run_loop.Run();
}

IN_PROC_BROWSER_TEST_F(InstallableManagerBrowserTest,
                       ManifestUrlChangeFlushesState) {
  auto manager = std::make_unique<ResetDataInstallableManager>(web_contents());

  // Start on a page with no manifest.

  GURL url =
      embedded_test_server()->GetURL("/banners/no_manifest_test_page.html");
  ASSERT_TRUE(content::NavigateToURL(web_contents(), url));

  {
    // Fetch the data. This should return an empty manifest.
    base::RunLoop run_loop;
    std::unique_ptr<CallbackTester> tester(
        new CallbackTester(run_loop.QuitClosure()));

    manager->GetData(
        GetWebAppParams(),
        base::BindOnce(&CallbackTester::OnDidFinishInstallableCheck,
                       base::Unretained(tester.get())));
    run_loop.Run();

    EXPECT_FALSE(blink::IsEmptyManifest(tester->manifest()));
    EXPECT_TRUE(blink::IsDefaultManifest(tester->manifest(), url));
    EXPECT_EQ(InstallableStatusCode::NO_ERROR_DETECTED,
              manager->manifest_error());
    EXPECT_THAT(tester->errors(),
                testing::ElementsAre(InstallableStatusCode::NO_MANIFEST));
  }

  {
    // Injecting a manifest URL but not navigating should flush the state.
    base::RunLoop run_loop;
    manager->SetQuitClosure(run_loop.QuitClosure());
    EXPECT_TRUE(content::ExecJs(web_contents(), "addManifestLinkTag()"));
    run_loop.Run();

    EXPECT_TRUE(blink::IsEmptyManifest(manager->manifest()));
    EXPECT_EQ(InstallableStatusCode::NO_ERROR_DETECTED,
              manager->manifest_error());
  }

  {
    // Fetch the data again. This should succeed.
    base::RunLoop run_loop;
    std::unique_ptr<CallbackTester> tester(
        new CallbackTester(run_loop.QuitClosure()));

    manager->GetData(
        GetWebAppParams(),
        base::BindOnce(&CallbackTester::OnDidFinishInstallableCheck,
                       base::Unretained(tester.get())));
    run_loop.Run();

    EXPECT_FALSE(blink::IsEmptyManifest(tester->manifest()));
    EXPECT_EQ(std::vector<InstallableStatusCode>{}, tester->errors());
    EXPECT_EQ(u"Manifest test app", tester->manifest().name);
    EXPECT_EQ(std::u16string(),
              tester->manifest().short_name.value_or(std::u16string()));
  }

  {
    // Flush the state again by changing the manifest URL.
    base::RunLoop run_loop;
    manager->SetQuitClosure(run_loop.QuitClosure());

    GURL manifest_url = embedded_test_server()->GetURL(
        "/banners/manifest_short_name_only.json");
    EXPECT_TRUE(content::ExecJs(
        web_contents(), "changeManifestUrl('" + manifest_url.spec() + "');"));
    run_loop.Run();

    EXPECT_TRUE(blink::IsEmptyManifest(manager->manifest()));
  }

  {
    // Fetch again. This should return the data from the new manifest.
    base::RunLoop run_loop;
    std::unique_ptr<CallbackTester> tester(
        new CallbackTester(run_loop.QuitClosure()));

    manager->GetData(
        GetWebAppParams(),
        base::BindOnce(&CallbackTester::OnDidFinishInstallableCheck,
                       base::Unretained(tester.get())));
    run_loop.Run();

    EXPECT_FALSE(blink::IsEmptyManifest(tester->manifest()));
    EXPECT_EQ(std::u16string(),
              tester->manifest().name.value_or(std::u16string()));
    EXPECT_EQ(u"Manifest", tester->manifest().short_name);
    EXPECT_EQ(std::vector<InstallableStatusCode>{}, tester->errors());
  }
}

IN_PROC_BROWSER_TEST_F(InstallableManagerBrowserTest, DebugModeWithNoManifest) {
  // Ensure that a page with no manifest stops with NO_MANIFEST in debug mode.
  base::RunLoop run_loop;
  std::unique_ptr<CallbackTester> tester(
      new CallbackTester(run_loop.QuitClosure()));

  InstallableParams params = GetWebAppParams();
  params.is_debug_mode = true;
  ASSERT_TRUE(content::NavigateToURL(
      web_contents(),
      embedded_test_server()->GetURL("/banners/no_manifest_test_page.html")));
  RunInstallableManager(tester.get(), params);
  run_loop.Run();

  // The default manifest is created, but we still report no manifest.
  EXPECT_THAT(tester->errors(),
              testing::ElementsAre(InstallableStatusCode::NO_MANIFEST));
}

IN_PROC_BROWSER_TEST_F(InstallableManagerBrowserTest,
                       DebugModeAccumulatesErrorsWithManifest) {
  base::RunLoop run_loop;
  std::unique_ptr<CallbackTester> tester(
      new CallbackTester(run_loop.QuitClosure()));

  InstallableParams params = GetWebAppParams();
  params.is_debug_mode = true;
  NavigateAndRunInstallableManager(
      tester.get(), params,
      GetURLOfPageWithManifest("/banners/play_app_manifest.json"));
  run_loop.Run();

  EXPECT_THAT(tester->errors(),
              testing::UnorderedElementsAre(
                  InstallableStatusCode::START_URL_NOT_VALID,
                  InstallableStatusCode::MANIFEST_MISSING_NAME_OR_SHORT_NAME,
                  InstallableStatusCode::MANIFEST_DISPLAY_NOT_SUPPORTED,
                  InstallableStatusCode::MANIFEST_MISSING_SUITABLE_ICON,
                  InstallableStatusCode::NO_ACCEPTABLE_ICON));
}

IN_PROC_BROWSER_TEST_F(InstallableManagerBrowserTest,
                       DebugModeBadFallbackMaskable) {
  base::RunLoop run_loop;
  std::unique_ptr<CallbackTester> tester(
      new CallbackTester(run_loop.QuitClosure()));

  InstallableParams params = GetPrimaryIconPreferMaskableParams();
  params.is_debug_mode = true;

  NavigateAndRunInstallableManager(
      tester.get(), params,
      GetURLOfPageWithManifest("/banners/manifest_one_bad_maskable.json"));

  run_loop.Run();

  EXPECT_FALSE(blink::IsEmptyManifest(tester->manifest()));
  EXPECT_FALSE(tester->manifest_url().is_empty());

  EXPECT_TRUE(tester->primary_icon_url().is_empty());
  EXPECT_EQ(nullptr, tester->primary_icon());
  EXPECT_FALSE(tester->has_maskable_primary_icon());

  EXPECT_FALSE(tester->installable_check_passed());
  EXPECT_EQ(
      std::vector<InstallableStatusCode>{
          InstallableStatusCode::NO_ACCEPTABLE_ICON},
      tester->errors());
}

IN_PROC_BROWSER_TEST_F(InstallableManagerBrowserTest,
                       GetAllInstallabilityErrorsNoErrors) {
  EXPECT_THAT(
      NavigateAndGetAllInstallabilityErrors("/banners/manifest_test_page.html"),
      testing::IsEmpty());

  // Should pass a second time with no issues.
  EXPECT_THAT(GetAllInstallabilityErrors(), testing::IsEmpty());
}

IN_PROC_BROWSER_TEST_F(InstallableManagerBrowserTest,
                       GetAllInstallabilityErrorsWithNoManifest) {
  EXPECT_THAT(NavigateAndGetAllInstallabilityErrors(
                  "/banners/no_manifest_test_page.html"),
              testing::UnorderedElementsAre(
                  GetInstallabilityError(InstallableStatusCode::NO_MANIFEST)));

  // Should return a second time with no issues.
  EXPECT_THAT(GetAllInstallabilityErrors(),
              testing::UnorderedElementsAre(
                  GetInstallabilityError(InstallableStatusCode::NO_MANIFEST)));
}

IN_PROC_BROWSER_TEST_F(InstallableManagerBrowserTest,
                       GetAllInstallabilityErrorsWithPlayAppManifest) {
  auto errors = std::vector<content::InstallabilityError>(
      {GetInstallabilityError(InstallableStatusCode::START_URL_NOT_VALID),
       GetInstallabilityError(
           InstallableStatusCode::MANIFEST_MISSING_NAME_OR_SHORT_NAME),
       GetInstallabilityError(
           InstallableStatusCode::MANIFEST_DISPLAY_NOT_SUPPORTED),
       GetInstallabilityError(
           InstallableStatusCode::MANIFEST_MISSING_SUITABLE_ICON)});
  errors.push_back(
      GetInstallabilityError(InstallableStatusCode::NO_ACCEPTABLE_ICON));
  EXPECT_EQ(errors,
            NavigateAndGetAllInstallabilityErrors(
                GetURLOfPageWithManifest("/banners/play_app_manifest.json")));
}

#if !BUILDFLAG(IS_ANDROID)
class InstallableManagerAllowlistOriginBrowserTest
    : public InstallableManagerBrowserTest {
  void SetUpCommandLine(base::CommandLine* command_line) override {
    command_line->AppendSwitchASCII(kUnsafeSecureOriginFlag, kInsecureOrigin);
  }
};

IN_PROC_BROWSER_TEST_F(InstallableManagerAllowlistOriginBrowserTest,
                       SecureOriginCheckRespectsUnsafeFlag) {
  // The allowlisted origin should be regarded as secure.
  ASSERT_TRUE(ui_test_utils::NavigateToURL(browser(), GURL(kInsecureOrigin)));
  content::WebContents* contents =
      browser()->tab_strip_model()->GetActiveWebContents();
  EXPECT_TRUE(InstallableEvaluator::IsContentSecure(web_contents()));

  // While a non-allowlisted origin should not.
  ASSERT_TRUE(
      ui_test_utils::NavigateToURL(browser(), GURL(kOtherInsecureOrigin)));
  EXPECT_FALSE(InstallableEvaluator::IsContentSecure(contents));
}
#endif

IN_PROC_BROWSER_TEST_F(InstallableManagerBrowserTest, CheckScreenshots) {
  base::RunLoop run_loop;
  std::unique_ptr<CallbackTester> tester(
      new CallbackTester(run_loop.QuitClosure()));

  InstallableParams params = GetManifestParams();
  params.fetch_screenshots = true;

  NavigateAndRunInstallableManager(
      tester.get(), params,
      GetURLOfPageWithManifest("/banners/manifest_with_screenshots.json"));

  run_loop.Run();

  EXPECT_FALSE(blink::IsEmptyManifest(tester->manifest()));
  EXPECT_FALSE(tester->manifest_url().is_empty());

  EXPECT_FALSE(tester->installable_check_passed());
  EXPECT_EQ(1u, tester->screenshots().size());
  // Corresponding form_factor should filter out the screenshot with mismatched
  // form_factor.
#if BUILDFLAG(IS_ANDROID)
  EXPECT_LT(tester->screenshots()[0].image.width(),
            tester->screenshots()[0].image.height());
#else
  EXPECT_GT(tester->screenshots()[0].image.width(),
            tester->screenshots()[0].image.height());
#endif
  EXPECT_EQ(std::vector<InstallableStatusCode>{}, tester->errors());
}

IN_PROC_BROWSER_TEST_F(InstallableManagerBrowserTest,
                       CheckScreenshotsPlatform) {
  base::RunLoop run_loop;
  std::unique_ptr<CallbackTester> tester(
      new CallbackTester(run_loop.QuitClosure()));

  InstallableParams params = GetManifestParams();
  params.fetch_screenshots = true;

  size_t num_of_screenshots = 0;
#if BUILDFLAG(IS_ANDROID)
  NavigateAndRunInstallableManager(
      tester.get(), params,
      GetURLOfPageWithManifest(
          "/banners/manifest_with_narrow_screenshots.json"));
  // Screenshots with unspecified form_factor is not filtered out.
  num_of_screenshots = 2;
  // EXPECT_EQ(2u, tester->screenshots().size());
#else
  NavigateAndRunInstallableManager(
      tester.get(), params,
      GetURLOfPageWithManifest("/banners/manifest_with_wide_screenshots.json"));
  // Screenshots with unspecified form_factor is filtered out.
  num_of_screenshots = 1;
#endif
  run_loop.Run();

  EXPECT_FALSE(blink::IsEmptyManifest(tester->manifest()));
  EXPECT_FALSE(tester->manifest_url().is_empty());

  EXPECT_FALSE(tester->installable_check_passed());
  EXPECT_EQ(num_of_screenshots, tester->screenshots().size());
  EXPECT_EQ(std::vector<InstallableStatusCode>{}, tester->errors());
}

IN_PROC_BROWSER_TEST_F(InstallableManagerBrowserTest, CheckScreenshotsNumber) {
  base::RunLoop run_loop;
  std::unique_ptr<CallbackTester> tester(
      new CallbackTester(run_loop.QuitClosure()));

  InstallableParams params = GetManifestParams();
  params.fetch_screenshots = true;

  NavigateAndRunInstallableManager(
      tester.get(), params,
      GetURLOfPageWithManifest(
          "/banners/manifest_with_too_many_screenshots.json"));

  run_loop.Run();

  EXPECT_FALSE(blink::IsEmptyManifest(tester->manifest()));
  EXPECT_FALSE(tester->manifest_url().is_empty());

  EXPECT_FALSE(tester->installable_check_passed());
  EXPECT_EQ(8u, tester->screenshots().size());
  EXPECT_EQ(std::vector<InstallableStatusCode>{}, tester->errors());
}

IN_PROC_BROWSER_TEST_F(InstallableManagerBrowserTest,
                       CheckLargeScreenshotsFilteredOut) {
  base::RunLoop run_loop;
  std::unique_ptr<CallbackTester> tester(
      new CallbackTester(run_loop.QuitClosure()));

  InstallableParams params = GetManifestParams();
  params.fetch_screenshots = true;

  NavigateAndRunInstallableManager(
      tester.get(), params,
      GetURLOfPageWithManifest("/banners/manifest_large_screenshot.json"));

  run_loop.Run();

  EXPECT_FALSE(blink::IsEmptyManifest(tester->manifest()));
  EXPECT_FALSE(tester->manifest_url().is_empty());

  EXPECT_FALSE(tester->installable_check_passed());
  for (const auto& screenshot : tester->screenshots()) {
    EXPECT_LE(screenshot.image.width(), 3840);
    EXPECT_LE(screenshot.image.height(), 3840);
  }
  EXPECT_EQ(std::vector<InstallableStatusCode>{}, tester->errors());
}

IN_PROC_BROWSER_TEST_F(InstallableManagerBrowserTest,
                       ManifestLinkChangeReportsError) {
  InstallableManager* manager = GetManager();
  scoped_refptr<base::TestSimpleTaskRunner> test_task_runner =
      base::MakeRefCounted<base::TestSimpleTaskRunner>();
  manager->SetSequencedTaskRunnerForTesting(test_task_runner);

  base::RunLoop run_loop;
  std::unique_ptr<CallbackTester> tester(
      new CallbackTester(run_loop.QuitClosure(), test_task_runner));

  NavigateAndRunInstallableManager(tester.get(), GetManifestParams(),
                                   "/banners/manifest_test_page.html");

  // Simulate a manifest URL update by just calling the observer function.
  static_cast<content::WebContentsObserver*>(manager)->DidUpdateWebManifestURL(
      web_contents()->GetPrimaryMainFrame(), GURL());

  // This will run all tasks currently pending on the task runner. This includes
  // any changes that could have been caused by calling DidUpdateWebManifestURL,
  // which should synchronously modify the data to be passed to the tester
  // callback.
  test_task_runner->RunPendingTasks();
  run_loop.Run();

  ASSERT_EQ(tester->errors().size(), 1u);
  EXPECT_EQ(tester->errors()[0], InstallableStatusCode::MANIFEST_URL_CHANGED);
}

IN_PROC_BROWSER_TEST_F(InstallableManagerBrowserTest,
                       CheckManifestOnly_DisplayOverride) {
  base::RunLoop run_loop;
  std::unique_ptr<CallbackTester> tester(
      new CallbackTester(run_loop.QuitClosure()));

  NavigateAndRunInstallableManager(
      tester.get(), GetManifestParams(),
      GetURLOfPageWithManifest("/banners/manifest_display_override.json"));
  run_loop.Run();

  EXPECT_FALSE(blink::IsEmptyManifest(tester->manifest()));
  EXPECT_FALSE(tester->manifest_url().is_empty());
  ASSERT_EQ(2u, tester->manifest().display_override.size());
  EXPECT_EQ(blink::mojom::DisplayMode::kMinimalUi,
            tester->manifest().display_override[0]);
  EXPECT_EQ(blink::mojom::DisplayMode::kStandalone,
            tester->manifest().display_override[1]);

  EXPECT_TRUE(tester->primary_icon_url().is_empty());
  EXPECT_EQ(nullptr, tester->primary_icon());
  EXPECT_FALSE(tester->has_maskable_primary_icon());
  EXPECT_FALSE(tester->installable_check_passed());
  EXPECT_EQ(std::vector<InstallableStatusCode>{}, tester->errors());
}

IN_PROC_BROWSER_TEST_F(InstallableManagerBrowserTest,
                       ManifestDisplayOverrideReportsError_DisplayOverride) {
  base::RunLoop run_loop;
  std::unique_ptr<CallbackTester> tester(
      new CallbackTester(run_loop.QuitClosure()));

  NavigateAndRunInstallableManager(
      tester.get(), GetWebAppParams(),
      GetURLOfPageWithManifest(
          "/banners/manifest_display_override_contains_browser.json"));
  run_loop.Run();

  EXPECT_FALSE(blink::IsEmptyManifest(tester->manifest()));
  EXPECT_FALSE(tester->manifest_url().is_empty());
  ASSERT_EQ(3u, tester->manifest().display_override.size());
  EXPECT_EQ(blink::mojom::DisplayMode::kBrowser,
            tester->manifest().display_override[0]);
  EXPECT_EQ(blink::mojom::DisplayMode::kMinimalUi,
            tester->manifest().display_override[1]);
  EXPECT_EQ(blink::mojom::DisplayMode::kStandalone,
            tester->manifest().display_override[2]);
  EXPECT_EQ(
      std::vector<InstallableStatusCode>{
          InstallableStatusCode::MANIFEST_DISPLAY_OVERRIDE_NOT_SUPPORTED},
      tester->errors());
}

IN_PROC_BROWSER_TEST_F(InstallableManagerBrowserTest,
                       FallbackToDisplayBrowser_DisplayOverride) {
  base::RunLoop run_loop;
  std::unique_ptr<CallbackTester> tester(
      new CallbackTester(run_loop.QuitClosure()));

  NavigateAndRunInstallableManager(
      tester.get(), GetWebAppParams(),
      GetURLOfPageWithManifest(
          "/banners/manifest_display_override_display_is_browser.json"));
  run_loop.Run();

  EXPECT_FALSE(blink::IsEmptyManifest(tester->manifest()));
  EXPECT_FALSE(tester->manifest_url().is_empty());
  ASSERT_EQ(1u, tester->manifest().display_override.size());
  EXPECT_EQ(blink::mojom::DisplayMode::kStandalone,
            tester->manifest().display_override[0]);

  EXPECT_FALSE(tester->primary_icon_url().is_empty());
  EXPECT_NE(nullptr, tester->primary_icon());
  EXPECT_GT(tester->primary_icon()->width(), 0);
  EXPECT_TRUE(tester->installable_check_passed());
  EXPECT_EQ(std::vector<InstallableStatusCode>{}, tester->errors());
}

class InstallableManagerInPrerenderingBrowserTest
    : public InstallableManagerBrowserTest {
 public:
  InstallableManagerInPrerenderingBrowserTest()
      : prerender_helper_(base::BindRepeating(
            &InstallableManagerInPrerenderingBrowserTest::web_contents,
            base::Unretained(this))) {}
  ~InstallableManagerInPrerenderingBrowserTest() override = default;

  content::test::PrerenderTestHelper* prerender_helper() {
    return &prerender_helper_;
  }

 private:
  content::test::PrerenderTestHelper prerender_helper_;
};

IN_PROC_BROWSER_TEST_F(InstallableManagerInPrerenderingBrowserTest,
                       InstallableManagerInPrerendering) {
  auto manager = std::make_unique<ResetDataInstallableManager>(web_contents());
  GURL url = embedded_test_server()->GetURL("/empty.html");
  ASSERT_TRUE(content::NavigateToURL(web_contents(), url));

  manager->ClearOnResetData();

  // Loads a page in the prerendering.
  const std::string path = "/banners/manifest_test_page.html";
  auto prerender_url = embedded_test_server()->GetURL(path);
  content::FrameTreeNodeId host_id =
      prerender_helper()->AddPrerender(prerender_url);
  content::test::PrerenderHostObserver host_observer(*web_contents(), host_id);

  // The prerendering should not affect the current data.
  EXPECT_FALSE(manager->GetOnResetData());

  {
    // Fetches the data.
    base::RunLoop run_loop;
    std::unique_ptr<CallbackTester> tester(
        new CallbackTester(run_loop.QuitClosure()));

    manager->GetData(
        GetWebAppParams(),
        base::BindOnce(&CallbackTester::OnDidFinishInstallableCheck,
                       base::Unretained(tester.get())));
    run_loop.Run();
    // It should have the default manifest for `url` since no data since
    // manifest_test_page.html is loaded in the prerendering.
    EXPECT_TRUE(blink::IsDefaultManifest(manager->manifest(), url));
    EXPECT_EQ(
        std::vector<InstallableStatusCode>{InstallableStatusCode::NO_MANIFEST},
        tester->errors());
  }

  {
    // If the page is activated from the prerendering and the data should be
    // reset.
    base::RunLoop run_loop;
    manager->SetQuitClosure(run_loop.QuitClosure());
    NavigateToPath(path);
    run_loop.Run();
    EXPECT_TRUE(blink::IsEmptyManifest(manager->manifest()));
    EXPECT_EQ(InstallableStatusCode::NO_ERROR_DETECTED,
              manager->manifest_error());
  }

  {
    // Fetch the data again. This should succeed.
    base::RunLoop run_loop;
    std::unique_ptr<CallbackTester> tester(
        new CallbackTester(run_loop.QuitClosure()));

    manager->GetData(
        GetWebAppParams(),
        base::BindOnce(&CallbackTester::OnDidFinishInstallableCheck,
                       base::Unretained(tester.get())));
    run_loop.Run();
    EXPECT_FALSE(blink::IsEmptyManifest(tester->manifest()));
    EXPECT_EQ(std::vector<InstallableStatusCode>{}, tester->errors());
    EXPECT_EQ(u"Manifest test app", tester->manifest().name);
    EXPECT_EQ(std::u16string(),
              tester->manifest().short_name.value_or(std::u16string()));
  }
}

class MockInstallableManager : public InstallableManager {
 public:
  explicit MockInstallableManager(content::WebContents* web_contents)
      : InstallableManager(web_contents) {}
  ~MockInstallableManager() override = default;

  MOCK_METHOD(void, OnResetData, (), (override));
  MOCK_METHOD(void,
              DidUpdateWebManifestURL,
              (content::RenderFrameHost * rfh, const GURL& manifest_url),
              (override));
};

MATCHER_P(IsManifestURL, file_name, std::string()) {
  return arg.ExtractFileName() == file_name;
}

MATCHER_P(IsPrerenderedRFH, render_frame_host, std::string()) {
  return arg->GetGlobalId() == render_frame_host->GetGlobalId();
}

// TODO(crbug.com/40275175): Test failed on Android.
#if BUILDFLAG(IS_ANDROID)
#define MAYBE_NotifyManifestUrlChangedInActivation \
  DISABLED_NotifyManifestUrlChangedInActivation
#else
#define MAYBE_NotifyManifestUrlChangedInActivation \
  NotifyManifestUrlChangedInActivation
#endif
// Tests that NotifyManifestUrlChanged is called on the page that has manifest
// after the activation from the prerendering.
IN_PROC_BROWSER_TEST_F(InstallableManagerInPrerenderingBrowserTest,
                       MAYBE_NotifyManifestUrlChangedInActivation) {
  auto manager = std::make_unique<MockInstallableManager>(web_contents());
  GURL url = embedded_test_server()->GetURL("/empty.html");
  // OnResetData() is called when a navigation is finished.
  EXPECT_CALL(*manager.get(), OnResetData()).Times(1);
  ASSERT_TRUE(content::NavigateToURL(web_contents(), url));

  // Loads a page in the prerendering.
  auto prerender_url =
      embedded_test_server()->GetURL("/banners/manifest_test_page.html");
  // OnResetData() should not be called on the prerendering.
  EXPECT_CALL(*manager.get(), OnResetData()).Times(0);
  content::FrameTreeNodeId host_id =
      prerender_helper()->AddPrerender(prerender_url);

  content::test::PrerenderHostObserver host_observer(*web_contents(), host_id);
  content::RenderFrameHost* render_frame_host =
      prerender_helper()->GetPrerenderedMainFrameHost(host_id);

  {
    // Fetches the data.
    base::RunLoop run_loop;
    std::unique_ptr<CallbackTester> tester(
        new CallbackTester(run_loop.QuitClosure()));

    manager->GetData(
        GetWebAppParams(),
        base::BindOnce(&CallbackTester::OnDidFinishInstallableCheck,
                       base::Unretained(tester.get())));
    run_loop.Run();
    // It should have default data for the original `url` since
    // manifest_test_page.html is loaded in the prerendering.
    EXPECT_TRUE(blink::IsDefaultManifest(manager->manifest(), url));
    EXPECT_EQ(
        std::vector<InstallableStatusCode>{InstallableStatusCode::NO_MANIFEST},
        tester->errors());
  }

  {
    // If the page is activated from the prerendering and the data should be
    // reset and notify the updated manifest url.
    EXPECT_CALL(*manager.get(), OnResetData()).Times(1);
    EXPECT_CALL(*manager.get(),
                DidUpdateWebManifestURL(IsPrerenderedRFH(render_frame_host),
                                        IsManifestURL("manifest.json")));
    prerender_helper()->NavigatePrimaryPage(prerender_url);
  }

  EXPECT_TRUE(host_observer.was_activated());
  EXPECT_TRUE(blink::IsEmptyManifest(manager->manifest()));
  EXPECT_EQ(InstallableStatusCode::NO_ERROR_DETECTED,
            manager->manifest_error());

  {
    // Fetch the data again. This should succeed.
    base::RunLoop run_loop;
    std::unique_ptr<CallbackTester> tester(
        new CallbackTester(run_loop.QuitClosure()));

    manager->GetData(
        GetWebAppParams(),
        base::BindOnce(&CallbackTester::OnDidFinishInstallableCheck,
                       base::Unretained(tester.get())));
    run_loop.Run();
    EXPECT_FALSE(blink::IsEmptyManifest(tester->manifest()));
    EXPECT_EQ(std::vector<InstallableStatusCode>{}, tester->errors());
    EXPECT_EQ(u"Manifest test app", tester->manifest().name);
    EXPECT_EQ(std::u16string(),
              tester->manifest().short_name.value_or(std::u16string()));
  }
}

// TODO(crbug.com/40275175): Test failed on Android.
#if BUILDFLAG(IS_ANDROID)
#define MAYBE_NotNotifyManifestUrlChangedInActivation \
  DISABLED_NotNotifyManifestUrlChangedInActivation
#else
#define MAYBE_NotNotifyManifestUrlChangedInActivation \
  NotNotifyManifestUrlChangedInActivation
#endif
// Tests that NotifyManifestUrlChanged is not called without manifest after
// the activation from the prerendering.
IN_PROC_BROWSER_TEST_F(InstallableManagerInPrerenderingBrowserTest,
                       MAYBE_NotNotifyManifestUrlChangedInActivation) {
  auto manager = std::make_unique<MockInstallableManager>(web_contents());
  GURL url = embedded_test_server()->GetURL("/empty.html");
  // OnResetData() is called when a navigation is finished.
  EXPECT_CALL(*manager.get(), OnResetData()).Times(1);
  ASSERT_TRUE(content::NavigateToURL(web_contents(), url));

  // Loads a page in the prerendering.
  auto prerender_url =
      embedded_test_server()->GetURL("/banners/no_manifest_test_page.html");
  // OnResetData() should not be called on the prerendering.
  EXPECT_CALL(*manager.get(), OnResetData()).Times(0);
  content::FrameTreeNodeId host_id =
      prerender_helper()->AddPrerender(prerender_url);

  content::test::PrerenderHostObserver host_observer(*web_contents(), host_id);
  {
    // Fetches the data.
    base::RunLoop run_loop;
    std::unique_ptr<CallbackTester> tester(
        new CallbackTester(run_loop.QuitClosure()));

    manager->GetData(
        GetWebAppParams(),
        base::BindOnce(&CallbackTester::OnDidFinishInstallableCheck,
                       base::Unretained(tester.get())));
    run_loop.Run();
    // It should have default data for the original `url` since
    // manifest_test_page.html is loaded in the prerendering.
    EXPECT_TRUE(blink::IsDefaultManifest(manager->manifest(), url));
    EXPECT_EQ(
        std::vector<InstallableStatusCode>{InstallableStatusCode::NO_MANIFEST},
        tester->errors());
  }

  // OnResetData() is called when a navigation is finished.
  EXPECT_CALL(*manager.get(), OnResetData()).Times(1);
  // OnResetData() should not be called when a page doesn't have a manifest.
  EXPECT_CALL(*manager.get(), DidUpdateWebManifestURL(testing::_, testing::_))
      .Times(0);
  prerender_helper()->NavigatePrimaryPage(prerender_url);

  EXPECT_TRUE(host_observer.was_activated());
  EXPECT_TRUE(blink::IsEmptyManifest(manager->manifest()));
  EXPECT_EQ(InstallableStatusCode::NO_ERROR_DETECTED,
            manager->manifest_error());

  {
    // Fetch the data again. This should return the default manifest for the
    // prerendered page now.
    base::RunLoop run_loop;
    std::unique_ptr<CallbackTester> tester(
        new CallbackTester(run_loop.QuitClosure()));

    manager->GetData(
        GetWebAppParams(),
        base::BindOnce(&CallbackTester::OnDidFinishInstallableCheck,
                       base::Unretained(tester.get())));
    run_loop.Run();
    EXPECT_TRUE(blink::IsDefaultManifest(manager->manifest(), prerender_url));
    EXPECT_EQ(InstallableStatusCode::NO_ERROR_DETECTED,
              manager->manifest_error());
    EXPECT_THAT(tester->errors(),
                testing::ElementsAre(InstallableStatusCode::NO_MANIFEST));
  }
}

IN_PROC_BROWSER_TEST_F(InstallableManagerBrowserTest, NoManifestRootScopeTest) {
  InstallableParams params;
  params.fetch_metadata = true;
  params.installable_criteria = InstallableCriteria::kNoManifestAtRootScope;

  std::map<std::string, std::string> meta_tags = {
      {"application-name", "Test App Name"},
      {"icon", "/banners/256x256-red.png"}};
  {
    base::RunLoop run_loop;
    std::unique_ptr<CallbackTester> tester(
        new CallbackTester(run_loop.QuitClosure()));

    std::string path =
        GetUrlOfPageWithTags("/no_manifest_test_page.html", meta_tags);
    NavigateAndRunInstallableManager(tester.get(), params, path);

    run_loop.Run();

    EXPECT_TRUE(blink::IsDefaultManifest(tester->manifest(),
                                         embedded_test_server()->GetURL(path)));
    EXPECT_TRUE(tester->manifest_url().is_empty());
    EXPECT_EQ(u"Test App Name", tester->metadata().application_name);
    EXPECT_TRUE(tester->manifest().icons.empty());
    EXPECT_FALSE(web_contents()->GetFaviconURLs().empty());
    EXPECT_TRUE(tester->installable_check_passed());
    EXPECT_THAT(tester->errors(), testing::IsEmpty());
  }
  {
    base::RunLoop run_loop;
    std::unique_ptr<CallbackTester> tester(
        new CallbackTester(run_loop.QuitClosure()));

    std::string path =
        GetUrlOfPageWithTags("/no_manifest_test_page.html", meta_tags);
    NavigateAndRunInstallableManager(tester.get(), params, path);

    run_loop.Run();

    EXPECT_TRUE(blink::IsDefaultManifest(tester->manifest(),
                                         embedded_test_server()->GetURL(path)));
    EXPECT_TRUE(tester->manifest_url().is_empty());
    EXPECT_EQ(u"Test App Name", tester->metadata().application_name);
    EXPECT_TRUE(tester->manifest().icons.empty());
    EXPECT_FALSE(web_contents()->GetFaviconURLs().empty());
    EXPECT_TRUE(tester->installable_check_passed());
    EXPECT_THAT(tester->errors(), testing::IsEmpty());
  }
}
}  // namespace webapps
