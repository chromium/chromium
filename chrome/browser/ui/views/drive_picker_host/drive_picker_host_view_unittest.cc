// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/views/drive_picker_host/drive_picker_host_view.h"

#include "chrome/browser/ui/browser_window/test/mock_browser_window_interface.h"
#include "chrome/browser/ui/webui/drive_picker_host/drive_picker_host_ui.h"
#include "chrome/common/webui_url_constants.h"
#include "chrome/test/base/testing_profile.h"
#include "chrome/test/views/chrome_views_test_base.h"
#include "components/omnibox/common/omnibox_features.h"
#include "content/public/browser/web_contents.h"
#include "content/public/browser/web_ui.h"
#include "content/public/test/navigation_simulator.h"
#include "content/public/test/scoped_web_ui_controller_factory_registration.h"
#include "content/public/test/test_renderer_host.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "ui/base/accelerators/accelerator.h"
#include "ui/views/controls/webview/webview.h"
#include "ui/views/view_utils.h"
#include "ui/views/widget/widget.h"
#include "url/gurl.h"

namespace {

class MockDrivePickerHostUI : public DrivePickerHostUI {
 public:
  explicit MockDrivePickerHostUI(content::WebUI* web_ui)
      : DrivePickerHostUI(web_ui) {}
  MOCK_METHOD(void,
              TriggerDrivePickerHost,
              (std::unique_ptr<drive_picker_host::DrivePickerHostRequest>),
              (override));
};

class MockDrivePickerHostUIConfig : public DrivePickerHostUIConfig {
 public:
  MockDrivePickerHostUIConfig() = default;

  bool IsWebUIEnabled(content::BrowserContext* browser_context) override {
    return true;
  }

  std::unique_ptr<content::WebUIController> CreateWebUIController(
      content::WebUI* web_ui,
      const GURL& url) override {
    return std::make_unique<MockDrivePickerHostUI>(web_ui);
  }
};

}  // namespace

class DrivePickerHostViewTest : public ChromeViewsTestBase {
 public:
  DrivePickerHostViewTest() = default;
  ~DrivePickerHostViewTest() override = default;

  void SetUp() override {
    ChromeViewsTestBase::SetUp();
    profile_ = std::make_unique<TestingProfile>();
  }

  void TearDown() override {
    profile_.reset();
    ChromeViewsTestBase::TearDown();
  }

  TestingProfile* profile() { return profile_.get(); }
  MockBrowserWindowInterface* browser_window_interface() {
    return &mock_browser_window_interface_;
  }

 private:
  std::unique_ptr<TestingProfile> profile_;
  MockBrowserWindowInterface mock_browser_window_interface_;

  // Needed because DrivePickerHostView creates a views::WebView and
  // TriggerDrivePickerHostUi test uses NavigationSimulator to "load" the
  // WebUI URL. This requires creating a RenderViewHost and a RenderFrameHost.
  // Without the test enabler, the code attempts to "launch" a real process and
  // hits a DCHECK or SEGV in ChildProcessLauncherHelper because the environment
  // isn't set up for it.
  content::RenderViewHostTestEnabler rvh_test_enabler_;
};

TEST_F(DrivePickerHostViewTest, Initialization) {
  auto view = std::make_unique<DrivePickerHostView>(profile(),
                                                    browser_window_interface());

  EXPECT_EQ(view->children().size(), 1u);
  EXPECT_FALSE(view->GetBackground());
}

TEST_F(DrivePickerHostViewTest, TriggerDrivePickerHostUi) {
  base::test::ScopedFeatureList feature_list;
  feature_list.InitAndEnableFeature(omnibox::kComposeboxDriveContextMenuOption);

  content::ScopedWebUIConfigRegistration registration(
      std::make_unique<MockDrivePickerHostUIConfig>());

  auto view = std::make_unique<DrivePickerHostView>(profile(),
                                                    browser_window_interface());

  content::WebContents* contents =
      views::AsViewClass<views::WebView>(view->view_tracker_.view())
          ->GetWebContents();

  auto simulator = content::NavigationSimulator::CreateBrowserInitiated(
      GURL(chrome::kChromeUIDrivePickerHostURL), contents);
  simulator->Commit();

  auto* mock_controller =
      contents->GetWebUI()->GetController()->GetAs<MockDrivePickerHostUI>();
  ASSERT_TRUE(mock_controller);
  EXPECT_CALL(*mock_controller, TriggerDrivePickerHost(testing::_));
  auto request = std::make_unique<drive_picker_host::DrivePickerHostRequest>(
      drive_picker_host::DrivePickerHostRequest::RequestType::kPickerUi,
      mojo::PendingRemote<
          drive_picker_host::mojom::DrivePickerResultHandler>());
  view->TriggerDrivePickerHostUi(std::move(request));

  // Clean up the view to avoid dangling references.
  view.reset();
}

TEST_F(DrivePickerHostViewTest, EscapeAcceleratorClosesWidget) {
  auto view = std::make_unique<DrivePickerHostView>(profile(),
                                                    browser_window_interface());

  auto widget = std::make_unique<views::Widget>();
  views::Widget::InitParams params(
      views::Widget::InitParams::CLIENT_OWNS_WIDGET,
      views::Widget::InitParams::TYPE_WINDOW_FRAMELESS);
  params.context = GetContext();
  widget->Init(std::move(params));
  views::View* view_ptr = widget->SetContentsView(std::move(view));
  widget->Show();

  EXPECT_FALSE(widget->IsClosed());

  ui::Accelerator escape_accelerator(ui::VKEY_ESCAPE, ui::EF_NONE);
  view_ptr->AcceleratorPressed(escape_accelerator);

  EXPECT_TRUE(widget->IsClosed());
}
