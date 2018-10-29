// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/views/page_info/page_info_bubble_view.h"

#include "base/macros.h"
#include "base/strings/utf_string_conversions.h"
#include "build/build_config.h"
#include "chrome/browser/content_settings/host_content_settings_map_factory.h"
#include "chrome/browser/ui/exclusive_access/exclusive_access_manager.h"
#include "chrome/browser/ui/views/chrome_layout_provider.h"
#include "chrome/browser/ui/views/hover_button.h"
#include "chrome/browser/ui/views/page_info/chosen_object_view.h"
#include "chrome/browser/ui/views/page_info/permission_selector_row.h"
#include "chrome/browser/usb/usb_chooser_context.h"
#include "chrome/browser/usb/usb_chooser_context_factory.h"
#include "chrome/test/base/testing_profile.h"
#include "components/content_settings/core/browser/host_content_settings_map.h"
#include "components/strings/grit/components_strings.h"
#include "content/public/browser/ssl_status.h"
#include "content/public/test/test_browser_thread_bundle.h"
#include "content/public/test/test_web_contents_factory.h"
#include "device/usb/public/cpp/fake_usb_device_manager.h"
#include "device/usb/public/mojom/device.mojom.h"
#include "ppapi/buildflags/buildflags.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "ui/base/l10n/l10n_util.h"
#include "ui/base/ui_base_features.h"
#include "ui/events/event_utils.h"
#include "ui/views/controls/combobox/combobox.h"
#include "ui/views/controls/label.h"
#include "ui/views/controls/link.h"
#include "ui/views/test/scoped_views_test_helper.h"
#include "ui/views/test/test_views_delegate.h"

#if BUILDFLAG(ENABLE_PLUGINS)
#include "chrome/browser/plugins/chrome_plugin_service_filter.h"
#endif

const char* kUrl = "http://www.example.com/index.html";

namespace test {

class PageInfoBubbleViewTestApi {
 public:
  PageInfoBubbleViewTestApi(gfx::NativeView parent,
                            Profile* profile,
                            content::WebContents* web_contents)
      : view_(nullptr),
        parent_(parent),
        profile_(profile),
        web_contents_(web_contents) {
    CreateView();
  }

  void CreateView() {
    if (view_) {
      view_->GetWidget()->CloseNow();
    }

    security_state::SecurityInfo security_info;
    views::View* anchor_view = nullptr;
    view_ = new PageInfoBubbleView(anchor_view, gfx::Rect(), parent_, profile_,
                                   web_contents_, GURL(kUrl), security_info);
  }

  PageInfoBubbleView* view() { return view_; }
  views::View* permissions_view() { return view_->permissions_view_; }

  PermissionSelectorRow* GetPermissionSelectorAt(int index) {
    return view_->selector_rows_[index].get();
  }

  // Returns the number of cookies shown on the link or button to open the
  // collected cookies dialog. This should always be shown.
  base::string16 GetCookiesLinkText() {
      EXPECT_TRUE(view_->cookie_button_);
      ui::AXNodeData data;
      view_->cookie_button_->GetAccessibleNodeData(&data);
      std::string name;
      data.GetStringAttribute(ax::mojom::StringAttribute::kName, &name);
      return base::ASCIIToUTF16(name);
  }

  base::string16 GetPermissionLabelTextAt(int index) {
    return GetPermissionSelectorAt(index)->label_->text();
  }

  base::string16 GetPermissionComboboxTextAt(int index) {
    auto* combobox = GetPermissionSelectorAt(index)->combobox_;
    return combobox->GetTextForRow(combobox->GetSelectedRow());
  }

  void SimulateUserSelectingComboboxItemAt(int selector_index, int menu_index) {
    auto* combobox = GetPermissionSelectorAt(selector_index)->combobox_;
    combobox->SetSelectedRow(menu_index);
  }

  // Simulates updating the number of cookies.
  void SetCookieInfo(const CookieInfoList& list) { view_->SetCookieInfo(list); }

  // Simulates recreating the dialog with a new PermissionInfoList.
  void SetPermissionInfo(const PermissionInfoList& list) {
    for (const PageInfoBubbleView::PermissionInfo& info : list) {
      view_->presenter_->OnSitePermissionChanged(info.type, info.setting);
    }
    CreateView();
  }

 private:
  PageInfoBubbleView* view_;  // Weak. Owned by its Widget.

  // For recreating the view.
  gfx::NativeView parent_;
  Profile* profile_;
  content::WebContents* web_contents_;

  DISALLOW_COPY_AND_ASSIGN(PageInfoBubbleViewTestApi);
};

}  // namespace test

namespace {

// Helper class that wraps a TestingProfile and a TestWebContents for a test
// harness. Inspired by RenderViewHostTestHarness, but doesn't use inheritance
// so the helper can be composed with other helpers in the test harness.
class ScopedWebContentsTestHelper {
 public:
  ScopedWebContentsTestHelper() {
    web_contents_ = factory_.CreateWebContents(&profile_);
  }

  Profile* profile() { return &profile_; }
  content::WebContents* web_contents() { return web_contents_; }

 private:
  content::TestBrowserThreadBundle thread_bundle_;
  TestingProfile profile_;
  content::TestWebContentsFactory factory_;
  content::WebContents* web_contents_;  // Weak. Owned by factory_.

  DISALLOW_COPY_AND_ASSIGN(ScopedWebContentsTestHelper);
};

class PageInfoBubbleViewTest : public testing::Test {
 public:
  PageInfoBubbleViewTest() {}

  // testing::Test:
  void SetUp() override {
    views_helper_.test_views_delegate()->set_layout_provider(
        ChromeLayoutProvider::CreateLayoutProvider());
    views::Widget::InitParams parent_params;
    parent_params.context = views_helper_.GetContext();
    parent_window_ = new views::Widget();
    parent_window_->Init(parent_params);

    content::WebContents* web_contents = web_contents_helper_.web_contents();
    TabSpecificContentSettings::CreateForWebContents(web_contents);
    api_.reset(new test::PageInfoBubbleViewTestApi(
        parent_window_->GetNativeView(), web_contents_helper_.profile(),
        web_contents));
  }

  void TearDown() override { parent_window_->CloseNow(); }

 protected:
  ScopedWebContentsTestHelper web_contents_helper_;
  views::ScopedViewsTestHelper views_helper_;

  views::Widget* parent_window_ = nullptr;  // Weak. Owned by the NativeWidget.
  std::unique_ptr<test::PageInfoBubbleViewTestApi> api_;

 private:
  DISALLOW_COPY_AND_ASSIGN(PageInfoBubbleViewTest);
};

#if BUILDFLAG(ENABLE_PLUGINS)
// Waits until a change is observed in content settings.
class FlashContentSettingsChangeWaiter : public content_settings::Observer {
 public:
  explicit FlashContentSettingsChangeWaiter(Profile* profile)
      : profile_(profile) {
    HostContentSettingsMapFactory::GetForProfile(profile)->AddObserver(this);
  }
  ~FlashContentSettingsChangeWaiter() override {
    HostContentSettingsMapFactory::GetForProfile(profile_)->RemoveObserver(
        this);
  }

  // content_settings::Observer:
  void OnContentSettingChanged(
      const ContentSettingsPattern& primary_pattern,
      const ContentSettingsPattern& secondary_pattern,
      ContentSettingsType content_type,
      const std::string& resource_identifier) override {
    if (content_type == CONTENT_SETTINGS_TYPE_PLUGINS)
      Proceed();
  }

  void Wait() { run_loop_.Run(); }

 private:
  void Proceed() { run_loop_.Quit(); }

  Profile* profile_;
  base::RunLoop run_loop_;

  DISALLOW_COPY_AND_ASSIGN(FlashContentSettingsChangeWaiter);
};
#endif

}  // namespace

// Each permission selector row is like this: [icon] [label] [selector]
constexpr int kViewsPerPermissionRow = 3;

// Test UI construction and reconstruction via
// PageInfoBubbleView::SetPermissionInfo().
TEST_F(PageInfoBubbleViewTest, SetPermissionInfo) {
  // This test exercises PermissionSelectorRow in a way that it is not used in
  // practice. In practice, every setting in PermissionSelectorRow starts off
  // "set", so there is always one option checked in the resulting MenuModel.
  // This test creates settings that are left at their defaults, leading to zero
  // checked options, and checks that the text on the MenuButtons is right.

  PermissionInfoList list(1);
  list.back().type = CONTENT_SETTINGS_TYPE_GEOLOCATION;
  list.back().source = content_settings::SETTING_SOURCE_USER;
  list.back().is_incognito = false;
  list.back().setting = CONTENT_SETTING_BLOCK;

  // Initially, no permissions are shown because they are all set to default.
  int num_expected_children = 0;
  EXPECT_EQ(num_expected_children, api_->permissions_view()->child_count());

  num_expected_children += kViewsPerPermissionRow * list.size();
  list.back().setting = CONTENT_SETTING_ALLOW;
  api_->SetPermissionInfo(list);
  EXPECT_EQ(num_expected_children, api_->permissions_view()->child_count());

  PermissionSelectorRow* selector = api_->GetPermissionSelectorAt(0);
  EXPECT_TRUE(selector);

  // Verify labels match the settings on the PermissionInfoList.
  EXPECT_EQ(base::ASCIIToUTF16("Location"), api_->GetPermissionLabelTextAt(0));
  EXPECT_EQ(base::ASCIIToUTF16("Allow"), api_->GetPermissionComboboxTextAt(0));

  // Verify calling SetPermissionInfo() directly updates the UI.
  list.back().setting = CONTENT_SETTING_BLOCK;
  api_->SetPermissionInfo(list);
  EXPECT_EQ(base::ASCIIToUTF16("Block"), api_->GetPermissionComboboxTextAt(0));

  // Simulate a user selection via the UI. Note this will also cover logic in
  // PageInfo to update the pref.
  api_->SimulateUserSelectingComboboxItemAt(0, 1);
  EXPECT_EQ(num_expected_children, api_->permissions_view()->child_count());
  EXPECT_EQ(base::ASCIIToUTF16("Allow"), api_->GetPermissionComboboxTextAt(0));

  // Setting to the default via the UI should keep the button around.
  api_->SimulateUserSelectingComboboxItemAt(0, 0);
  EXPECT_EQ(base::ASCIIToUTF16("Ask (default)"),
            api_->GetPermissionComboboxTextAt(0));
  EXPECT_EQ(num_expected_children, api_->permissions_view()->child_count());

  // However, since the setting is now default, recreating the dialog with those
  // settings should omit the permission from the UI.
  //
  // TODO(https://crbug.com/829576): Reconcile the comment above with the fact
  // that |num_expected_children| is not, at this point, 0 and therefore the
  // permission is not being omitted from the UI.
  api_->SetPermissionInfo(list);
  EXPECT_EQ(num_expected_children, api_->permissions_view()->child_count());
}

// Test UI construction and reconstruction with USB devices.
TEST_F(PageInfoBubbleViewTest, SetPermissionInfoWithUsbDevice) {
  const int kExpectedChildren = 0;
  EXPECT_EQ(kExpectedChildren, api_->permissions_view()->child_count());

  const GURL origin = GURL(kUrl).GetOrigin();

  // Connect the UsbChooserContext with FakeUsbDeviceManager.
  device::FakeUsbDeviceManager usb_device_manager;
  device::mojom::UsbDeviceManagerPtr device_manager_ptr;
  usb_device_manager.AddBinding(mojo::MakeRequest(&device_manager_ptr));
  UsbChooserContext* store =
      UsbChooserContextFactory::GetForProfile(web_contents_helper_.profile());
  store->SetDeviceManagerForTesting(std::move(device_manager_ptr));

  auto device_info = usb_device_manager.CreateAndAddDevice(
      0, 0, "Google", "Gizmo", "1234567890");
  store->GrantDevicePermission(origin, origin, *device_info);

  PermissionInfoList list;
  api_->SetPermissionInfo(list);
  EXPECT_EQ(kExpectedChildren + 1, api_->permissions_view()->child_count());

  ChosenObjectView* object_view = static_cast<ChosenObjectView*>(
      api_->permissions_view()->child_at(kExpectedChildren));
  EXPECT_EQ(4, object_view->child_count());

  const int kLabelIndex = 1;
  views::Label* label =
      static_cast<views::Label*>(object_view->child_at(kLabelIndex));
  EXPECT_EQ(base::ASCIIToUTF16("Gizmo"), label->text());

  const int kButtonIndex = 2;
  views::Button* button =
      static_cast<views::Button*>(object_view->child_at(kButtonIndex));

  const ui::MouseEvent event(ui::ET_MOUSE_PRESSED, gfx::Point(), gfx::Point(),
                             ui::EventTimeForNow(), 0, 0);
  views::ButtonListener* button_listener =
      static_cast<views::ButtonListener*>(object_view);
  button_listener->ButtonPressed(button, event);
  api_->SetPermissionInfo(list);
  EXPECT_EQ(kExpectedChildren, api_->permissions_view()->child_count());
  EXPECT_FALSE(store->HasDevicePermission(origin, origin, *device_info));
}

TEST_F(PageInfoBubbleViewTest, SetPermissionInfoForUsbGuard) {
  // This test exercises PermissionSelectorRow in a way that it is not used in
  // practice. In practice, every setting in PermissionSelectorRow starts off
  // "set", so there is always one option checked in the resulting MenuModel.
  // This test creates settings that are left at their defaults, leading to zero
  // checked options, and checks that the text on the MenuButtons is right.
  PermissionInfoList list(1);
  list.back().type = CONTENT_SETTINGS_TYPE_USB_GUARD;
  list.back().source = content_settings::SETTING_SOURCE_USER;
  list.back().is_incognito = false;
  list.back().setting = CONTENT_SETTING_ASK;

  // Initially, no permissions are shown because they are all set to default.
  int num_expected_children = 0;
  EXPECT_EQ(num_expected_children, api_->permissions_view()->child_count());

  // Verify calling SetPermissionInfo() directly updates the UI.
  num_expected_children += kViewsPerPermissionRow * list.size();
  list.back().setting = CONTENT_SETTING_BLOCK;
  api_->SetPermissionInfo(list);
  EXPECT_EQ(base::ASCIIToUTF16("Block"), api_->GetPermissionComboboxTextAt(0));

  // Simulate a user selection via the UI. Note this will also cover logic in
  // PageInfo to update the pref.
  api_->SimulateUserSelectingComboboxItemAt(0, 2);
  EXPECT_EQ(num_expected_children, api_->permissions_view()->child_count());
  EXPECT_EQ(base::ASCIIToUTF16("Ask"), api_->GetPermissionComboboxTextAt(0));

  // Setting to the default via the UI should keep the button around.
  api_->SimulateUserSelectingComboboxItemAt(0, 0);
  EXPECT_EQ(base::ASCIIToUTF16("Ask (default)"),
            api_->GetPermissionComboboxTextAt(0));
  EXPECT_EQ(num_expected_children, api_->permissions_view()->child_count());

  // However, since the setting is now default, recreating the dialog with
  // those settings should omit the permission from the UI.
  //
  // TODO(https://crbug.com/829576): Reconcile the comment above with the fact
  // that |num_expected_children| is not, at this point, 0 and therefore the
  // permission is not being omitted from the UI.
  api_->SetPermissionInfo(list);
  EXPECT_EQ(num_expected_children, api_->permissions_view()->child_count());
}

// Test that updating the number of cookies used by the current page doesn't add
// any extra views to Page Info.
TEST_F(PageInfoBubbleViewTest, UpdatingSiteDataRetainsLayout) {
  const int kExpectedChildren = 5;
  EXPECT_EQ(kExpectedChildren, api_->view()->child_count());

  // Create a fake list of cookies.
  PageInfoUI::CookieInfo first_party_cookies;
  first_party_cookies.allowed = 10;
  first_party_cookies.blocked = 0;
  first_party_cookies.is_first_party = true;

  PageInfoUI::CookieInfo third_party_cookies;
  third_party_cookies.allowed = 6;
  third_party_cookies.blocked = 32;
  third_party_cookies.is_first_party = false;

  const CookieInfoList cookies = {first_party_cookies, third_party_cookies};

  // Update the number of cookies.
  api_->SetCookieInfo(cookies);
  EXPECT_EQ(kExpectedChildren, api_->view()->child_count());

  // Check the number of cookies shown is correct.
  base::string16 expected = l10n_util::GetPluralStringFUTF16(
      IDS_PAGE_INFO_NUM_COOKIES,
      first_party_cookies.allowed + third_party_cookies.allowed);
  size_t index = api_->GetCookiesLinkText().find(expected);
  EXPECT_NE(std::string::npos, index);
}

#if BUILDFLAG(ENABLE_PLUGINS)
TEST_F(PageInfoBubbleViewTest, ChangingFlashSettingForSiteIsRemembered) {
  Profile* profile = web_contents_helper_.profile();
  ChromePluginServiceFilter::GetInstance()->RegisterResourceContext(
      profile, profile->GetResourceContext());
  FlashContentSettingsChangeWaiter waiter(profile);

  const GURL url(kUrl);
  HostContentSettingsMap* map =
      HostContentSettingsMapFactory::GetForProfile(profile);
  // Make sure the site being tested doesn't already have this marker set.
  EXPECT_EQ(nullptr,
            map->GetWebsiteSetting(url, url, CONTENT_SETTINGS_TYPE_PLUGINS_DATA,
                                   std::string(), nullptr));
  EXPECT_EQ(0, api_->permissions_view()->child_count());

  // Change the Flash setting.
  map->SetContentSettingDefaultScope(url, url, CONTENT_SETTINGS_TYPE_PLUGINS,
                                     std::string(), CONTENT_SETTING_ALLOW);
  waiter.Wait();

  // Check that this site has now been marked for displaying Flash always.
  EXPECT_NE(nullptr,
            map->GetWebsiteSetting(url, url, CONTENT_SETTINGS_TYPE_PLUGINS_DATA,
                                   std::string(), nullptr));

  // Check the Flash permission is now showing since it's non-default.
  api_->CreateView();
  const int kPermissionLabelIndex = 1;
  views::Label* label = static_cast<views::Label*>(
      api_->permissions_view()->child_at(kPermissionLabelIndex));
  EXPECT_EQ(base::ASCIIToUTF16("Flash"), label->text());

  // Change the Flash setting back to the default.
  map->SetContentSettingDefaultScope(url, url, CONTENT_SETTINGS_TYPE_PLUGINS,
                                     std::string(), CONTENT_SETTING_DEFAULT);
  EXPECT_EQ(kViewsPerPermissionRow, api_->permissions_view()->child_count());

  // Check the Flash permission is still showing since the user changed it
  // previously.
  label = static_cast<views::Label*>(
      api_->permissions_view()->child_at(kPermissionLabelIndex));
  EXPECT_EQ(base::ASCIIToUTF16("Flash"), label->text());
}
#endif
