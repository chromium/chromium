// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/webshare/chromeos/sharesheet_client.h"

#include <string>
#include <vector>

#include "base/bind.h"
#include "base/files/file_path.h"
#include "base/files/file_util.h"
#include "base/memory/scoped_refptr.h"
#include "base/run_loop.h"
#include "base/test/bind.h"
#include "base/test/task_environment.h"
#include "chrome/browser/chromeos/file_manager/path_util.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/sharesheet/sharesheet_types.h"
#include "chrome/browser/webshare/prepare_directory_task.h"
#include "chrome/browser/webshare/store_file_task.h"
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
    StoreFileTask::SkipCopyingForTesting();
    SharesheetClient::SetSharesheetCallbackForTesting(
        base::BindRepeating(&SharesheetClientUnitTest::AcceptShareRequest));
  }

  void SetGuest() {
    Profile* const otr_profile = profile()->GetOffTheRecordProfile(
        Profile::OTRProfileID("Test::SharesheetClient"));
    EXPECT_TRUE(otr_profile->IsOffTheRecord());
    EXPECT_FALSE(otr_profile->IsIncognitoProfile());
    scoped_refptr<content::SiteInstance> instance =
        content::SiteInstance::Create(otr_profile);
    SetContents(content::WebContentsTester::CreateTestWebContents(
        otr_profile, std::move(instance)));
  }

  void SetIncognito() {
    Profile* const otr_profile = profile()->GetPrimaryOTRProfile();
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
      const std::vector<std::string>& content_types,
      const std::string& text,
      const std::string& title,
      sharesheet::DeliveredCallback delivered_callback) {
    std::move(delivered_callback).Run(sharesheet::SharesheetResult::kSuccess);
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

TEST_F(SharesheetClientUnitTest, DeleteAfterShare) {
  SetGuest();
  SharesheetClient sharesheet_client(web_contents());
  const base::FilePath my_files =
      file_manager::util::GetMyFilesFolderForProfile(profile());
  const base::FilePath first_file =
      my_files.AppendASCII(".WebShare/share1.txt");
  const base::FilePath second_file =
      my_files.AppendASCII(".WebShare/share2.txt");

  const std::string title = "Subject";
  const std::string text = "Message";
  const GURL share_url("https://example.com/");
  std::vector<blink::mojom::SharedFilePtr> files;
  files.push_back(blink::mojom::SharedFile::New(
      first_file.AsUTF8Unsafe(), blink::mojom::SerializedBlob::New()));
  files.push_back(blink::mojom::SharedFile::New(
      second_file.AsUTF8Unsafe(), blink::mojom::SerializedBlob::New()));

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

  task_environment()->FastForwardBy(PrepareDirectoryTask::kSharedFileLifetime /
                                    2);
  EXPECT_TRUE(base::PathExists(first_file));
  EXPECT_TRUE(base::PathExists(second_file));

  task_environment()->FastForwardBy(PrepareDirectoryTask::kSharedFileLifetime *
                                    2);
  EXPECT_FALSE(base::PathExists(first_file));
  EXPECT_FALSE(base::PathExists(second_file));
}

}  // namespace webshare
