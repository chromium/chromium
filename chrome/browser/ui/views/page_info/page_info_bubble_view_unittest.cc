// Copyright 2015 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/views/page_info/page_info_bubble_view.h"

#include "base/memory/raw_ptr.h"
#include "base/strings/utf_string_conversions.h"
#include "base/test/metrics/histogram_tester.h"
#include "base/test/values_test_util.h"
#include "build/build_config.h"
#include "build/chromeos_buildflags.h"
#include "chrome/browser/browser_process.h"
#include "chrome/browser/content_settings/host_content_settings_map_factory.h"
#include "chrome/browser/content_settings/page_specific_content_settings_delegate.h"
#include "chrome/browser/history/history_service_factory.h"
#include "chrome/browser/privacy_sandbox/mock_privacy_sandbox_service.h"
#include "chrome/browser/privacy_sandbox/privacy_sandbox_service_factory.h"
#include "chrome/browser/ssl/chrome_security_state_tab_helper.h"
#include "chrome/browser/ui/exclusive_access/exclusive_access_manager.h"
#include "chrome/browser/ui/hats/mock_trust_safety_sentiment_service.h"
#include "chrome/browser/ui/hats/trust_safety_sentiment_service_factory.h"
#include "chrome/browser/ui/views/controls/hover_button.h"
#include "chrome/browser/ui/views/controls/page_switcher_view.h"
#include "chrome/browser/ui/views/controls/rich_controls_container_view.h"
#include "chrome/browser/ui/views/controls/rich_hover_button.h"
#include "chrome/browser/ui/views/page_info/chosen_object_view.h"
#include "chrome/browser/ui/views/page_info/page_info_main_view.h"
#include "chrome/browser/ui/views/page_info/page_info_security_content_view.h"
#include "chrome/browser/ui/views/page_info/page_info_view_factory.h"
#include "chrome/browser/ui/views/page_info/permission_toggle_row_view.h"
#include "chrome/browser/usb/usb_chooser_context.h"
#include "chrome/browser/usb/usb_chooser_context_factory.h"
#include "chrome/common/pref_names.h"
#include "chrome/common/url_constants.h"
#include "chrome/test/base/testing_browser_process.h"
#include "chrome/test/base/testing_profile.h"
#include "chrome/test/base/testing_profile_manager.h"
#include "chrome/test/views/chrome_test_views_delegate.h"
#include "components/content_settings/core/browser/content_settings_uma_util.h"
#include "components/content_settings/core/browser/host_content_settings_map.h"
#include "components/content_settings/core/common/cookie_blocking_3pcd_status.h"
#include "components/content_settings/core/common/features.h"
#include "components/content_settings/core/common/pref_names.h"
#include "components/history/core/browser/history_service.h"
#include "components/page_info/core/features.h"
#include "components/permissions/permission_recovery_success_rate_tracker.h"
#include "components/permissions/permission_uma_util.h"
#include "components/permissions/permission_util.h"
#include "components/privacy_sandbox/privacy_sandbox_features.h"
#include "components/strings/grit/components_strings.h"
#include "components/ukm/test_ukm_recorder.h"
#include "content/public/browser/ssl_status.h"
#include "content/public/test/browser_task_environment.h"
#include "content/public/test/navigation_simulator.h"
#include "content/public/test/test_renderer_host.h"
#include "content/public/test/test_web_contents_factory.h"
#include "mojo/public/cpp/bindings/pending_remote.h"
#include "net/cert/cert_status_flags.h"
#include "net/ssl/ssl_connection_status_flags.h"
#include "net/ssl/ssl_info.h"
#include "net/test/cert_test_util.h"
#include "net/test/test_certificate_data.h"
#include "net/test/test_data_directory.h"
#include "ppapi/buildflags/buildflags.h"
#include "services/device/public/cpp/test/fake_usb_device_manager.h"
#include "services/device/public/mojom/usb_device.mojom.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "ui/accessibility/ax_enums.mojom.h"
#include "ui/accessibility/ax_node_data.h"
#include "ui/base/l10n/l10n_util.h"
#include "ui/base/ui_base_features.h"
#include "ui/events/event_utils.h"
#include "ui/views/controls/button/toggle_button.h"
#include "ui/views/controls/combobox/combobox.h"
#include "ui/views/controls/label.h"
#include "ui/views/controls/link.h"
#include "ui/views/controls/styled_label.h"
#include "ui/views/test/button_test_api.h"
#include "ui/views/test/scoped_views_test_helper.h"
#include "ui/views/test/test_views_delegate.h"

#if BUILDFLAG(ENABLE_PLUGINS)
#include "chrome/browser/plugins/chrome_plugin_service_filter.h"
#endif

#if BUILDFLAG(IS_CHROMEOS_ASH)
#include "chrome/browser/ash/login/users/fake_chrome_user_manager.h"
#include "components/account_id/account_id.h"
#include "components/user_manager/scoped_user_manager.h"
#endif  // BUILDFLAG(IS_CHROMEOS_ASH)

const char* kUrl = "http://www.example.com/index.html";
const char* kSecureUrl = "https://www.example.com/index.html";
std::u16string kHostname = u"example.com";

namespace test {

class PageInfoBubbleViewTestApi {
 public:
  PageInfoBubbleViewTestApi(gfx::NativeWindow parent,
                            content::WebContents* web_contents)
      : bubble_delegate_(nullptr),
        parent_(parent),
        web_contents_(web_contents) {
    CreateView();
  }

  PageInfoBubbleViewTestApi(const PageInfoBubbleViewTestApi&) = delete;
  PageInfoBubbleViewTestApi& operator=(const PageInfoBubbleViewTestApi&) =
      delete;

  void CreateView() {
    if (bubble_delegate_) {
      bubble_delegate_->GetWidget()->CloseNow();
    }

    views::View* anchor_view = nullptr;
    auto* bubble = static_cast<PageInfoBubbleView*>(
        PageInfoBubbleView::CreatePageInfoBubble(
            anchor_view, gfx::Rect(), parent_, web_contents_, GURL(kUrl),
            base::DoNothing(),
            base::BindOnce(&PageInfoBubbleViewTestApi::OnPageInfoBubbleClosed,
                           base::Unretained(this), run_loop_.QuitClosure()),
            /*allow_about_this_site=*/true));
    presenter_ = bubble->presenter_for_testing();
    navigation_handler_ = bubble;
    bubble_delegate_ = bubble;
    toggle_rows_ =
        &static_cast<PageInfoMainView*>(current_view())->toggle_rows_;
  }

  views::View* current_view() {
    return bubble_delegate_->GetViewByID(
        PageInfoViewFactory::VIEW_ID_PAGE_INFO_CURRENT_VIEW);
  }
  bool reload_prompt() const { return *reload_prompt_; }
  views::Widget::ClosedReason closed_reason() const { return *closed_reason_; }

  views::View* permissions_view() {
    return bubble_delegate_->GetViewByID(
        PageInfoViewFactory::VIEW_ID_PAGE_INFO_PERMISSION_VIEW);
  }

  const views::View* permissions_view() const {
    return bubble_delegate_->GetViewByID(
        PageInfoViewFactory::VIEW_ID_PAGE_INFO_PERMISSION_VIEW);
  }

  views::View* cookie_button() {
    return bubble_delegate_->GetViewByID(
        PageInfoViewFactory::VIEW_ID_PAGE_INFO_LINK_OR_BUTTON_COOKIE_DIALOG);
  }

  views::View* cookies_buttons_container_view() {
    return bubble_delegate_->GetViewByID(
        PageInfoViewFactory::VIEW_ID_PAGE_INFO_COOKIES_BUTTONS_CONTAINER);
  }
  views::View* cookies_dialog_button() {
    return bubble_delegate_->GetViewByID(
        PageInfoViewFactory::VIEW_ID_PAGE_INFO_LINK_OR_BUTTON_COOKIE_DIALOG);
  }

  views::View* blocking_third_party_cookies_row() {
    return bubble_delegate_->GetViewByID(
        PageInfoViewFactory::VIEW_ID_PAGE_INFO_BLOCK_THIRD_PARTY_COOKIES_ROW);
  }

  views::View* blocking_third_party_cookies_subtitle() {
    return bubble_delegate_->GetViewByID(
        PageInfoViewFactory::
            VIEW_ID_PAGE_INFO_BLOCK_THIRD_PARTY_COOKIES_SUBTITLE);
  }

  views::View* rws_button() {
    return bubble_delegate_->GetViewByID(
        PageInfoViewFactory::VIEW_ID_PAGE_INFO_LINK_OR_BUTTON_RWS_SETTINGS);
  }

  RichHoverButton* certificate_button() const {
    return static_cast<RichHoverButton*>(bubble_delegate_->GetViewByID(
        PageInfoViewFactory::
            VIEW_ID_PAGE_INFO_LINK_OR_BUTTON_CERTIFICATE_VIEWER));
  }

  views::View* security_summary_label() {
    return bubble_delegate_->GetViewByID(
        PageInfoViewFactory::VIEW_ID_PAGE_INFO_SECURITY_SUMMARY_LABEL);
  }

  views::StyledLabel* security_details_label() {
    return static_cast<views::StyledLabel*>(bubble_delegate_->GetViewByID(
        PageInfoViewFactory::VIEW_ID_PAGE_INFO_SECURITY_DETAILS_LABEL));
  }

  views::LabelButton* reset_permissions_button() {
    return static_cast<views::LabelButton*>(bubble_delegate_->GetViewByID(
        PageInfoViewFactory::VIEW_ID_PAGE_INFO_RESET_PERMISSIONS_BUTTON));
  }

  PageInfoNavigationHandler* navigation_handler() {
    return navigation_handler_;
  }

  std::u16string GetWindowTitle() { return bubble_delegate_->GetWindowTitle(); }

  PermissionToggleRowView* GetPermissionToggleRowAt(int index) {
    return (*toggle_rows_)[index];
  }

  views::ToggleButton* GetToggleViewAt(int index) {
    return GetPermissionToggleRowAt(index)->toggle_button_;
  }

  views::Label* GetStateLabelAt(int index) {
    return GetPermissionToggleRowAt(index)->state_label_;
  }

  std::u16string GetTrackingProtectionSubpageTitle() {
    navigation_handler()->OpenCookiesPage();
    auto* title_label = bubble_delegate_->GetViewByID(
        PageInfoViewFactory::VIEW_ID_PAGE_INFO_SUBPAGE_TITLE);
    return static_cast<views::Label*>(title_label)->GetText();
  }

  // Returns the text shown on the view.
  std::u16string GetTextOnView(views::View* view) {
    EXPECT_TRUE(view);
    ui::AXNodeData data;
    view->GetAccessibleNodeData(&data);
    const std::string& name =
        data.GetStringAttribute(ax::mojom::StringAttribute::kName);
    return base::ASCIIToUTF16(name);
  }

  // Returns the number of cookies shown on the link or button to open the
  // collected cookies dialog. This should always be shown.
  std::u16string GetCookiesLinkText() {
    EXPECT_TRUE(cookie_button());
    ui::AXNodeData data;
    cookie_button()->GetAccessibleNodeData(&data);
    const std::string& name =
        data.GetStringAttribute(ax::mojom::StringAttribute::kName);
    return base::ASCIIToUTF16(name);
  }

  std::u16string GetSecurityInformationButtonText() {
    auto* button = bubble_delegate_->GetViewByID(
        PageInfoViewFactory::
            VIEW_ID_PAGE_INFO_LINK_OR_BUTTON_SECURITY_INFORMATION);
    return static_cast<RichHoverButton*>(button)
        ->GetTitleViewForTesting()
        ->GetText();
  }

  std::u16string GetSecuritySummaryText() {
    EXPECT_TRUE(security_summary_label());
    return static_cast<views::StyledLabel*>(security_summary_label())
        ->GetText();
  }

  std::u16string GetTrackingProtectionButtonTitleText() {
    auto* button = bubble_delegate_->GetViewByID(
        PageInfoViewFactory::VIEW_ID_PAGE_INFO_LINK_OR_BUTTON_COOKIES_SUBPAGE);
    return static_cast<RichHoverButton*>(button)
        ->GetTitleViewForTesting()
        ->GetText();
  }

  std::u16string GetTrackingProtectionButtonSubTitleText() {
    auto* button = bubble_delegate_->GetViewByID(
        PageInfoViewFactory::VIEW_ID_PAGE_INFO_LINK_OR_BUTTON_COOKIES_SUBPAGE);
    return static_cast<RichHoverButton*>(button)
        ->GetSubTitleViewForTesting()
        ->GetText();
  }

  std::u16string GetPermissionLabelTextAt(int index) {
    return GetPermissionToggleRowAt(index)->row_view_->GetTitleForTesting();
  }

  bool GetPermissionToggleIsOnAt(int index) {
    auto* toggle = GetToggleViewAt(index);
    return toggle->GetIsOn();
  }

  void SimulateTogglingPermissionAt(int index) {
    auto* toggle = GetToggleViewAt(index);
    toggle->SetIsOn(!toggle->GetIsOn());
  }

  size_t GetPermissionsCount() const {
    const views::View* parent = permissions_view();
    size_t actual_count = parent ? parent->children().size() : 0;

    // Non-empty permission section has a reset all button
    // after all permission rows.
    if (actual_count)
      --actual_count;

    return actual_count;
  }

  // Simulates updating the number of blocked and allowed sites and rws info.
  void SetCookieInfo(const PageInfoUI::CookiesNewInfo& cookie_info) {
    presenter_->ui_for_testing()->SetCookieInfo(cookie_info);
  }

  // Simulates recreating the dialog with a new PermissionInfoList.
  // It ignores `source` field and assumes that user is the source. It's because
  // in the actual UI, permission's state can be changed only if the source is
  // user.
  void SetPermissionInfo(const PermissionInfoList& list) {
    for (const PageInfo::PermissionInfo& info : list) {
      presenter_->OnSitePermissionChanged(info.type, info.setting,
                                          info.requesting_origin,
                                          /*is_one_time=*/false);
    }
    CreateView();
  }

  std::u16string GetCertificateButtonSubtitleText() const {
    EXPECT_TRUE(certificate_button());
    EXPECT_TRUE(certificate_button()->GetSubTitleViewForTesting());
    return certificate_button()->GetSubTitleViewForTesting()->GetText();
  }

  const views::View::Views& GetChosenObjectChildren() {
    const views::View* parent = permissions_view();
    const int object_view_index = 0;
    ChosenObjectView* object_view =
        static_cast<ChosenObjectView*>(parent->children()[object_view_index]);
    views::View* row_view = object_view->children()[0];
    return row_view->children();
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

  raw_ptr<views::BubbleDialogDelegateView, DanglingUntriaged> bubble_delegate_;
  raw_ptr<PageInfo, DanglingUntriaged> presenter_ = nullptr;
  raw_ptr<std::vector<raw_ptr<PermissionToggleRowView, VectorExperimental>>,
          DanglingUntriaged>
      toggle_rows_ = nullptr;

  raw_ptr<PageInfoNavigationHandler, DanglingUntriaged> navigation_handler_ =
      nullptr;

  // For recreating the view.
  gfx::NativeWindow parent_;
  raw_ptr<content::WebContents> web_contents_;
  base::RunLoop run_loop_;
  std::optional<bool> reload_prompt_;
  std::optional<views::Widget::ClosedReason> closed_reason_;
};

}  // namespace test

namespace {

using ::base::test::ParseJson;
using ::testing::_;
using ::testing::Return;

constexpr char kTestUserEmail[] = "user@example.com";

// Helper class that wraps a TestingProfile and a TestWebContents for a test
// harness. Inspired by RenderViewHostTestHarness, but doesn't use inheritance
// so the helper can be composed with other helpers in the test harness.
class ScopedWebContentsTestHelper {
 public:
  explicit ScopedWebContentsTestHelper(bool off_the_record)
      : testing_profile_manager_(TestingBrowserProcess::GetGlobal()) {
#if BUILDFLAG(IS_CHROMEOS_ASH)
    auto fake_user_manager = std::make_unique<ash::FakeChromeUserManager>();
    auto* fake_user_manager_ptr = fake_user_manager.get();
    scoped_user_manager_ = std::make_unique<user_manager::ScopedUserManager>(
        std::move(fake_user_manager));

    constexpr char kTestUserGaiaId[] = "1111111111";
    auto account_id =
        AccountId::FromUserEmailGaiaId(kTestUserEmail, kTestUserGaiaId);
    fake_user_manager_ptr->AddUserWithAffiliation(account_id,
                                                  /*is_affiliated=*/true);
    fake_user_manager_ptr->LoginUser(account_id);
#endif  // BUILDFLAG(IS_CHROMEOS_ASH)

    EXPECT_TRUE(testing_profile_manager_.SetUp());
    profile_ = testing_profile_manager_.CreateTestingProfile(
        kTestUserEmail, {TestingProfile::TestingFactory{
                            HistoryServiceFactory::GetInstance(),
                            HistoryServiceFactory::GetDefaultFactory()}});
    EXPECT_TRUE(profile_);

    if (off_the_record)
      profile_ = profile_->GetPrimaryOTRProfile(/*create_if_needed=*/true);
    web_contents_ = factory_.CreateWebContents(profile_);
  }

  ScopedWebContentsTestHelper(const ScopedWebContentsTestHelper&) = delete;
  ScopedWebContentsTestHelper& operator=(const ScopedWebContentsTestHelper&) =
      delete;

  content::WebContents* web_contents() { return web_contents_; }
  Profile* profile() { return profile_; }
  TestingPrefServiceSimple* local_state() {
    return testing_profile_manager_.local_state()->Get();
  }

 private:
  content::BrowserTaskEnvironment task_environment_;

#if BUILDFLAG(IS_CHROMEOS_ASH)
  std::unique_ptr<user_manager::ScopedUserManager> scoped_user_manager_;
#endif

  TestingProfileManager testing_profile_manager_;
  raw_ptr<Profile> profile_ = nullptr;
  content::TestWebContentsFactory factory_;
  raw_ptr<content::WebContents> web_contents_;  // Weak. Owned by factory_.
};

class PageInfoBubbleViewTest : public testing::Test {
 public:
  PageInfoBubbleViewTest() = default;
  PageInfoBubbleViewTest(const PageInfoBubbleViewTest& chip) = delete;
  PageInfoBubbleViewTest& operator=(const PageInfoBubbleViewTest& chip) =
      delete;

  // testing::Test:
  void SetUp() override {
    if (!web_contents_helper_) {
      web_contents_helper_ =
          std::make_unique<ScopedWebContentsTestHelper>(false);
    }
    views_helper_ = std::make_unique<views::ScopedViewsTestHelper>(
        std::make_unique<ChromeTestViewsDelegate<>>());
    views::Widget::InitParams parent_params(
        views::Widget::InitParams::NATIVE_WIDGET_OWNS_WIDGET);
    parent_params.context = views_helper_->GetContext();
    parent_window_ = new views::Widget();
    parent_window_->Init(std::move(parent_params));

    mock_sentiment_service_ = static_cast<MockTrustSafetySentimentService*>(
        TrustSafetySentimentServiceFactory::GetInstance()
            ->SetTestingFactoryAndUse(
                web_contents_helper_->profile(),
                base::BindRepeating(&BuildMockTrustSafetySentimentService)));

    content::WebContents* web_contents = web_contents_helper_->web_contents();
    content_settings::PageSpecificContentSettings::CreateForWebContents(
        web_contents,
        std::make_unique<PageSpecificContentSettingsDelegate>(web_contents));
    api_ = std::make_unique<test::PageInfoBubbleViewTestApi>(
        parent_window_->GetNativeWindow(), web_contents);

    permissions::PermissionRecoverySuccessRateTracker::CreateForWebContents(
        web_contents);
  }

  void TearDown() override {
    parent_window_->CloseNow();
  }

 protected:
  std::unique_ptr<ScopedWebContentsTestHelper> web_contents_helper_;
  std::unique_ptr<views::ScopedViewsTestHelper> views_helper_;
  raw_ptr<MockTrustSafetySentimentService> mock_sentiment_service_;

  raw_ptr<views::Widget, DanglingUntriaged> parent_window_ =
      nullptr;  // Weak. Owned by the NativeWidget.
  std::unique_ptr<test::PageInfoBubbleViewTestApi> api_;
};

views::Label* GetChosenObjectTitle(const views::View::Views& children) {
  views::View* labels_container = children[1];
  return static_cast<views::Label*>(labels_container->children()[0]);
}

views::Button* GetChosenObjectButton(const views::View::Views& children) {
  return static_cast<views::Button*>(children[2]);
}

views::Label* GetChosenObjectDescriptionLabel(
    const views::View::Views& children) {
  views::View* labels_container = children[1];
  return static_cast<views::Label*>(labels_container->children()[1]);
}

}  // namespace

TEST_F(PageInfoBubbleViewTest, NotificationPermissionRevokeUkm) {
  GURL origin_url = GURL(kUrl).DeprecatedGetOriginAsURL();
  ukm::TestAutoSetUkmRecorder ukm_recorder;

  PermissionInfoList list(1);
  list.back().type = ContentSettingsType::NOTIFICATIONS;

  list.back().setting = CONTENT_SETTING_ALLOW;
  api_->SetPermissionInfo(list);

  list.back().setting = CONTENT_SETTING_BLOCK;
  api_->SetPermissionInfo(list);

  auto entries = ukm_recorder.GetEntriesByName("Permission");
  EXPECT_EQ(1u, entries.size());
  auto* entry = entries.front().get();

  ukm_recorder.ExpectEntrySourceHasUrl(entry, origin_url);
  EXPECT_EQ(*ukm_recorder.GetEntryMetric(entry, "Source"),
            static_cast<int64_t>(permissions::PermissionSourceUI::OIB));
  EXPECT_EQ(*ukm_recorder.GetEntryMetric(entry, "PermissionType"),
            content_settings_uma_util::ContentSettingTypeToHistogramValue(
                ContentSettingsType::NOTIFICATIONS));
  EXPECT_EQ(*ukm_recorder.GetEntryMetric(entry, "Action"),
            static_cast<int64_t>(permissions::PermissionAction::REVOKED));
}

// Test UI construction and reconstruction via
// PageInfoBubbleView::SetPermissionInfo().
TEST_F(PageInfoBubbleViewTest, SetPermissionInfo) {
  PermissionInfoList list(1);
  list.back().type = ContentSettingsType::GEOLOCATION;
  list.back().setting = CONTENT_SETTING_BLOCK;

  // Initially, no permissions are shown because they are all set to default.
  size_t num_expected_children = 0;
  EXPECT_EQ(num_expected_children, api_->GetPermissionsCount());
  EXPECT_FALSE(api_->reset_permissions_button());

  num_expected_children += list.size();
  list.back().setting = CONTENT_SETTING_ALLOW;
  api_->SetPermissionInfo(list);
  EXPECT_EQ(num_expected_children, api_->GetPermissionsCount());

  EXPECT_TRUE(api_->reset_permissions_button()->GetVisible());
  EXPECT_TRUE(api_->reset_permissions_button()->GetEnabled());
  EXPECT_EQ(u"Reset permission", api_->reset_permissions_button()->GetText());
  PermissionToggleRowView* toggle_view = api_->GetPermissionToggleRowAt(0);
  EXPECT_TRUE(toggle_view);

  // Verify labels match the settings on the PermissionInfoList.
  EXPECT_EQ(u"Location", api_->GetPermissionLabelTextAt(0));
  EXPECT_TRUE(api_->GetPermissionToggleIsOnAt(0));

  // Verify calling SetPermissionInfo() directly updates the UI.
  list.back().setting = CONTENT_SETTING_BLOCK;
  api_->SetPermissionInfo(list);
  EXPECT_FALSE(api_->GetPermissionToggleIsOnAt(0));

  // Simulate a user selection via the UI. Note this will also cover logic in
  // PageInfo to update the pref.
  api_->SimulateTogglingPermissionAt(0);
  EXPECT_EQ(num_expected_children, api_->GetPermissionsCount());
  EXPECT_TRUE(api_->GetPermissionToggleIsOnAt(0));

  const ui::MouseEvent event(ui::EventType::kMousePressed, gfx::Point(),
                             gfx::Point(), ui::EventTimeForNow(), 0, 0);
  views::test::ButtonTestApi(api_->reset_permissions_button())
      .NotifyClick(event);
  // After resetting permissions, button doesn't disappear but is disabled.
  EXPECT_TRUE(api_->reset_permissions_button()->GetVisible());
  EXPECT_FALSE(api_->reset_permissions_button()->GetEnabled());

  // In the ask state, the toggle is in the off state, indicating that
  // permission isn't granted.
  EXPECT_FALSE(api_->GetPermissionToggleIsOnAt(0));

  // However, since the setting is now default, recreating the dialog with
  // those settings should omit the permission from the UI.
  //
  // TODO(crbug.com/40570388): Reconcile the comment above with the fact
  // that |num_expected_children| is not, at this point, 0 and therefore the
  // permission is not being omitted from the UI.
  api_->SetPermissionInfo(list);
  EXPECT_EQ(num_expected_children, api_->GetPermissionsCount());
}

class PageInfoBubbleViewOffTheRecordTest : public PageInfoBubbleViewTest {
 public:
  PageInfoBubbleViewOffTheRecordTest() {
    web_contents_helper_ = std::make_unique<ScopedWebContentsTestHelper>(true);
  }
};

// Test resetting blocked in Incognito permission.
TEST_F(PageInfoBubbleViewOffTheRecordTest, ResetBlockedInIncognitoPermission) {
  // No sentiment service in incognito.
  EXPECT_FALSE(mock_sentiment_service_);

  PermissionInfoList list(1);
  list.back().type = ContentSettingsType::NOTIFICATIONS;
  list.back().setting = CONTENT_SETTING_BLOCK;

  // Initially, no permissions are shown because they are all set to default.
  size_t num_expected_children = 0;
  EXPECT_EQ(num_expected_children, api_->GetPermissionsCount());
  EXPECT_FALSE(api_->reset_permissions_button());

  num_expected_children = list.size();
  api_->SetPermissionInfo(list);
  EXPECT_EQ(num_expected_children, api_->GetPermissionsCount());

  // Because permission is autoblocked, no reset button initially is shown.
  EXPECT_FALSE(api_->reset_permissions_button()->GetVisible());
  EXPECT_FALSE(api_->reset_permissions_button()->GetEnabled());

  // Autoblocked permissions don't have toggles or state labels.
  EXPECT_FALSE(api_->GetToggleViewAt(0));
  EXPECT_FALSE(api_->GetStateLabelAt(0));

  // Verify labels match the settings on the PermissionInfoList.
  EXPECT_EQ(u"Notifications", api_->GetPermissionLabelTextAt(0));

  PageInfo::PermissionInfo window_management_permission;
  window_management_permission.type = ContentSettingsType::WINDOW_MANAGEMENT;
  window_management_permission.setting = CONTENT_SETTING_ALLOW;
  window_management_permission.default_setting = CONTENT_SETTING_ASK;
  list.push_back(window_management_permission);

  num_expected_children = list.size();
  api_->SetPermissionInfo(list);
  EXPECT_EQ(num_expected_children, api_->GetPermissionsCount());

  // Because a non-managed permission was added, reset button is visible and
  // enabled.
  EXPECT_TRUE(api_->reset_permissions_button()->GetVisible());
  EXPECT_TRUE(api_->reset_permissions_button()->GetEnabled());
  // Although there are only one resettable permission, multiple rows are
  // shown. Because of that use plural version of the "permission" word.
  EXPECT_EQ(u"Reset permissions", api_->reset_permissions_button()->GetText());

  // User managed permissions have toggles. |camera_permission| is allowed and
  // the toggle must be on.
  EXPECT_TRUE(api_->GetToggleViewAt(1));
  EXPECT_TRUE(api_->GetPermissionToggleIsOnAt(1));

  const ui::MouseEvent event(ui::EventType::kMousePressed, gfx::Point(),
                             gfx::Point(), ui::EventTimeForNow(), 0, 0);
  views::test::ButtonTestApi(api_->reset_permissions_button())
      .NotifyClick(event);
  // After resetting permissions, button doesn't disappear but is disabled.
  EXPECT_TRUE(api_->reset_permissions_button()->GetVisible());
  EXPECT_FALSE(api_->reset_permissions_button()->GetEnabled());

  // Show state label for user managed permission, indicating that permission
  // is in the default ask state now. Autoblocked permission doesn't change.
  EXPECT_FALSE(api_->GetStateLabelAt(0));
  EXPECT_EQ(u"Can ask to manage windows on all your displays",
            api_->GetStateLabelAt(1)->GetText());

  // In the ask state, the toggle is in the off state, indicating that
  // permission isn't granted.
  EXPECT_FALSE(api_->GetPermissionToggleIsOnAt(1));
}

// Test UI construction and reconstruction with USB devices.
TEST_F(PageInfoBubbleViewTest, SetPermissionInfoWithUsbDevice) {
  EXPECT_CALL(*mock_sentiment_service_, InteractedWithPageInfo);
  constexpr size_t kExpectedChildren = 0;
  EXPECT_EQ(kExpectedChildren, api_->GetPermissionsCount());

  const auto origin = url::Origin::Create(GURL(kUrl));

  // Connect the UsbChooserContext with FakeUsbDeviceManager.
  device::FakeUsbDeviceManager usb_device_manager;
  mojo::PendingRemote<device::mojom::UsbDeviceManager> usb_manager;
  usb_device_manager.AddReceiver(usb_manager.InitWithNewPipeAndPassReceiver());
  UsbChooserContext* store =
      UsbChooserContextFactory::GetForProfile(web_contents_helper_->profile());
  store->SetDeviceManagerForTesting(std::move(usb_manager));

  auto device_info = usb_device_manager.CreateAndAddDevice(
      0, 0, "Google", "Gizmo", "1234567890");
  store->GrantDevicePermission(origin, *device_info);

  PermissionInfoList list;
  api_->SetPermissionInfo(list);
  EXPECT_EQ(kExpectedChildren + 1, api_->GetPermissionsCount());

  const auto& chosen_object_children = api_->GetChosenObjectChildren();
  EXPECT_EQ(3u, chosen_object_children.size());

  views::Label* label = GetChosenObjectTitle(chosen_object_children);
  EXPECT_EQ(u"Gizmo", label->GetText());

  views::Button* button = GetChosenObjectButton(chosen_object_children);
  const ui::MouseEvent event(ui::EventType::kMousePressed, gfx::Point(),
                             gfx::Point(), ui::EventTimeForNow(), 0, 0);
  views::test::ButtonTestApi(button).NotifyClick(event);
  api_->SetPermissionInfo(list);
  EXPECT_EQ(kExpectedChildren, api_->GetPermissionsCount());
  EXPECT_FALSE(store->HasDevicePermission(origin, *device_info));
}

// Test resetting USB devices permission.
TEST_F(PageInfoBubbleViewTest, ResetPermissionInfoWithUsbDevice) {
  EXPECT_CALL(*mock_sentiment_service_, InteractedWithPageInfo).Times(2);

  constexpr size_t kExpectedChildren = 0;
  EXPECT_EQ(kExpectedChildren, api_->GetPermissionsCount());
  EXPECT_FALSE(api_->reset_permissions_button());

  const auto origin = url::Origin::Create(GURL(kUrl));

  // Connect the UsbChooserContext with FakeUsbDeviceManager.
  device::FakeUsbDeviceManager usb_device_manager;
  mojo::PendingRemote<device::mojom::UsbDeviceManager> usb_manager;
  usb_device_manager.AddReceiver(usb_manager.InitWithNewPipeAndPassReceiver());
  UsbChooserContext* store =
      UsbChooserContextFactory::GetForProfile(web_contents_helper_->profile());
  store->SetDeviceManagerForTesting(std::move(usb_manager));

  auto device_info = usb_device_manager.CreateAndAddDevice(
      0, 0, "Google", "Gizmo", "1234567890");
  store->GrantDevicePermission(origin, *device_info);

  PermissionInfoList list;
  api_->SetPermissionInfo(list);
  EXPECT_EQ(kExpectedChildren + 1, api_->GetPermissionsCount());
  EXPECT_TRUE(api_->reset_permissions_button()->GetVisible());
  EXPECT_TRUE(api_->reset_permissions_button()->GetEnabled());
  EXPECT_EQ(u"Reset permission", api_->reset_permissions_button()->GetText());

  const auto& chosen_object_children = api_->GetChosenObjectChildren();
  EXPECT_EQ(3u, chosen_object_children.size());

  views::Label* label = GetChosenObjectTitle(chosen_object_children);
  EXPECT_EQ(u"Gizmo", label->GetText());

  const ui::MouseEvent event(ui::EventType::kMousePressed, gfx::Point(),
                             gfx::Point(), ui::EventTimeForNow(), 0, 0);
  views::test::ButtonTestApi(api_->reset_permissions_button())
      .NotifyClick(event);
  api_->SetPermissionInfo(list);
  EXPECT_EQ(kExpectedChildren, api_->GetPermissionsCount());
  EXPECT_FALSE(api_->reset_permissions_button());
  EXPECT_FALSE(store->HasDevicePermission(origin, *device_info));
}

namespace {

constexpr char kWebUsbPolicySetting[] = R"(
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
  EXPECT_EQ(kExpectedChildren, api_->GetPermissionsCount());

  const auto origin = url::Origin::Create(GURL(kUrl));

  // Add the policy setting to prefs.
  Profile* profile = web_contents_helper_->profile();
  profile->GetPrefs()->Set(prefs::kManagedWebUsbAllowDevicesForUrls,
                           ParseJson(kWebUsbPolicySetting));
  UsbChooserContext* store = UsbChooserContextFactory::GetForProfile(profile);

  auto objects = store->GetGrantedObjects(origin);
  EXPECT_EQ(objects.size(), 1u);

  PermissionInfoList list;
  api_->SetPermissionInfo(list);
  EXPECT_EQ(kExpectedChildren + 1, api_->GetPermissionsCount());

  const auto& chosen_object_children = api_->GetChosenObjectChildren();
  EXPECT_EQ(3u, chosen_object_children.size());

  views::Label* label = GetChosenObjectTitle(chosen_object_children);
  EXPECT_EQ(u"Unknown product 0x162E from Google Inc.", label->GetText());

  views::Button* button = GetChosenObjectButton(chosen_object_children);
  EXPECT_EQ(button->GetState(), views::Button::STATE_DISABLED);

  views::Label* desc_label =
      GetChosenObjectDescriptionLabel(chosen_object_children);
  EXPECT_EQ(u"USB device allowed by your administrator", desc_label->GetText());

  // Policy granted USB permissions should not be able to be deleted.
  const ui::MouseEvent event(ui::EventType::kMousePressed, gfx::Point(),
                             gfx::Point(), ui::EventTimeForNow(), 0, 0);
  views::test::ButtonTestApi(button).NotifyClick(event);
  api_->SetPermissionInfo(list);
  EXPECT_EQ(kExpectedChildren + 1, api_->GetPermissionsCount());
}

// Test UI construction and reconstruction with both user and policy USB
// devices.
TEST_F(PageInfoBubbleViewTest, SetPermissionInfoWithUserAndPolicyUsbDevices) {
  EXPECT_CALL(*mock_sentiment_service_, InteractedWithPageInfo);
  constexpr size_t kExpectedChildren = 0;
  EXPECT_EQ(kExpectedChildren, api_->GetPermissionsCount());

  const auto origin = url::Origin::Create(GURL(kUrl));

  // Add the policy setting to prefs.
  Profile* profile = web_contents_helper_->profile();
  profile->GetPrefs()->Set(prefs::kManagedWebUsbAllowDevicesForUrls,
                           ParseJson(kWebUsbPolicySetting));

  // Connect the UsbChooserContext with FakeUsbDeviceManager.
  device::FakeUsbDeviceManager usb_device_manager;
  mojo::PendingRemote<device::mojom::UsbDeviceManager> device_manager;
  usb_device_manager.AddReceiver(
      device_manager.InitWithNewPipeAndPassReceiver());
  UsbChooserContext* store = UsbChooserContextFactory::GetForProfile(profile);
  store->SetDeviceManagerForTesting(std::move(device_manager));

  auto device_info = usb_device_manager.CreateAndAddDevice(
      0, 0, "Google", "Gizmo", "1234567890");
  store->GrantDevicePermission(origin, *device_info);

  auto objects = store->GetGrantedObjects(origin);
  EXPECT_EQ(objects.size(), 2u);

  PermissionInfoList list;
  api_->SetPermissionInfo(list);
  EXPECT_EQ(kExpectedChildren + 2, api_->GetPermissionsCount());

  const ui::MouseEvent event(ui::EventType::kMousePressed, gfx::Point(),
                             gfx::Point(), ui::EventTimeForNow(), 0, 0);

  // The first object is the user granted permission for the "Gizmo" device.
  {
    const auto& chosen_object_children = api_->GetChosenObjectChildren();
    EXPECT_EQ(3u, chosen_object_children.size());

    views::Label* label = GetChosenObjectTitle(chosen_object_children);
    EXPECT_EQ(u"Gizmo", label->GetText());

    views::Button* button = GetChosenObjectButton(chosen_object_children);
    EXPECT_NE(button->GetState(), views::Button::STATE_DISABLED);

    views::Label* desc_label =
        GetChosenObjectDescriptionLabel(chosen_object_children);
    EXPECT_EQ(u"USB device", desc_label->GetText());

    views::test::ButtonTestApi(button).NotifyClick(event);
    api_->SetPermissionInfo(list);
    EXPECT_EQ(kExpectedChildren + 1, api_->GetPermissionsCount());
    EXPECT_FALSE(store->HasDevicePermission(origin, *device_info));
  }

  // The policy granted permission should now be the first child, since the user
  // permission was deleted.
  {
    const auto& chosen_object_children = api_->GetChosenObjectChildren();
    EXPECT_EQ(3u, chosen_object_children.size());

    views::Label* label = GetChosenObjectTitle(chosen_object_children);
    EXPECT_EQ(u"Unknown product 0x162E from Google Inc.", label->GetText());

    views::Button* button = GetChosenObjectButton(chosen_object_children);
    EXPECT_EQ(button->GetState(), views::Button::STATE_DISABLED);

    views::Label* desc_label =
        GetChosenObjectDescriptionLabel(chosen_object_children);
    EXPECT_EQ(u"USB device allowed by your administrator",
              desc_label->GetText());

    views::test::ButtonTestApi(button).NotifyClick(event);
    api_->SetPermissionInfo(list);
    EXPECT_EQ(kExpectedChildren + 1, api_->GetPermissionsCount());
  }
}

TEST_F(PageInfoBubbleViewTest, SetPermissionInfoForUsbGuard) {
  PermissionInfoList list(1);
  list.back().type = ContentSettingsType::USB_GUARD;
  list.back().setting = CONTENT_SETTING_ASK;

  // Initially, no permissions are shown because they are all set to default.
  size_t num_expected_children = 0;
  EXPECT_EQ(num_expected_children, api_->GetPermissionsCount());

  // Verify calling SetPermissionInfo() directly updates the UI.
  num_expected_children += list.size();
  list.back().setting = CONTENT_SETTING_BLOCK;
  api_->SetPermissionInfo(list);
  EXPECT_FALSE(api_->GetPermissionToggleIsOnAt(0));

  // Simulate a user selection via the UI. Note this will also cover logic in
  // PageInfo to update the pref.
  api_->SimulateTogglingPermissionAt(0);
  EXPECT_EQ(num_expected_children, api_->GetPermissionsCount());
  EXPECT_TRUE(api_->GetPermissionToggleIsOnAt(0));

  // However, since the setting is now default, recreating the dialog with
  // those settings should omit the permission from the UI.
  //
  // TODO(crbug.com/40570388): Reconcile the comment above with the fact
  // that |num_expected_children| is not, at this point, 0 and therefore the
  // permission is not being omitted from the UI.
  api_->SetPermissionInfo(list);
  EXPECT_EQ(num_expected_children, api_->GetPermissionsCount());
}

// Test UI construction and reconstruction with policy USB devices.
TEST_F(PageInfoBubbleViewTest, SetPermissionInfoWithPolicySerialPorts) {
  constexpr size_t kExpectedChildren = 0;
  EXPECT_EQ(kExpectedChildren, api_->GetPermissionsCount());

  // Add the policy setting to prefs.
  web_contents_helper_->local_state()->Set(
      prefs::kManagedSerialAllowUsbDevicesForUrls, ParseJson(R"([
               {
                 "devices": [{ "vendor_id": 6353, "product_id": 5678 }],
                 "urls": [ "http://www.example.com" ]
               }
             ])"));

  PermissionInfoList list;
  api_->SetPermissionInfo(list);
  EXPECT_EQ(kExpectedChildren + 1, api_->GetPermissionsCount());

  const auto& chosen_object_children = api_->GetChosenObjectChildren();
  EXPECT_EQ(3u, chosen_object_children.size());

  views::Label* label = GetChosenObjectTitle(chosen_object_children);
  EXPECT_EQ(u"USB device from Google Inc. (product 162E)", label->GetText());

  views::Button* button = GetChosenObjectButton(chosen_object_children);
  EXPECT_EQ(button->GetState(), views::Button::STATE_DISABLED);

  views::Label* desc_label =
      GetChosenObjectDescriptionLabel(chosen_object_children);
  EXPECT_EQ(u"Serial port allowed by your administrator",
            desc_label->GetText());

  // Policy granted serial port permissions should not be able to be deleted.
  const ui::MouseEvent event(ui::EventType::kMousePressed, gfx::Point(),
                             gfx::Point(), ui::EventTimeForNow(), 0, 0);
  views::test::ButtonTestApi(button).NotifyClick(event);
  api_->SetPermissionInfo(list);
  EXPECT_EQ(kExpectedChildren + 1, api_->GetPermissionsCount());
}

// Test that updating the number of cookies used by the current page doesn't add
// any extra views to Page Info.
TEST_F(PageInfoBubbleViewTest, UpdatingSiteDataRetainsLayout) {
#if BUILDFLAG(IS_WIN) && BUILDFLAG(ENABLE_VR)
  size_t kExpectedChildren = 6;
#else
  size_t kExpectedChildren = 5;
#endif
  if (page_info::IsAboutThisSiteFeatureEnabled(
          g_browser_process->GetApplicationLocale())) {
    ++kExpectedChildren;
  }

  EXPECT_EQ(kExpectedChildren, api_->current_view()->children().size());

  // Create a fake cookies info.
  PageInfoUI::CookiesNewInfo cookies;
  cookies.allowed_sites_count = 10;
  cookies.protections_on = true;
  cookies.enforcement = CookieControlsEnforcement::kNoEnforcement;
  cookies.blocking_status = CookieBlocking3pcdStatus::kNotIn3pcd;

  // Update the cookies info.
  api_->SetCookieInfo(cookies);

  EXPECT_EQ(kExpectedChildren, api_->current_view()->children().size());
}

// Tests opening the bubble between navigation start and finish. The bubble
// should be updated to reflect the secure state after the navigation commits.
TEST_F(PageInfoBubbleViewTest, OpenPageInfoBubbleAfterNavigationStart) {
  ChromeSecurityStateTabHelper::CreateForWebContents(
      web_contents_helper_->web_contents());
  std::unique_ptr<content::NavigationSimulator> navigation =
      content::NavigationSimulator::CreateRendererInitiated(
          GURL(kSecureUrl),
          web_contents_helper_->web_contents()->GetPrimaryMainFrame());
  navigation->Start();
  api_->CreateView();
  EXPECT_EQ(kHostname, api_->GetWindowTitle());
  EXPECT_FALSE(api_->certificate_button());
  EXPECT_TRUE(api_->security_details_label());
  EXPECT_EQ(api_->GetSecuritySummaryText(),
            l10n_util::GetStringUTF16(IDS_PAGE_INFO_NOT_SECURE_SUMMARY));

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
  // In page info v2, in secure state description and learn more link aren't
  // shown on the main page.
  EXPECT_EQ(kHostname, api_->GetWindowTitle());
  EXPECT_FALSE(api_->security_details_label());
  EXPECT_EQ(api_->GetSecurityInformationButtonText(),
            l10n_util::GetStringUTF16(IDS_PAGE_INFO_SECURE_SUMMARY));

  api_->navigation_handler()->OpenSecurityPage();
  EXPECT_TRUE(api_->security_details_label());
}

TEST_F(PageInfoBubbleViewTest, EnsureCloseCallback) {
  api_->current_view()->GetWidget()->CloseWithReason(
      views::Widget::ClosedReason::kCloseButtonClicked);
  api_->WaitForBubbleClose();
  EXPECT_EQ(false, api_->reload_prompt());
  EXPECT_EQ(views::Widget::ClosedReason::kCloseButtonClicked,
            api_->closed_reason());
}

TEST_F(PageInfoBubbleViewTest, CheckHeaderInteractions) {
  // Confirm that interactions with the header tips are reported to the
  // sentiment service correctly.
  const ui::MouseEvent event(ui::EventType::kMousePressed, gfx::Point(),
                             gfx::Point(), ui::EventTimeForNow(), 0, 0);
  // Navigating to the security page constitutes an interaction.
  EXPECT_CALL(*mock_sentiment_service_, InteractedWithPageInfo).Times(3);
  api_->navigation_handler()->OpenSecurityPage();
  auto* page_view = static_cast<PageInfoSecurityContentView*>(
      api_->current_view()->children()[1]);
  page_view->SecurityDetailsClicked(event);
  page_view->ResetDecisionsClicked();
}

TEST_F(PageInfoBubbleViewTest, CertificateButtonShowsEvCertDetails) {
  ChromeSecurityStateTabHelper::CreateForWebContents(
      web_contents_helper_->web_contents());
  std::unique_ptr<content::NavigationSimulator> navigation =
      content::NavigationSimulator::CreateRendererInitiated(
          GURL(kSecureUrl),
          web_contents_helper_->web_contents()->GetPrimaryMainFrame());
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
  ssl_info.cert =
      net::ImportCertFromFile(net::GetTestCertsDirectory(), "ev_test.pem");
  ASSERT_TRUE(ssl_info.cert);
  ssl_info.cert_status = net::CERT_STATUS_IS_EV;

  navigation->SetSSLInfo(ssl_info);

  navigation->Commit();
  // In page info v2, in secure state certificate button isn't shown on the
  // main page.
  EXPECT_EQ(kHostname, api_->GetWindowTitle());
  EXPECT_FALSE(api_->certificate_button());
  EXPECT_FALSE(api_->security_summary_label());
  EXPECT_EQ(api_->GetSecurityInformationButtonText(),
            l10n_util::GetStringUTF16(IDS_PAGE_INFO_SECURE_SUMMARY));

  api_->navigation_handler()->OpenSecurityPage();
  EXPECT_TRUE(api_->certificate_button());
  EXPECT_EQ(l10n_util::GetStringUTF16(IDS_PAGE_INFO_SECURE_SUMMARY),
            api_->GetSecuritySummaryText());

  // The certificate button subtitle should show the EV certificate organization
  // name and country of incorporation.
  EXPECT_EQ(l10n_util::GetStringFUTF16(
                IDS_PAGE_INFO_SECURITY_TAB_SECURE_IDENTITY_EV_VERIFIED,
                u"Test Org", u"US"),
            api_->GetCertificateButtonSubtitleText());
}

// Regression test for crbug.com/1069113. Test cert includes country and state
// but not locality.
TEST_F(PageInfoBubbleViewTest, EvDetailsShowForCertWithStateButNoLocality) {
  ChromeSecurityStateTabHelper::CreateForWebContents(
      web_contents_helper_->web_contents());
  std::unique_ptr<content::NavigationSimulator> navigation =
      content::NavigationSimulator::CreateRendererInitiated(
          GURL(kSecureUrl),
          web_contents_helper_->web_contents()->GetPrimaryMainFrame());
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
  ssl_info.cert = net::ImportCertFromFile(net::GetTestCertsDirectory(),
                                          "ev_test_state_only.pem");
  ASSERT_TRUE(ssl_info.cert);

  ssl_info.cert_status = net::CERT_STATUS_IS_EV;

  navigation->SetSSLInfo(ssl_info);

  navigation->Commit();
  // In page info v2, in secure state certificate button isn't shown on the
  // main page.
  EXPECT_EQ(kHostname, api_->GetWindowTitle());
  EXPECT_FALSE(api_->certificate_button());
  EXPECT_FALSE(api_->security_summary_label());
  EXPECT_EQ(api_->GetSecurityInformationButtonText(),
            l10n_util::GetStringUTF16(IDS_PAGE_INFO_SECURE_SUMMARY));

  api_->navigation_handler()->OpenSecurityPage();
  EXPECT_TRUE(api_->certificate_button());
  EXPECT_EQ(l10n_util::GetStringUTF16(IDS_PAGE_INFO_SECURE_SUMMARY),
            api_->GetSecuritySummaryText());

  // The certificate button subtitle should show the EV certificate organization
  // name and country of incorporation.
  EXPECT_EQ(l10n_util::GetStringFUTF16(
                IDS_PAGE_INFO_SECURITY_TAB_SECURE_IDENTITY_EV_VERIFIED,
                u"Test Org", u"US"),
            api_->GetCertificateButtonSubtitleText());
}

class PageInfoBubbleViewCookies3pcdButtonTest
    : public PageInfoBubbleViewTest,
      public testing::WithParamInterface<bool> {
 public:
  PageInfoBubbleViewCookies3pcdButtonTest() {
    feature_list_.InitWithFeatures(
        {content_settings::features::kTrackingProtection3pcd,
         privacy_sandbox::kTrackingProtection3pcdUx},
        {});
    web_contents_helper_ =
        std::make_unique<ScopedWebContentsTestHelper>(GetParam());
  }

 protected:
  void NavigateToPage(content::WebContents* web_contents,
                      const std::string& url) {
    web_contents->GetController().LoadURL(GURL(url), content::Referrer(),
                                          ui::PAGE_TRANSITION_FROM_ADDRESS_BAR,
                                          std::string());
    content::RenderFrameHostTester::CommitPendingLoad(
        &web_contents->GetController());
  }

  void CreateCookieExceptionForSite(const std::string& pattern) {
    auto top_level_domain_pattern = ContentSettingsPattern::FromString(pattern);
    HostContentSettingsMapFactory::GetForProfile(
        web_contents_helper_->profile())
        ->SetContentSettingCustomScope(ContentSettingsPattern::Wildcard(),
                                       top_level_domain_pattern,
                                       ContentSettingsType::COOKIES,
                                       ContentSetting::CONTENT_SETTING_ALLOW);
  }

 private:
  base::test::ScopedFeatureList feature_list_;
};

TEST_P(PageInfoBubbleViewCookies3pcdButtonTest,
       DisplaysTrackingProtectionButtonLabelsWhen3pcLimited) {
  EXPECT_EQ(api_->GetTrackingProtectionButtonTitleText(),
            l10n_util::GetStringUTF16(
                IDS_PAGE_INFO_TRACKING_PROTECTION_SITE_INFO_BUTTON_NAME));
  // 3PC are blocked in incognito even if limited in regular profile
  EXPECT_EQ(
      api_->GetTrackingProtectionButtonSubTitleText(),
      l10n_util::GetStringUTF16(
          GetParam()
              ? IDS_PAGE_INFO_TRACKING_PROTECTION_SITE_INFO_BUTTON_LABEL_BLOCKED
              : IDS_PAGE_INFO_TRACKING_PROTECTION_SITE_INFO_BUTTON_LABEL_LIMITED));
}

TEST_P(PageInfoBubbleViewCookies3pcdButtonTest,
       DisplaysTrackingProtectionButtonLabelsWhen3pcBlocked) {
  // Block all 3PC
  web_contents_helper_->profile()->GetPrefs()->SetBoolean(
      prefs::kBlockAll3pcToggleEnabled, true);
  // Rerender with the new pref set
  api_->CreateView();

  EXPECT_EQ(api_->GetTrackingProtectionButtonTitleText(),
            l10n_util::GetStringUTF16(
                IDS_PAGE_INFO_TRACKING_PROTECTION_SITE_INFO_BUTTON_NAME));
  EXPECT_EQ(
      api_->GetTrackingProtectionButtonSubTitleText(),
      l10n_util::GetStringUTF16(
          IDS_PAGE_INFO_TRACKING_PROTECTION_SITE_INFO_BUTTON_LABEL_BLOCKED));
}

TEST_P(
    PageInfoBubbleViewCookies3pcdButtonTest,
    DisplaysTrackingProtectionButtonLabelsWhenCookiesAllowedViaSiteException) {
  // Add a new cookies site exception for kUrl.
  CreateCookieExceptionForSite(std::string("[*.]example.com"));
  // Navigate to a page with the new site exception and rerender.
  NavigateToPage(web_contents_helper_->web_contents(), kUrl);
  api_->CreateView();

  EXPECT_EQ(api_->GetTrackingProtectionButtonTitleText(),
            l10n_util::GetStringUTF16(
                IDS_PAGE_INFO_TRACKING_PROTECTION_SITE_INFO_BUTTON_NAME));
  EXPECT_EQ(
      api_->GetTrackingProtectionButtonSubTitleText(),
      l10n_util::GetStringUTF16(
          IDS_PAGE_INFO_TRACKING_PROTECTION_SITE_INFO_BUTTON_LABEL_ALLOWED));
}

INSTANTIATE_TEST_SUITE_P(All,
                         PageInfoBubbleViewCookies3pcdButtonTest,
                         /*is_otr*/ testing::Bool());

class PageInfoBubbleViewTrackingProtectionSubpageTitleTest
    : public PageInfoBubbleViewTest,
      public testing::WithParamInterface<
          testing::tuple</*protections_on*/ bool,
                         CookieBlocking3pcdStatus,
                         /*is_otr*/ bool>> {
 public:
  PageInfoBubbleViewTrackingProtectionSubpageTitleTest() {
    feature_list_.InitWithFeatures(
        {content_settings::features::kTrackingProtection3pcd,
         privacy_sandbox::kTrackingProtection3pcdUx},
        {});
    web_contents_helper_ = std::make_unique<ScopedWebContentsTestHelper>(
        testing::get<2>(GetParam()));
  }

 private:
  base::test::ScopedFeatureList feature_list_;
};

TEST_P(PageInfoBubbleViewTrackingProtectionSubpageTitleTest,
       DisplaysTrackingProtectionTitle) {
  PageInfoUI::CookiesNewInfo cookie_info;
  cookie_info.protections_on = testing::get<0>(GetParam());
  cookie_info.blocking_status = testing::get<1>(GetParam());
  api_->SetCookieInfo(cookie_info);
  EXPECT_EQ(api_->GetTrackingProtectionSubpageTitle(),
            l10n_util::GetStringUTF16(
                IDS_PAGE_INFO_SUB_PAGE_VIEW_TRACKING_PROTECTION_HEADER));
}

INSTANTIATE_TEST_SUITE_P(
    All,
    PageInfoBubbleViewTrackingProtectionSubpageTitleTest,
    testing::Combine(/*protections_on*/ testing::Bool(),
                     testing::Values(CookieBlocking3pcdStatus::kNotIn3pcd,
                                     CookieBlocking3pcdStatus::kAll),
                     /*is_otr*/ testing::Bool()));
