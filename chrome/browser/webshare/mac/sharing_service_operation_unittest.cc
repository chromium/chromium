// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/webshare/mac/sharing_service_operation.h"

#include "base/run_loop.h"
#include "base/test/bind.h"
#include "base/test/scoped_feature_list.h"
#include "base/test/task_environment.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/browser/ui/tabs/tab_strip_model.h"
#include "chrome/browser/webshare/store_file_task.h"
#include "chrome/common/chrome_features.h"
#include "chrome/test/base/chrome_render_view_host_test_harness.h"
#include "content/public/browser/site_instance.h"
#include "content/public/test/web_contents_tester.h"
#include "third_party/blink/public/mojom/webshare/webshare.mojom.h"
#include "url/gurl.h"

namespace webshare {

class SharingServiceOperationUnitTest : public ChromeRenderViewHostTestHarness {
 public:
  SharingServiceOperationUnitTest()
      : ChromeRenderViewHostTestHarness(
            base::test::TaskEnvironment::TimeSource::MOCK_TIME) {
    feature_list_.InitAndEnableFeature(features::kWebShare);
  }
  ~SharingServiceOperationUnitTest() override = default;

  void SetUp() override {
    ChromeRenderViewHostTestHarness::SetUp();
    StoreFileTask::SkipCopyingForTesting();
    SharingServiceOperation::SetSharePickerCallbackForTesting(
        base::BindRepeating(
            &SharingServiceOperationUnitTest::AcceptShareRequest));
  }

  void SetIncognito() {
    Profile* const otr_profile =
        profile()->GetPrimaryOTRProfile(/*create_if_needed=*/true);
    EXPECT_TRUE(otr_profile->IsOffTheRecord());
    EXPECT_TRUE(otr_profile->IsIncognitoProfile());
    scoped_refptr<content::SiteInstance> instance =
        content::SiteInstance::Create(otr_profile);
    SetContents(content::WebContentsTester::CreateTestWebContents(
        otr_profile, std::move(instance)));
  }

  static void AcceptShareRequest(
      content::WebContents* web_contents,
      const std::vector<base::FilePath>& file_paths,
      const std::string& text,
      const std::string& title,
      const GURL& url,
      blink::mojom::ShareService::ShareCallback close_callback) {
    std::move(close_callback).Run(blink::mojom::ShareError::OK);
  }

 private:
  base::test::ScopedFeatureList feature_list_;
};

TEST_F(SharingServiceOperationUnitTest, TestIncognitoWithFiles) {
  SetIncognito();

  const std::string title = "Title";
  const std::string text = "Text";
  const GURL url("https://example.com");
  std::vector<blink::mojom::SharedFilePtr> files;
  files.push_back(blink::mojom::SharedFilePtr());

  SharingServiceOperation sharing_service_operation(
      title, text, url, std::move(files), web_contents());

  blink::mojom::ShareError error = blink::mojom::ShareError::INTERNAL_ERROR;

  sharing_service_operation.Share(base::BindLambdaForTesting(
      [&error](blink::mojom::ShareError in_error) { error = in_error; }));

  // Should be cancelled after 1-2 seconds. So 500ms is not enough.
  task_environment()->FastForwardBy(base::Milliseconds(500));
  EXPECT_EQ(error, blink::mojom::ShareError::INTERNAL_ERROR);

  // But 5*500ms > 2 seconds, so it should now be cancelled.
  for (int n = 0; n < 4; n++)
    task_environment()->FastForwardBy(base::Milliseconds(500));
  EXPECT_EQ(error, blink::mojom::ShareError::CANCELED);
}

TEST_F(SharingServiceOperationUnitTest, TestIncognitoWithoutFiles) {
  SetIncognito();

  const std::string title = "Title";
  const std::string text = "Text";
  const GURL url("https://example.com");
  std::vector<blink::mojom::SharedFilePtr> files;

  SharingServiceOperation sharing_service_operation(
      title, text, url, std::move(files), web_contents());

  base::RunLoop run_loop;
  blink::mojom::ShareError error = blink::mojom::ShareError::INTERNAL_ERROR;

  sharing_service_operation.Share(base::BindLambdaForTesting(
      [&run_loop, &error](blink::mojom::ShareError in_error) {
        error = in_error;
        run_loop.Quit();
      }));

  run_loop.Run();
  EXPECT_EQ(error, blink::mojom::ShareError::OK);
}

}  // namespace webshare
