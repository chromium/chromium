// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <memory>
#include <string>

#include "base/files/file_path.h"
#include "base/path_service.h"
#include "base/run_loop.h"
#include "base/test/scoped_feature_list.h"
#include "base/time/time.h"
#include "base/util/values/values_util.h"
#include "chrome/browser/content_settings/host_content_settings_map_factory.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/browser/ui/hats/hats_service.h"
#include "chrome/browser/ui/hats/hats_service_factory.h"
#include "chrome/browser/ui/hats/mock_hats_service.h"
#include "chrome/browser/ui/test/test_browser_dialog.h"
#include "chrome/browser/ui/views/frame/browser_view.h"
#include "chrome/browser/ui/views/hats/hats_bubble_view.h"
#include "chrome/browser/ui/views/hats/hats_next_web_dialog.h"
#include "chrome/browser/ui/views/hats/hats_web_dialog.h"
#include "chrome/common/chrome_features.h"
#include "chrome/common/chrome_paths.h"
#include "chrome/common/pref_names.h"
#include "components/content_settings/core/browser/host_content_settings_map.h"
#include "components/version_info/version_info.h"
#include "content/public/test/browser_test.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "url/gurl.h"

class HatsBubbleTest : public DialogBrowserTest {
 public:
  HatsBubbleTest() {}

  // DialogBrowserTest:
  void ShowUi(const std::string& name) override {
    ASSERT_TRUE(browser()->is_type_normal());
    BrowserView::GetBrowserViewForBrowser(InProcessBrowserTest::browser())
        ->ShowHatsBubble("test_site_id");
  }

 private:
  DISALLOW_COPY_AND_ASSIGN(HatsBubbleTest);
};

class TestHatsWebDialog : public HatsWebDialog {
 public:
  TestHatsWebDialog(Browser* browser,
                    const base::TimeDelta& timeout,
                    const GURL& url)
      : HatsWebDialog(browser, "fake_id_not_used"),
        loading_timeout_(timeout),
        content_url_(url) {}

  // ui::WebDialogDelegate implementation.
  GURL GetDialogContentURL() const override {
    if (content_url_.is_valid()) {
      // When we have a valid overridden url, use it instead.
      return content_url_;
    }
    return HatsWebDialog::GetDialogContentURL();
  }

  void OnMainFrameResourceLoadComplete(
      const blink::mojom::ResourceLoadInfo& resource_load_info) {
    if (resource_load_info.net_error == net::Error::OK &&
        resource_load_info.original_url == resource_url_) {
      // The resource is loaded successfully.
      resource_loaded_ = true;
    }
  }

  void set_resource_url(const GURL& url) { resource_url_ = url; }
  bool resource_loaded() { return resource_loaded_; }

  MOCK_METHOD0(OnWebContentsFinishedLoad, void());
  MOCK_METHOD0(OnLoadTimedOut, void());

 private:
  const base::TimeDelta ContentLoadingTimeout() const override {
    return loading_timeout_;
  }

  base::TimeDelta loading_timeout_;
  GURL content_url_;
  GURL resource_url_;
};

class HatsWebDialogBrowserTest : public InProcessBrowserTest {
 public:
  TestHatsWebDialog* Create(Browser* browser,
                            const base::TimeDelta& timeout,
                            const GURL& url = GURL()) {
    auto* hats_dialog = new TestHatsWebDialog(browser, timeout, url);
    hats_dialog->CreateWebDialog(browser);
    return hats_dialog;
  }
};

// Test that calls ShowUi("default").
IN_PROC_BROWSER_TEST_F(HatsBubbleTest, InvokeUi_Default) {
  ShowAndVerifyUi();
}

// Test time out of preloading works.
IN_PROC_BROWSER_TEST_F(HatsWebDialogBrowserTest, Timeout) {
  TestHatsWebDialog* dialog = Create(browser(), base::TimeDelta());
  EXPECT_CALL(*dialog, OnLoadTimedOut).Times(1);
}

// Test preloading content works.
IN_PROC_BROWSER_TEST_F(HatsWebDialogBrowserTest, ContentPreloading) {
  base::FilePath test_data_dir;
  base::PathService::Get(chrome::DIR_TEST_DATA, &test_data_dir);
  std::string contents;
  {
    base::ScopedAllowBlockingForTesting allow_blocking;
    EXPECT_TRUE(base::ReadFileToString(test_data_dir.AppendASCII("simple.html"),
                                       &contents));
  }

  TestHatsWebDialog* dialog =
      Create(browser(), base::TimeDelta::FromSeconds(100),
             GURL("data:text/html;charset=utf-8," + contents));
  base::RunLoop run_loop;
  EXPECT_CALL(*dialog, OnWebContentsFinishedLoad)
      .WillOnce(testing::Invoke(&run_loop, &base::RunLoop::Quit));
  run_loop.Run();
}

// Test the correct state will be set when the resource fails to load.
// Load with_inline_js.html which has an inline javascript that points to a
// nonexistent file.
IN_PROC_BROWSER_TEST_F(HatsWebDialogBrowserTest, LoadFailureInPreloading) {
  base::FilePath test_data_dir;
  base::PathService::Get(chrome::DIR_TEST_DATA, &test_data_dir);
  std::string contents;
  {
    base::ScopedAllowBlockingForTesting allow_blocking;
    EXPECT_TRUE(base::ReadFileToString(
        test_data_dir.AppendASCII("hats").AppendASCII("with_inline_js.html"),
        &contents));
  }

  ASSERT_TRUE(embedded_test_server()->Start());

  constexpr char kJSPath[] = "/hats/nonexistent.js";
  constexpr char kSrcPlaceholder[] = "$JS_SRC";
  GURL url = embedded_test_server()->GetURL(kJSPath);
  size_t pos = contents.find(kSrcPlaceholder);
  EXPECT_NE(pos, std::string::npos);
  contents.replace(pos, strlen(kSrcPlaceholder), url.spec());

  TestHatsWebDialog* dialog =
      Create(browser(), base::TimeDelta::FromSeconds(100),
             GURL("data:text/html;charset=utf-8," + contents));
  dialog->set_resource_url(url);
  base::RunLoop run_loop;
  EXPECT_CALL(*dialog, OnWebContentsFinishedLoad)
      .WillOnce(testing::Invoke([dialog, &run_loop]() {
        EXPECT_FALSE(dialog->resource_loaded());
        run_loop.Quit();
      }));
  run_loop.Run();
}

// Test cookies aren't blocked.
IN_PROC_BROWSER_TEST_F(HatsWebDialogBrowserTest, Cookies) {
  auto* settings_map =
      HostContentSettingsMapFactory::GetForProfile(browser()->profile());
  settings_map->SetDefaultContentSetting(ContentSettingsType::COOKIES,
                                         CONTENT_SETTING_BLOCK);

  TestHatsWebDialog* dialog =
      Create(browser(), base::TimeDelta::FromSeconds(100));

  settings_map = HostContentSettingsMapFactory::GetForProfile(
      dialog->otr_profile_for_testing());
  GURL url1("https://survey.google.com/");
  GURL url2("https://survey.g.doubleclick.net/");
  EXPECT_EQ(CONTENT_SETTING_ALLOW,
            settings_map->GetContentSetting(
                url1, url1, ContentSettingsType::COOKIES, std::string()));
  EXPECT_EQ(CONTENT_SETTING_ALLOW,
            settings_map->GetContentSetting(
                url2, url2, ContentSettingsType::COOKIES, std::string()));
}

class MockHatsNextWebDialog : public HatsNextWebDialog {
 public:
  MockHatsNextWebDialog(Browser* browser,
                        const std::string& trigger_id,
                        const GURL& hats_survey_url,
                        const base::TimeDelta& timeout)
      : HatsNextWebDialog(browser, trigger_id, hats_survey_url, timeout) {}

  MOCK_METHOD0(ShowWidget, void());
  MOCK_METHOD0(CloseWidget, void());
  MOCK_METHOD1(UpdateWidgetSize, void(gfx::Size));

  void WaitForClose() {
    base::RunLoop run_loop;
    EXPECT_CALL(*this, CloseWidget).WillOnce([&]() {
      widget_->Close();
      run_loop.Quit();
    });
    run_loop.Run();
  }
};

class HatsNextWebDialogBrowserTest : public InProcessBrowserTest {
 public:
  HatsNextWebDialogBrowserTest() {
    feature_list_.InitAndEnableFeature(
        features::kHappinessTrackingSurveysForDesktopMigration);
  }

  void SetUpOnMainThread() override {
    hats_service_ = static_cast<MockHatsService*>(
        HatsServiceFactory::GetInstance()->SetTestingFactoryAndUse(
            browser()->profile(), base::BindRepeating(&BuildMockHatsService)));
  }

  MockHatsService* hats_service() { return hats_service_; }

 private:
  base::test::ScopedFeatureList feature_list_;
  MockHatsService* hats_service_;
};

// Test that the web dialog correctly receives change to history state that
// indicates a survey is ready to be shown.
IN_PROC_BROWSER_TEST_F(HatsNextWebDialogBrowserTest, SurveyLoaded) {
  ASSERT_TRUE(embedded_test_server()->Start());

  // Use the preference path constants defined in hats_service.cc.
  const std::string kLastSurveyStartedTime =
      std::string(kHatsSurveyTriggerTesting) + ".last_survey_started_time";
  const std::string kLastMajorVersion =
      std::string(kHatsSurveyTriggerTesting) + ".last_major_version";

  auto* dialog = new MockHatsNextWebDialog(
      browser(), kHatsNextSurveyTriggerIDTesting,
      embedded_test_server()->GetURL("/hats/hats_next_mock.html"),
      base::TimeDelta::FromSeconds(100));

  // Check that no record of a survey being shown is present.
  const base::DictionaryValue* pref_data =
      browser()->profile()->GetPrefs()->GetDictionary(
          prefs::kHatsSurveyMetadata);
  base::Optional<base::Time> last_survey_started_time =
      util::ValueToTime(pref_data->FindPath(kLastSurveyStartedTime));
  base::Optional<int> last_major_version =
      pref_data->FindIntPath(kLastMajorVersion);
  ASSERT_FALSE(last_survey_started_time.has_value());
  ASSERT_FALSE(last_major_version.has_value());

  // The hats_next_mock.html will provide a state update to the dialog to
  // indicate that the survey has been loaded.
  base::RunLoop run_loop;
  EXPECT_CALL(*dialog, ShowWidget)
      .WillOnce(testing::Invoke([dialog, &run_loop]() {
        EXPECT_FALSE(dialog->IsWaitingForSurveyForTesting());
        run_loop.Quit();
      }));
  run_loop.Run();

  // Check that a record of the survey being shown has been recorded.
  pref_data = browser()->profile()->GetPrefs()->GetDictionary(
      prefs::kHatsSurveyMetadata);
  last_survey_started_time =
      util::ValueToTime(pref_data->FindPath(kLastSurveyStartedTime));
  last_major_version = pref_data->FindIntPath(kLastMajorVersion);
  ASSERT_TRUE(last_survey_started_time.has_value());
  ASSERT_TRUE(last_major_version.has_value());
  ASSERT_EQ(static_cast<uint32_t>(*last_major_version),
            version_info::GetVersion().components()[0]);
}

// Test that the web dialog correctly receives change to history state that
// indicates the survey window should be closed.
IN_PROC_BROWSER_TEST_F(HatsNextWebDialogBrowserTest, SurveyClosed) {
  ASSERT_TRUE(embedded_test_server()->Start());

  EXPECT_CALL(*hats_service(), HatsNextDialogClosed);
  auto* dialog = new MockHatsNextWebDialog(
      browser(), "close_for_testing",
      embedded_test_server()->GetURL("/hats/hats_next_mock.html"),
      base::TimeDelta::FromSeconds(100));

  // The hats_next_mock.html will provide a state update to the dialog to
  // indicate that the survey window should be closed.
  dialog->WaitForClose();
}

// Test that if the survey does not indicate it is ready for display before the
// timeout the widget is closed.
IN_PROC_BROWSER_TEST_F(HatsNextWebDialogBrowserTest, SurveyTimeout) {
  ASSERT_TRUE(embedded_test_server()->Start());

  EXPECT_CALL(*hats_service(), HatsNextDialogClosed);
  auto* dialog = new MockHatsNextWebDialog(
      browser(), "invalid_test",
      embedded_test_server()->GetURL("/hats/non_existent.html"),
      base::TimeDelta::FromMilliseconds(1));

  dialog->WaitForClose();
}

IN_PROC_BROWSER_TEST_F(HatsNextWebDialogBrowserTest, UnknownURLFragment) {
  ASSERT_TRUE(embedded_test_server()->Start());

  // Check that providing an unknown URL fragment results in the dialog being
  // closed.
  EXPECT_CALL(*hats_service(), HatsNextDialogClosed);
  auto* dialog = new MockHatsNextWebDialog(
      browser(), "invalid_url_fragment_for_testing",
      embedded_test_server()->GetURL("/hats/hats_next_mock.html"),
      base::TimeDelta::FromSeconds(100));

  dialog->WaitForClose();
}

IN_PROC_BROWSER_TEST_F(HatsNextWebDialogBrowserTest, NewWebContents) {
  ASSERT_TRUE(embedded_test_server()->Start());

  auto* dialog = new MockHatsNextWebDialog(
      browser(), "open_new_web_contents_for_testing",
      embedded_test_server()->GetURL("/hats/hats_next_mock.html"),
      base::TimeDelta::FromSeconds(100));

  // The mock hats dialog will push a close state after it has attempted to
  // open another web contents.
  EXPECT_CALL(*hats_service(), HatsNextDialogClosed);
  dialog->WaitForClose();

  // Check that a tab with http://foo.com (defined in hats_next_mock.html) has
  // been opened in the regular browser and is active.
  EXPECT_EQ(
      GURL("http://foo.com"),
      browser()->tab_strip_model()->GetActiveWebContents()->GetVisibleURL());
}

IN_PROC_BROWSER_TEST_F(HatsNextWebDialogBrowserTest, DialogResize) {
  ASSERT_TRUE(embedded_test_server()->Start());

  auto* dialog = new MockHatsNextWebDialog(
      browser(), "resize_for_testing",
      embedded_test_server()->GetURL("/hats/hats_next_mock.html"),
      base::TimeDelta::FromSeconds(100));

  // Check that the dialog attempts to resize with the sizes defined in
  // hats_next_mock.html.
  base::RunLoop run_loop;
  EXPECT_CALL(*dialog, UpdateWidgetSize(gfx::Size(123, 456)))
      .WillOnce(testing::Invoke([&run_loop] { run_loop.Quit(); }));
  run_loop.Run();
}

IN_PROC_BROWSER_TEST_F(HatsNextWebDialogBrowserTest, InvalidSize) {
  ASSERT_TRUE(embedded_test_server()->Start());

  // Check that providing a size which is too large results in the dialog being
  // closed.
  EXPECT_CALL(*hats_service(), HatsNextDialogClosed);
  auto* dialog = new MockHatsNextWebDialog(
      browser(), "invalid_size_for_testing",
      embedded_test_server()->GetURL("/hats/hats_next_mock.html"),
      base::TimeDelta::FromSeconds(100));

  dialog->WaitForClose();
}
