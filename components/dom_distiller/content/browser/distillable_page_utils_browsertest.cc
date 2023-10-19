// Copyright 2015 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/dom_distiller/content/browser/distillable_page_utils.h"

#include <utility>

#include "base/functional/bind.h"
#include "base/functional/callback_helpers.h"
#include "base/path_service.h"
#include "base/run_loop.h"
#include "build/build_config.h"
#include "components/dom_distiller/content/browser/distiller_javascript_utils.h"
#include "components/dom_distiller/core/distillable_page_detector.h"
#include "components/dom_distiller/core/page_features.h"
#include "content/public/browser/browser_context.h"
#include "content/public/browser/render_frame_host.h"
#include "content/public/browser/web_contents_observer.h"
#include "content/public/common/isolated_world_ids.h"
#include "content/public/test/browser_test.h"
#include "content/public/test/content_browser_test.h"
#include "content/shell/browser/shell.h"
#include "net/test/embedded_test_server/embedded_test_server.h"
#include "ui/base/resource/resource_bundle.h"

namespace dom_distiller {
namespace {

const char* kArticlePath = "/og_article.html";

class DomDistillerDistillablePageUtilsTest : public content::ContentBrowserTest,
                                             content::WebContentsObserver {
 public:
  void SetUpOnMainThread() override {
    if (!DistillerJavaScriptWorldIdIsSet()) {
      SetDistillerJavaScriptWorldId(content::ISOLATED_WORLD_ID_CONTENT_END);
    }
    AddComponentsResources();
    SetUpTestServer();
    ContentBrowserTest::SetUpOnMainThread();
  }

  void LoadURL(const std::string& url) {
    content::WebContents* current_web_contents = shell()->web_contents();
    Observe(current_web_contents);
    base::RunLoop url_loaded_runner;
    main_frame_loaded_callback_ = url_loaded_runner.QuitClosure();
    current_web_contents->GetController().LoadURL(
        embedded_test_server()->GetURL(url), content::Referrer(),
        ui::PAGE_TRANSITION_TYPED, std::string());
    url_loaded_runner.Run();
    main_frame_loaded_callback_.Reset();
    Observe(nullptr);
  }

 private:
  void AddComponentsResources() {
    base::FilePath pak_file;
    base::FilePath pak_dir;
#if BUILDFLAG(IS_ANDROID)
    CHECK(base::PathService::Get(base::DIR_ANDROID_APP_DATA, &pak_dir));
    pak_dir = pak_dir.Append(FILE_PATH_LITERAL("paks"));
#elif BUILDFLAG(IS_MAC)
    base::PathService::Get(base::DIR_MODULE, &pak_dir);
#else
    base::PathService::Get(base::DIR_ASSETS, &pak_dir);
#endif  // BUILDFLAG(IS_ANDROID)
    pak_file =
        pak_dir.Append(FILE_PATH_LITERAL("components_tests_resources.pak"));
    ui::ResourceBundle::GetSharedInstance().AddDataPackFromPath(
        pak_file, ui::kScaleFactorNone);
  }

  void SetUpTestServer() {
    base::FilePath path;
    base::PathService::Get(base::DIR_SRC_TEST_DATA_ROOT, &path);
    path = path.AppendASCII("components/test/data/dom_distiller");
    embedded_test_server()->ServeFilesFromDirectory(path);
    ASSERT_TRUE(embedded_test_server()->Start());
  }

  void DOMContentLoaded(content::RenderFrameHost* render_frame_host) override {
    if (!render_frame_host->GetParent())
      std::move(main_frame_loaded_callback_).Run();
  }

  base::OnceClosure main_frame_loaded_callback_;
};

class ResultHolder {
 public:
  ResultHolder(base::OnceClosure callback) : callback_(std::move(callback)) {}

  void OnResult(bool result) {
    result_ = result;
    std::move(callback_).Run();
  }

  bool GetResult() { return result_; }

  base::OnceCallback<void(bool)> GetCallback() {
    return base::BindOnce(&ResultHolder::OnResult, base::Unretained(this));
  }

 private:
  base::OnceClosure callback_;
  bool result_;
};

}  // namespace

IN_PROC_BROWSER_TEST_F(DomDistillerDistillablePageUtilsTest,
                       TestIsDistillablePage) {
  std::unique_ptr<AdaBoostProto> proto(new AdaBoostProto);
  proto->set_num_features(kDerivedFeaturesCount);
  proto->set_num_stumps(1);

  StumpProto* stump = proto->add_stump();
  stump->set_feature_number(0);
  stump->set_weight(1);
  stump->set_split(-1);
  std::unique_ptr<DistillablePageDetector> detector(
      new DistillablePageDetector(std::move(proto)));
  EXPECT_DOUBLE_EQ(0.5, detector->GetThreshold());
  // The first value of the first feature is either 0 or 1. Since the stump's
  // split is -1, the stump weight will be applied to any set of derived
  // features.
  LoadURL(kArticlePath);
  base::RunLoop run_loop_;
  ResultHolder holder(run_loop_.QuitClosure());
  IsDistillablePageForDetector(shell()->web_contents(), detector.get(),
                               holder.GetCallback());
  run_loop_.Run();
  ASSERT_TRUE(holder.GetResult());
}

IN_PROC_BROWSER_TEST_F(DomDistillerDistillablePageUtilsTest,
                       TestIsNotDistillablePage) {
  std::unique_ptr<AdaBoostProto> proto(new AdaBoostProto);
  proto->set_num_features(kDerivedFeaturesCount);
  proto->set_num_stumps(1);
  StumpProto* stump = proto->add_stump();
  stump->set_feature_number(0);
  stump->set_weight(-1);
  stump->set_split(-1);
  std::unique_ptr<DistillablePageDetector> detector(
      new DistillablePageDetector(std::move(proto)));
  EXPECT_DOUBLE_EQ(-0.5, detector->GetThreshold());
  // The first value of the first feature is either 0 or 1. Since the stump's
  // split is -1, the stump weight will be applied to any set of derived
  // features.
  LoadURL(kArticlePath);
  base::RunLoop run_loop_;
  ResultHolder holder(run_loop_.QuitClosure());
  IsDistillablePageForDetector(shell()->web_contents(), detector.get(),
                               holder.GetCallback());
  run_loop_.Run();
  ASSERT_FALSE(holder.GetResult());
}

}  // namespace dom_distiller
