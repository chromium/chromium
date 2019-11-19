// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/views/page_info/page_info_bubble_view.h"

#include "base/json/json_reader.h"
#include "base/macros.h"
#include "base/strings/utf_string_conversions.h"
#include "build/build_config.h"
#include "chrome/browser/content_settings/host_content_settings_map_factory.h"
#include "chrome/browser/history/history_service_factory.h"
#include "chrome/browser/permissions/permission_uma_util.h"
#include "chrome/browser/ssl/security_state_tab_helper.h"
#include "chrome/browser/ui/exclusive_access/exclusive_access_manager.h"
#include "chrome/browser/ui/views/chrome_layout_provider.h"
#include "chrome/browser/ui/views/hover_button.h"
#include "chrome/browser/ui/views/page_info/chosen_object_view.h"
#include "chrome/browser/ui/views/page_info/page_info_hover_button.h"
#include "chrome/browser/ui/views/page_info/permission_selector_row.h"
#include "chrome/browser/usb/usb_chooser_context.h"
#include "chrome/browser/usb/usb_chooser_context_factory.h"
#include "chrome/test/base/testing_profile.h"
#include "components/content_settings/core/browser/host_content_settings_map.h"
#include "components/content_settings/core/common/pref_names.h"
#include "components/history/core/browser/history_service.h"
#include "components/strings/grit/components_strings.h"
#include "components/ukm/test_ukm_recorder.h"
#include "content/public/browser/ssl_status.h"
#include "content/public/test/browser_side_navigation_test_utils.h"
#include "content/public/test/browser_task_environment.h"
#include "content/public/test/navigation_simulator.h"
#include "content/public/test/test_web_contents_factory.h"
#include "mojo/public/cpp/bindings/pending_remote.h"
#include "net/cert/cert_status_flags.h"
#include "net/ssl/ssl_connection_status_flags.h"
#include "net/test/cert_test_util.h"
#include "net/test/test_certificate_data.h"
#include "net/test/test_data_directory.h"
#include "ppapi/buildflags/buildflags.h"
#include "services/device/public/cpp/test/fake_usb_device_manager.h"
#include "services/device/public/mojom/usb_device.mojom.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "ui/accessibility/ax_enums.mojom.h"
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
const char* kSecureUrl = "https://www.example.com/index.html";

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

    views::View* anchor_view = nullptr;
    view_ = new PageInfoBubbleView(
        anchor_view, gfx::Rect(), parent_, profile_, web_contents_, GURL(kUrl),
        security_state::NONE, security_state::VisibleSecurityState(),
        base::BindOnce(&PageInfoBubbleViewTestApi::OnPageInfoBubbleClosed,
                       base::Unretained(this), run_loop_.QuitClosure()));
  }

  PageInfoBubbleView* view() { return view_; }
  views::View* permissions_view() { return view_->permissions_view_; }
  bool reload_prompt() const { return *reload_prompt_; }
  views::Widget::ClosedReason closed_reason() const { return *closed_reason_; }

  base::string16 GetWindowTitle() { return view_->GetWindowTitle(); }
  PageInfoUI::SecurityDescriptionType GetSecurityDescriptionType() {
    return view_->GetSecurityDescriptionType();
  }

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
    return GetPermissionSelectorAt(index)->label_->GetText();
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

  base::string16 GetCertificateButtonSubtitleText() const {
    EXPECT_TRUE(view_->certificate_button_);
    EXPECT_TRUE(view_->certificate_button_->subtitle());
    return view_->certificate_button_->subtitle()->GetText();
  }

  void WaitForBubbleClose() { run_loop_.Run(); }

 private:
  void OnPageInfoBubbleClosed(base::RepeatingCallback<void()> quit_closure,
                              views::Widget::ClosedReason closed_reason,
                              bool reload_prompt) {
    closed_reason_ = closed_reason;
    reload_prompt_ = reload_prompt;
    quit_closure.Run();
  }

  PageInfoBubbleView* view_;  // Weak. Owned by its Widget.

  // For recreating the view.
  gfx::NativeView parent_;
  Profile* profile_;
  content::WebContents* web_contents_;
  base::RunLoop run_loop_;
  base::Optional<bool> reload_prompt_;
  base::Optional<views::Widget::ClosedReason> closed_reason_;

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
  content::BrowserTaskEnvironment task_environment_;
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
    content::BrowserSideNavigationSetUp();
    views_helper_.test_views_delegate()->set_layout_provider(
        ChromeLayoutProvider::CreateLayoutProvider());
    views::Widget::InitParams parent_params;
    parent_params.context = views_helper_.GetContext();
    parent_window_ = new views::Widget();
    parent_window_->Init(std::move(parent_params));

    content::WebContents* web_contents = web_contents_helper_.web_contents();
    TabSpecificContentSettings::CreateForWebContents(web_contents);
    api_ = std::make_unique<test::PageInfoBubbleViewTestApi>(
        parent_window_->GetNativeView(), web_contents_helper_.profile(),
        web_contents);
  }

  void TearDown() override {
    parent_window_->CloseNow();
    content::BrowserSideNavigationTearDown();
  }

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
    if (content_type == ContentSettingsType::PLUGINS)
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
constexpr size_t kViewsPerPermissionRow = 3;

TEST_F(PageInfoBubbleViewTest, NotificationPermissionRevokeUkm) {
  GURL origin_url = GURL(kUrl).GetOrigin();
  TestingProfile* profile =
      static_cast<TestingProfile*>(web_contents_helper_.profile());
  ukm::TestAutoSetUkmRecorder ukm_recorder;
  ASSERT_TRUE(profile->CreateHistoryService(
      /* delete_file= */ true,
      /* no_db= */ false));
  auto* history_service = HistoryServiceFactory::GetForProfile(
      profile, ServiceAccessType::EXPLICIT_ACCESS);
  history_service->AddPage(origin_url, base::Time::Now(),
                           history::SOURCE_BROWSED);
  base::RunLoop origin_queried_waiter;
  history_service->set_origin_queried_closure_for_testing(
      origin_queried_waiter.QuitClosure());

  PermissionInfoList list(1);
  list.back().type = ContentSettingsType::NOTIFICATIONS;
  list.back().source = content_settings::SETTING_SOURCE_USER;
  list.back().is_incognito = false;

  list.back().setting = CONTENT_SETTING_ALLOW;
  api_->SetPermissionInfo(list);

  list.back().setting = CONTENT_SETTING_BLOCK;
  api_->SetPermissionInfo(list);

  origin_queried_waiter.Run();

  auto entries = ukm_recorder.GetEntriesByName("Permission");
  EXPECT_EQ(1u, entries.size());
  auto* entry = entries.front();

  ukm_recorder.ExpectEntrySourceHasUrl(entry, origin_url);
  EXPECT_EQ(*ukm_recorder.GetEntryMetric(entry, "Source"),
            static_cast<int64_t>(PermissionSourceUI::OIB));
  EXPECT_EQ(*ukm_recorder.GetEntryMetric(entry, "PermissionType"),
            static_cast<int64_t>(ContentSettingsType::NOTIFICATIONS));
  EXPECT_EQ(*ukm_recorder.GetEntryMetric(entry, "Action"),
            static_cast<int64_t>(PermissionAction::REVOKED));
}

// Test UI construction and reconstruction via
// PageInfoBubbleView::SetPermissionInfo().
TEST_F(PageInfoBubbleViewTest, SetPermissionInfo) {
  // This test exercises PermissionSelectorRow in a way that it is not used in
  // practice. In practice, every setting in PermissionSelectorRow starts off
  // "set", so there is always one option checked in the resulting MenuModel.
  // This test creates settings that are left at their defaults, leading to zero
  // checked options, and checks that the text on the MenuButtons is right.

  TestingProfile* profile =
      static_cast<TestingProfile*>(web_contents_helper_.profile());
  ASSERT_TRUE(profile->CreateHistoryService(
      /* delete_file= */ true,
      /* no_db= */ false));

  PermissionInfoList list(1);
  list.back().type = ContentSettingsType::GEOLOCATION;
  list.back().source = content_settings::SETTING_SOURCE_USER;
  list.back().is_incognito = false;
  list.back().setting = CONTENT_SETTING_BLOCK;

  // Initially, no permissions are shown because they are all set to default.
  size_t num_expected_children = 0;
  EXPECT_EQ(num_expected_children, api_->permissions_view()->children().size());

  num_expected_children += kViewsPerPermissionRow * list.size();
  list.back().setting = CONTENT_SETTING_ALLOW;
  api_->SetPermissionInfo(list);
  EXPECT_EQ(num_expected_children, api_->permissions_view()->children().size());

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
  EXPECT_EQ(num_expected_children, api_->permissions_view()->children().size());
  EXPECT_EQ(base::ASCIIToUTF16("Allow"), api_->GetPermissionComboboxTextAt(0));

  // Setting to the default via the UI should keep the button around.
  api_->SimulateUserSelectingComboboxItemAt(0, 0);
  EXPECT_EQ(base::ASCIIToUTF16("Ask (default)"),
            api_->GetPermissionComboboxTextAt(0));
  EXPECT_EQ(num_expected_children, api_->permissions_view()->children().size());

  // However, since the setting is now default, recreating the dialog with those
  // settings should omit the permission from the UI.
  //
  // TODO(https://crbug.com/829576): Reconcile the comment above with the fact
  // that |num_expected_children| is not, at this point, 0 and therefore the
  // permission is not being omitted from the UI.
  api_->SetPermissionInfo(list);
  EXPECT_EQ(num_expected_children, api_->permissions_view()->children().size());
}

// Test UI construction and reconstruction with USB devices.
TEST_F(PageInfoBubbleViewTest, SetPermissionInfoWithUsbDevice) {
  constexpr size_t kExpectedChildren = 0;
  EXPECT_EQ(kExpectedChildren, api_->permissions_view()->children().size());

  const auto origin = url::Origin::Create(GURL(kUrl));

  // Connect the UsbChooserContext with FakeUsbDeviceManager.
  device::FakeUsbDeviceManager usb_device_manager;
  mojo::PendingRemote<device::mojom::UsbDeviceManager> usb_manager;
  usb_device_manager.AddReceiver(usb_manager.InitWithNewPipeAndPassReceiver());
  UsbChooserContext* store =
      UsbChooserContextFactory::GetForProfile(web_contents_helper_.profile());
  store->SetDeviceManagerForTesting(std::move(usb_manager));

  auto device_info = usb_device_manager.CreateAndAddDevice(
      0, 0, "Google", "Gizmo", "1234567890");
  store->GrantDevicePermission(origin, origin, *device_info);

  PermissionInfoList list;
  api_->SetPermissionInfo(list);
  EXPECT_EQ(kExpectedChildren + 1, api_->permissions_view()->children().size());

  ChosenObjectView* object_view = static_cast<ChosenObjectView*>(
      api_->permissions_view()->children()[kExpectedChildren]);
  const auto& children = object_view->children();
  EXPECT_EQ(4u, children.size());

  views::Label* label = static_cast<views::Label*>(children[1]);
  EXPECT_EQ(base::ASCIIToUTF16("Gizmo"), label->GetText());

  views::Button* button = static_cast<views::Button*>(children[2]);
  const ui::MouseEvent event(ui::ET_MOUSE_PRESSED, gfx::Point(), gfx::Point(),
                             ui::EventTimeForNow(), 0, 0);
  static_cast<views::ButtonListener*>(object_view)
      ->ButtonPressed(button, event);
  api_->SetPermissionInfo(list);
  EXPECT_EQ(kExpectedChildren, api_->permissions_view()->children().size());
  EXPECT_FALSE(store->HasDevicePermission(origin, origin, *device_info));
}

namespace {

constexpr char kPolicySetting[] = R"(
    [
      {
        "devices": [{ "vendor_id": 6353, "product_id": 5678 }],
        "urls": ["http://www.example.com"]
      }
    ])";

}  // namespace

// Test UI construction and reconstruction with policy USB devices.
TEST_F(PageInfoBubbleViewTest, SetPermissionInfoWithPolicyUsbDevices) {
  constexpr size_t kExpectedChildren = 0;
  EXPECT_EQ(kExpectedChildren, api_->permissions_view()->children().size());

  const auto origin = url::Origin::Create(GURL(kUrl));

  // Add the policy setting to prefs.
  Profile* profile = web_contents_helper_.profile();
  profile->GetPrefs()->Set(prefs::kManagedWebUsbAllowDevicesForUrls,
                           *base::JSONReader::ReadDeprecated(kPolicySetting));
  UsbChooserContext* store = UsbChooserContextFactory::GetForProfile(profile);

  auto objects = store->GetGrantedObjects(origin, origin);
  EXPECT_EQ(objects.size(), 1u);

  PermissionInfoList list;
  api_->SetPermissionInfo(list);
  EXPECT_EQ(kExpectedChildren + 1, api_->permissions_view()->children().size());

  ChosenObjectView* object_view = static_cast<ChosenObjectView*>(
      api_->permissions_view()->children()[kExpectedChildren]);
  const auto& children = object_view->children();
  EXPECT_EQ(4u, children.size());

  views::Label* label = static_cast<views::Label*>(children[1]);
  EXPECT_EQ(base::ASCIIToUTF16("Unknown product 0x162E from Google Inc."),
            label->GetText());

  views::Button* button = static_cast<views::Button*>(children[2]);
  EXPECT_EQ(button->state(), views::Button::STATE_DISABLED);

  views::Label* desc_label = static_cast<views::Label*>(children[3]);
  EXPECT_EQ(base::ASCIIToUTF16("USB device allowed by your administrator"),
            desc_label->GetText());

  // Policy granted USB permissions should not be able to be deleted.
  const ui::MouseEvent event(ui::ET_MOUSE_PRESSED, gfx::Point(), gfx::Point(),
                             ui::EventTimeForNow(), 0, 0);
  views::ButtonListener* button_listener =
      static_cast<views::ButtonListener*>(object_view);
  button_listener->ButtonPressed(button, event);
  api_->SetPermissionInfo(list);
  EXPECT_EQ(kExpectedChildren + 1, api_->permissions_view()->children().size());
}

// Test UI construction and reconstruction with both user and policy USB
// devices.
TEST_F(PageInfoBubbleViewTest, SetPermissionInfoWithUserAndPolicyUsbDevices) {
  constexpr size_t kExpectedChildren = 0;
  EXPECT_EQ(kExpectedChildren, api_->permissions_view()->children().size());

  const auto origin = url::Origin::Create(GURL(kUrl));

  // Add the policy setting to prefs.
  Profile* profile = web_contents_helper_.profile();
  profile->GetPrefs()->Set(prefs::kManagedWebUsbAllowDevicesForUrls,
                           *base::JSONReader::ReadDeprecated(kPolicySetting));

  // Connect the UsbChooserContext with FakeUsbDeviceManager.
  device::FakeUsbDeviceManager usb_device_manager;
  mojo::PendingRemote<device::mojom::UsbDeviceManager> device_manager;
  usb_device_manager.AddReceiver(
      device_manager.InitWithNewPipeAndPassReceiver());
  UsbChooserContext* store = UsbChooserContextFactory::GetForProfile(profile);
  store->SetDeviceManagerForTesting(std::move(device_manager));

  auto device_info = usb_device_manager.CreateAndAddDevice(
      0, 0, "Google", "Gizmo", "1234567890");
  store->GrantDevicePermission(origin, origin, *device_info);

  auto objects = store->GetGrantedObjects(origin, origin);
  EXPECT_EQ(objects.size(), 2u);

  PermissionInfoList list;
  api_->SetPermissionInfo(list);
  EXPECT_EQ(kExpectedChildren + 2, api_->permissions_view()->children().size());

  const ui::MouseEvent event(ui::ET_MOUSE_PRESSED, gfx::Point(), gfx::Point(),
                             ui::EventTimeForNow(), 0, 0);

  // The first object is the user granted permission for the "Gizmo" device.
  {
    ChosenObjectView* object_view = static_cast<ChosenObjectView*>(
        api_->permissions_view()->children()[kExpectedChildren]);
    const auto& children = object_view->children();
    EXPECT_EQ(4u, children.size());

    views::Label* label = static_cast<views::Label*>(children[1]);
    EXPECT_EQ(base::ASCIIToUTF16("Gizmo"), label->GetText());

    views::Button* button = static_cast<views::Button*>(children[2]);
    EXPECT_NE(button->state(), views::Button::STATE_DISABLED);

    views::Label* desc_label = static_cast<views::Label*>(children[3]);
    EXPECT_EQ(base::ASCIIToUTF16("USB device"), desc_label->GetText());

    views::ButtonListener* button_listener =
        static_cast<views::ButtonListener*>(object_view);
    button_listener->ButtonPressed(button, event);
    api_->SetPermissionInfo(list);
    EXPECT_EQ(kExpectedChildren + 1,
              api_->permissions_view()->children().size());
    EXPECT_FALSE(store->HasDevicePermission(origin, origin, *device_info));
  }

  // The policy granted permission should now be the first child, since the user
  // permission was deleted.
  {
    ChosenObjectView* object_view = static_cast<ChosenObjectView*>(
        api_->permissions_view()->children()[kExpectedChildren]);
    const auto& children = object_view->children();
    EXPECT_EQ(4u, children.size());

    views::Label* label = static_cast<views::Label*>(children[1]);
    EXPECT_EQ(base::ASCIIToUTF16("Unknown product 0x162E from Google Inc."),
              label->GetText());

    views::Button* button = static_cast<views::Button*>(children[2]);
    EXPECT_EQ(button->state(), views::Button::STATE_DISABLED);

    views::Label* desc_label = static_cast<views::Label*>(children[3]);
    EXPECT_EQ(base::ASCIIToUTF16("USB device allowed by your administrator"),
              desc_label->GetText());

    views::ButtonListener* button_listener =
        static_cast<views::ButtonListener*>(object_view);
    button_listener->ButtonPressed(button, event);
    api_->SetPermissionInfo(list);
    EXPECT_EQ(kExpectedChildren + 1,
              api_->permissions_view()->children().size());
  }
}

TEST_F(PageInfoBubbleViewTest, SetPermissionInfoForUsbGuard) {
  // This test exercises PermissionSelectorRow in a way that it is not used in
  // practice. In practice, every setting in PermissionSelectorRow starts off
  // "set", so there is always one option checked in the resulting MenuModel.
  // This test creates settings that are left at their defaults, leading to zero
  // checked options, and checks that the text on the MenuButtons is right.
  PermissionInfoList list(1);
  list.back().type = ContentSettingsType::USB_GUARD;
  list.back().source = content_settings::SETTING_SOURCE_USER;
  list.back().is_incognito = false;
  list.back().setting = CONTENT_SETTING_ASK;

  // Initially, no permissions are shown because they are all set to default.
  size_t num_expected_children = 0;
  EXPECT_EQ(num_expected_children, api_->permissions_view()->children().size());

  // Verify calling SetPermissionInfo() directly updates the UI.
  num_expected_children += kViewsPerPermissionRow * list.size();
  list.back().setting = CONTENT_SETTING_BLOCK;
  api_->SetPermissionInfo(list);
  EXPECT_EQ(base::ASCIIToUTF16("Block"), api_->GetPermissionComboboxTextAt(0));

  // Simulate a user selection via the UI. Note this will also cover logic in
  // PageInfo to update the pref.
  api_->SimulateUserSelectingComboboxItemAt(0, 2);
  EXPECT_EQ(num_expected_children, api_->permissions_view()->children().size());
  EXPECT_EQ(base::ASCIIToUTF16("Ask"), api_->GetPermissionComboboxTextAt(0));

  // Setting to the default via the UI should keep the button around.
  api_->SimulateUserSelectingComboboxItemAt(0, 0);
  EXPECT_EQ(base::ASCIIToUTF16("Ask (default)"),
            api_->GetPermissionComboboxTextAt(0));
  EXPECT_EQ(num_expected_children, api_->permissions_view()->children().size());

  // However, since the setting is now default, recreating the dialog with
  // those settings should omit the permission from the UI.
  //
  // TODO(https://crbug.com/829576): Reconcile the comment above with the fact
  // that |num_expected_children| is not, at this point, 0 and therefore the
  // permission is not being omitted from the UI.
  api_->SetPermissionInfo(list);
  EXPECT_EQ(num_expected_children, api_->permissions_view()->children().size());
}

// Test that updating the number of cookies used by the current page doesn't add
// any extra views to Page Info.
TEST_F(PageInfoBubbleViewTest, UpdatingSiteDataRetainsLayout) {
#if defined(OS_WIN) && BUILDFLAG(ENABLE_VR)
  constexpr size_t kExpectedChildren = 6;
#else
  constexpr size_t kExpectedChildren = 5;
#endif

  EXPECT_EQ(kExpectedChildren, api_->view()->children().size());

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
  EXPECT_EQ(kExpectedChildren, api_->view()->children().size());

  // Check the number of cookies shown is correct.
  base::string16 expected = l10n_util::GetPluralStringFUTF16(
      IDS_PAGE_INFO_NUM_COOKIES_PARENTHESIZED,
      first_party_cookies.allowed + third_party_cookies.allowed);
  size_t index = api_->GetCookiesLinkText().find(expected);
  EXPECT_NE(std::string::npos, index);
}

#if BUILDFLAG(ENABLE_PLUGINS)
TEST_F(PageInfoBubbleViewTest, ChangingFlashSettingForSiteIsRemembered) {
  Profile* profile = web_contents_helper_.profile();
  ChromePluginServiceFilter::GetInstance()->RegisterProfile(profile);
  FlashContentSettingsChangeWaiter waiter(profile);

  const GURL url(kUrl);
  HostContentSettingsMap* map =
      HostContentSettingsMapFactory::GetForProfile(profile);
  // Make sure the site being tested doesn't already have this marker set.
  EXPECT_EQ(nullptr,
            map->GetWebsiteSetting(url, url, ContentSettingsType::PLUGINS_DATA,
                                   std::string(), nullptr));
  EXPECT_EQ(0u, api_->permissions_view()->children().size());

  // Change the Flash setting.
  map->SetContentSettingDefaultScope(url, url, ContentSettingsType::PLUGINS,
                                     std::string(), CONTENT_SETTING_ALLOW);
  waiter.Wait();

  // Check that this site has now been marked for displaying Flash always.
  EXPECT_NE(nullptr,
            map->GetWebsiteSetting(url, url, ContentSettingsType::PLUGINS_DATA,
                                   std::string(), nullptr));

  // Check the Flash permission is now showing since it's non-default.
  api_->CreateView();
  const auto& children = api_->permissions_view()->children();
  views::Label* label = static_cast<views::Label*>(children[1]);
  EXPECT_EQ(base::ASCIIToUTF16("Flash"), label->GetText());

  // Change the Flash setting back to the default.
  map->SetContentSettingDefaultScope(url, url, ContentSettingsType::PLUGINS,
                                     std::string(), CONTENT_SETTING_DEFAULT);
  EXPECT_EQ(kViewsPerPermissionRow, children.size());

  // Check the Flash permission is still showing since the user changed it
  // previously.
  label = static_cast<views::Label*>(children[1]);
  EXPECT_EQ(base::ASCIIToUTF16("Flash"), label->GetText());
}
#endif

// Tests opening the bubble between navigation start and finish. The bubble
// should be updated to reflect the secure state after the navigation commits.
TEST_F(PageInfoBubbleViewTest, OpenPageInfoBubbleAfterNavigationStart) {
  SecurityStateTabHelper::CreateForWebContents(
      web_contents_helper_.web_contents());
  std::unique_ptr<content::NavigationSimulator> navigation =
      content::NavigationSimulator::CreateRendererInitiated(
          GURL(kSecureUrl),
          web_contents_helper_.web_contents()->GetMainFrame());
  navigation->Start();
  api_->CreateView();
  EXPECT_EQ(l10n_util::GetStringUTF16(IDS_PAGE_INFO_NOT_SECURE_SUMMARY),
            api_->GetWindowTitle());
  EXPECT_EQ(PageInfoUI::SecurityDescriptionType::CONNECTION,
            api_->GetSecurityDescriptionType());

  // Set up a test SSLInfo so that Page Info sees the connection as secure.
  uint16_t cipher_suite = 0xc02f;  // TLS_ECDHE_RSA_WITH_AES_128_GCM_SHA256
  int connection_status = 0;
  net::SSLConnectionStatusSetCipherSuite(cipher_suite, &connection_status);
  net::SSLConnectionStatusSetVersion(net::SSL_CONNECTION_VERSION_TLS1_2,
                                     &connection_status);
  net::SSLInfo ssl_info;
  ssl_info.connection_status = connection_status;
  ssl_info.cert =
      net::ImportCertFromFile(net::GetTestCertsDirectory(), "ok_cert.pem");
  ASSERT_TRUE(ssl_info.cert);

  navigation->SetSSLInfo(ssl_info);

  navigation->Commit();
  EXPECT_EQ(l10n_util::GetStringUTF16(IDS_PAGE_INFO_SECURE_SUMMARY),
            api_->GetWindowTitle());
  EXPECT_EQ(PageInfoUI::SecurityDescriptionType::CONNECTION,
            api_->GetSecurityDescriptionType());
}

TEST_F(PageInfoBubbleViewTest, EnsureCloseCallback) {
  api_->view()->GetWidget()->CloseWithReason(
      views::Widget::ClosedReason::kCloseButtonClicked);
  api_->WaitForBubbleClose();
  EXPECT_EQ(false, api_->reload_prompt());
  EXPECT_EQ(views::Widget::ClosedReason::kCloseButtonClicked,
            api_->closed_reason());
}

TEST_F(PageInfoBubbleViewTest, CertificateButtonShowsEvCertDetails) {
  SecurityStateTabHelper::CreateForWebContents(
      web_contents_helper_.web_contents());
  std::unique_ptr<content::NavigationSimulator> navigation =
      content::NavigationSimulator::CreateRendererInitiated(
          GURL(kSecureUrl),
          web_contents_helper_.web_contents()->GetMainFrame());
  navigation->Start();
  api_->CreateView();

  // Set up a test SSLInfo so that Page Info sees the connection as secure and
  // using an EV certificate.
  uint16_t cipher_suite = 0xc02f;  // TLS_ECDHE_RSA_WITH_AES_128_GCM_SHA256
  int connection_status = 0;
  net::SSLConnectionStatusSetCipherSuite(cipher_suite, &connection_status);
  net::SSLConnectionStatusSetVersion(net::SSL_CONNECTION_VERSION_TLS1_2,
                                     &connection_status);
  net::SSLInfo ssl_info;
  ssl_info.connection_status = connection_status;
  ssl_info.cert = net::X509Certificate::CreateFromBytes(
      reinterpret_cast<const char*>(thawte_der), sizeof(thawte_der));
  ASSERT_TRUE(ssl_info.cert);
  ssl_info.cert_status = net::CERT_STATUS_IS_EV;

  navigation->SetSSLInfo(ssl_info);

  navigation->Commit();
  EXPECT_EQ(l10n_util::GetStringUTF16(IDS_PAGE_INFO_SECURE_SUMMARY),
            api_->GetWindowTitle());

  // The certificate button subtitle should show the EV certificate organization
  // name and country of incorporation.
  EXPECT_EQ(l10n_util::GetStringFUTF16(
                IDS_PAGE_INFO_SECURITY_TAB_SECURE_IDENTITY_EV_VERIFIED,
                base::UTF8ToUTF16("Thawte Inc"), base::UTF8ToUTF16("US")),
            api_->GetCertificateButtonSubtitleText());
}
