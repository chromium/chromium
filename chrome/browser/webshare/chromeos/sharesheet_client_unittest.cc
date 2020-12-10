// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/webshare/chromeos/sharesheet_client.h"

#include <string>
#include <vector>

#include "base/bind.h"
#include "base/files/file_path.h"
#include "base/memory/scoped_refptr.h"
#include "base/run_loop.h"
#include "base/test/bind.h"
#include "base/test/task_environment.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/sharesheet/sharesheet_types.h"
#include "chrome/test/base/chrome_render_view_host_test_harness.h"
#include "content/public/browser/site_instance.h"
#include "content/public/test/web_contents_tester.h"
#include "third_party/blink/public/mojom/webshare/webshare.mojom.h"
#include "url/gurl.h"

namespace webshare {

class SharesheetClientUnitTest : public ChromeRenderViewHostTestHarness {
 public:
  SharesheetClientUnitTest()
      : ChromeRenderViewHostTestHarness(
            base::test::TaskEnvironment::TimeSource::MOCK_TIME) {}

  void SetUp() override {
    ChromeRenderViewHostTestHarness::SetUp();
    SharesheetClient::SetSharesheetCallbackForTesting(
        base::BindRepeating(&SharesheetClientUnitTest::AcceptShareRequest));
  }

  void SetIncognito() {
    Profile* const otr_profile = profile()->GetOffTheRecordProfile(
        Profile::OTRProfileID("Test::SharesheetClient"));
    scoped_refptr<content::SiteInstance> instance =
        content::SiteInstance::Create(otr_profile);
    SetContents(content::WebContentsTester::CreateTestWebContents(
        otr_profile, std::move(instance)));
  }

  static void AcceptShareRequest(content::WebContents* web_contents,
                                 const std::vector<base::FilePath>& file_paths,
                                 const std::vector<std::string>& content_types,
                                 const std::string& text,
                                 const std::string& title,
                                 sharesheet::CloseCallback close_callback) {
    std::move(close_callback).Run(sharesheet::SharesheetResult::kSuccess);
  }
};

TEST_F(SharesheetClientUnitTest, TestDenyInIncognitoAfterDelay) {
  SetIncognito();
  SharesheetClient sharesheet_client(web_contents());

  const std::string title = "Subject";
  const std::string text = "Message";
  const GURL share_url("https://example.com/");
  std::vector<blink::mojom::SharedFilePtr> files;
  files.push_back(blink::mojom::SharedFilePtr());

  blink::mojom::ShareError error = blink::mojom::ShareError::INTERNAL_ERROR;
  sharesheet_client.Share(
      title, text, share_url, std::move(files),
      base::BindLambdaForTesting(
          [&error](blink::mojom::ShareError in_error) { error = in_error; }));

  // Should be cancelled after 1-2 seconds. So 500ms is not enough.
  task_environment()->FastForwardBy(base::TimeDelta::FromMilliseconds(500));
  EXPECT_EQ(error, blink::mojom::ShareError::INTERNAL_ERROR);

  // But 5*500ms > 2 seconds, so it should now be cancelled.
  for (int n = 0; n < 4; n++)
    task_environment()->FastForwardBy(base::TimeDelta::FromMilliseconds(500));
  EXPECT_EQ(error, blink::mojom::ShareError::CANCELED);
}

TEST_F(SharesheetClientUnitTest, TestWithoutFilesInIncognito) {
  SetIncognito();
  SharesheetClient sharesheet_client(web_contents());

  const std::string title = "Subject";
  const std::string text = "Message";
  const GURL share_url("https://example.com/");
  std::vector<blink::mojom::SharedFilePtr> files;

  base::RunLoop run_loop;
  blink::mojom::ShareError error = blink::mojom::ShareError::INTERNAL_ERROR;
  sharesheet_client.Share(
      title, text, share_url, std::move(files),
      base::BindLambdaForTesting(
          [&run_loop, &error](blink::mojom::ShareError in_error) {
            error = in_error;
            run_loop.Quit();
          }));

  run_loop.Run();
  EXPECT_EQ(error, blink::mojom::ShareError::OK);
}

}  // namespace webshare
