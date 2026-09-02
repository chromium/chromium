// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/omnibox/omnibox_context_menu_controller.h"

#include <memory>
#include <utility>

#include "build/branding_buildflags.h"
#include "chrome/app/chrome_command_ids.h"
#include "chrome/app/vector_icons/vector_icons.h"
#include "chrome/browser/optimization_guide/mock_optimization_guide_keyed_service.h"
#include "chrome/browser/optimization_guide/optimization_guide_keyed_service_factory.h"
#include "chrome/browser/ui/browser_window/public/browser_window_features.h"
#include "chrome/browser/ui/browser_window/test/mock_browser_window_interface.h"
#include "chrome/browser/ui/omnibox/omnibox_controller.h"
#include "chrome/browser/ui/omnibox/omnibox_next_features.h"
#include "chrome/browser/ui/omnibox/omnibox_popup_state_manager.h"
#include "chrome/browser/ui/omnibox/omnibox_tab_helper.h"
#include "chrome/browser/ui/omnibox/test_omnibox_popup_file_selector.h"
#include "chrome/browser/ui/omnibox/test_omnibox_popup_view.h"
#include "chrome/browser/ui/views/location_bar/omnibox_popup_file_selector.h"
#include "chrome/browser/ui/webui/cr_components/searchbox/contextual_searchbox_handler.h"
#include "chrome/browser/ui/webui/omnibox_popup/omnibox_popup_web_contents_helper.h"
#include "chrome/browser/ui/webui/webui_embedding_context.h"
#include "chrome/test/base/testing_profile.h"
#include "components/contextual_search/contextual_search_types.h"
#include "components/contextual_tasks/public/features.h"
#include "components/lens/lens_overlay_mime_type.h"
#include "components/omnibox/browser/test_omnibox_client.h"
#include "components/omnibox/common/composebox_features.h"
#include "components/policy/core/common/mock_configuration_policy_provider.h"
#include "components/policy/core/common/policy_service_impl.h"
#include "components/vector_icons/vector_icons.h"
#include "content/public/browser/web_contents.h"
#include "content/public/test/browser_task_environment.h"
#include "content/public/test/test_renderer_host.h"
#include "content/public/test/web_contents_tester.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/omnibox_proto/input_type.pb.h"
#include "third_party/omnibox_proto/tool_mode.pb.h"
#include "ui/base/models/image_model.h"
#include "ui/base/ui_base_features.h"

class FakeContextualSearchboxHandler : public ContextualSearchboxHandler {
 public:
  FakeContextualSearchboxHandler(Profile* profile,
                                 content::WebContents* web_contents)
      : ContextualSearchboxHandler(
            mojo::PendingReceiver<searchbox::mojom::PageHandler>(),
            mojo::PendingRemote<searchbox::mojom::Page>(),
            profile,
            web_contents,
            nullptr,
            base::NullCallback()) {}
  ~FakeContextualSearchboxHandler() override = default;

  bool IsSmartTabSharingActive() const override { return active_; }
  void SetSmartTabSharingActive(bool active) override { active_ = active; }

  void OnThumbnailRemoved() override {}

  bool active_ = false;
};

class TestOmniboxContextMenuController : public OmniboxContextMenuController {
 public:
  using OmniboxContextMenuController::GetIconForInputType;
  using OmniboxContextMenuController::GetIconForModel;
  using OmniboxContextMenuController::OmniboxContextMenuController;
  using OmniboxContextMenuController::OnGetInputState;

  ContextualSearchboxHandler* GetContextualSearchboxHandler() const override {
    return handler_;
  }

  void SetContextualSearchboxHandler(ContextualSearchboxHandler* handler) {
    handler_ = handler;
  }

  OmniboxPopupUI* GetOmniboxPopupUI() const override { return nullptr; }

  std::vector<OmniboxContextMenuController::TabInfo> GetRecentTabs()
      const override {
    return mock_tabs_;
  }

  void SetMockTabs(std::vector<OmniboxContextMenuController::TabInfo> tabs) {
    mock_tabs_ = std::move(tabs);
  }

  void RebuildMenu() {
    menu_model_ = std::make_unique<TabSimpleMenuModel>(this);
    shared_tabs_menu_model_.reset();
    BuildMenu();
  }

  bool IsContentSharingEnabled() const override {
    return is_content_sharing_enabled_;
  }

  void SetContentSharingEnabled(bool enabled) {
    is_content_sharing_enabled_ = enabled;
  }

  bool IsTabContextEnabled() const override { return is_tab_context_enabled_; }

  void SetTabContextEnabled(bool enabled) { is_tab_context_enabled_ = enabled; }

 private:
  raw_ptr<ContextualSearchboxHandler> handler_ = nullptr;
  std::vector<OmniboxContextMenuController::TabInfo> mock_tabs_;
  bool is_content_sharing_enabled_ = true;
  bool is_tab_context_enabled_ = true;
};

class OmniboxContextMenuControllerTest : public testing::Test {
 public:
  OmniboxContextMenuControllerTest() {
    policy_provider_.Init();
    policy::PolicyServiceImpl::Providers providers = {&policy_provider_};
    TestingProfile::Builder builder;
    builder.SetPolicyService(
        std::make_unique<policy::PolicyServiceImpl>(std::move(providers)));
    profile_ = builder.Build();

    OptimizationGuideKeyedServiceFactory::GetInstance()->SetTestingFactory(
        profile_.get(),
        base::BindRepeating([](content::BrowserContext* context)
                                -> std::unique_ptr<KeyedService> {
          return std::make_unique<
              testing::NiceMock<MockOptimizationGuideKeyedService>>();
        }));
    file_selector_ =
        std::make_unique<TestOmniboxPopupFileSelector>(gfx::NativeWindow());
    web_contents_ = content::WebContentsTester::CreateTestWebContents(
        profile_.get(), nullptr);
    ON_CALL(browser_window_interface_, GetProfile())
        .WillByDefault(testing::Return(profile_.get()));
    ON_CALL(browser_window_interface_, GetFeatures())
        .WillByDefault(testing::ReturnRef(browser_window_features_));
    webui::SetBrowserWindowInterface(web_contents_.get(),
                                     &browser_window_interface_);
    OmniboxPopupWebContentsHelper::CreateForWebContents(web_contents_.get());
    omnibox_controller_ = std::make_unique<OmniboxController>(
        std::make_unique<TestOmniboxClient>(), std::nullopt);
    auto* client =
        static_cast<TestOmniboxClient*>(omnibox_controller_->client());
    ON_CALL(*client, IsAimPopupEnabled()).WillByDefault(testing::Return(true));
    OmniboxPopupWebContentsHelper::FromWebContents(web_contents_.get())
        ->set_omnibox_controller(omnibox_controller_.get());
    omnibox_controller_->edit_model()->set_popup_view(&popup_view_);
    controller_ = std::make_unique<TestOmniboxContextMenuController>(
        file_selector_.get(), web_contents_.get());
  }

  void TearDown() override {
    policy_provider_.Shutdown();
    if (web_contents_) {
      auto* helper =
          OmniboxPopupWebContentsHelper::FromWebContents(web_contents_.get());
      if (helper) {
        helper->set_omnibox_controller(nullptr);
      }
    }
  }

  TestOmniboxContextMenuController* controller() { return controller_.get(); }

 protected:
  content::BrowserTaskEnvironment task_environment_;
  content::RenderViewHostTestEnabler rvh_test_enabler_;

  testing::NiceMock<policy::MockConfigurationPolicyProvider> policy_provider_;
  std::unique_ptr<TestingProfile> profile_;
  testing::NiceMock<MockBrowserWindowInterface> browser_window_interface_;
  BrowserWindowFeatures browser_window_features_;

  std::unique_ptr<OmniboxPopupFileSelector> file_selector_;
  // `omnibox_controller_` must outlive `web_contents_`, as `web_contents_`
  // holds an `OmniboxPopupWebContentsHelper` with a raw_ptr to
  // `omnibox_controller_`.
  std::unique_ptr<OmniboxController> omnibox_controller_;
  TestOmniboxPopupView popup_view_;
  std::unique_ptr<content::WebContents> web_contents_;
  std::unique_ptr<TestOmniboxContextMenuController> controller_;
};

TEST_F(OmniboxContextMenuControllerTest,
       IsCommandIdEnabledHelper_InitialState) {
  std::vector<contextual_search::FileInfo> file_infos;
  int max_num_files = 5;

  EXPECT_TRUE(controller()->IsCommandIdEnabledHelper(
      IDC_OMNIBOX_CONTEXT_ADD_IMAGE, omnibox::ToolMode::TOOL_MODE_UNSPECIFIED,
      file_infos, max_num_files, OmniboxPopupState::kNone));
  EXPECT_TRUE(controller()->IsCommandIdEnabledHelper(
      IDC_OMNIBOX_CONTEXT_ADD_FILE, omnibox::ToolMode::TOOL_MODE_UNSPECIFIED,
      file_infos, max_num_files, OmniboxPopupState::kNone));
  EXPECT_TRUE(controller()->IsCommandIdEnabledHelper(
      IDC_OMNIBOX_CONTEXT_DEEP_RESEARCH,
      omnibox::ToolMode::TOOL_MODE_UNSPECIFIED, file_infos, max_num_files,
      OmniboxPopupState::kNone));
  EXPECT_TRUE(controller()->IsCommandIdEnabledHelper(
      IDC_OMNIBOX_CONTEXT_CREATE_IMAGES,
      omnibox::ToolMode::TOOL_MODE_UNSPECIFIED, file_infos, max_num_files,
      OmniboxPopupState::kNone));
  EXPECT_TRUE(controller()->IsCommandIdEnabledHelper(
      IDC_OMNIBOX_CONTEXT_CANVAS, omnibox::ToolMode::TOOL_MODE_UNSPECIFIED,
      file_infos, max_num_files, OmniboxPopupState::kNone));
}

TEST_F(OmniboxContextMenuControllerTest,
       IsCommandIdEnabledHelper_ImageGenMode) {
  std::vector<contextual_search::FileInfo> file_infos;
  int max_num_files = 5;

  EXPECT_TRUE(controller()->IsCommandIdEnabledHelper(
      IDC_OMNIBOX_CONTEXT_ADD_IMAGE, omnibox::ToolMode::TOOL_MODE_IMAGE_GEN,
      file_infos, max_num_files, OmniboxPopupState::kNone));
  EXPECT_FALSE(controller()->IsCommandIdEnabledHelper(
      IDC_OMNIBOX_CONTEXT_ADD_FILE, omnibox::ToolMode::TOOL_MODE_IMAGE_GEN,
      file_infos, max_num_files, OmniboxPopupState::kNone));
}

TEST_F(OmniboxContextMenuControllerTest,
       IsCommandIdEnabledHelper_WithImageFile) {
  std::vector<contextual_search::FileInfo> file_infos;
  int max_num_files = 5;

  contextual_search::FileInfo image_file;
  image_file.mime_type = lens::MimeType::kImage;
  file_infos.push_back(image_file);

  EXPECT_FALSE(controller()->IsCommandIdEnabledHelper(
      IDC_OMNIBOX_CONTEXT_DEEP_RESEARCH,
      omnibox::ToolMode::TOOL_MODE_UNSPECIFIED, file_infos, max_num_files,
      OmniboxPopupState::kNone));
  EXPECT_TRUE(controller()->IsCommandIdEnabledHelper(
      IDC_OMNIBOX_CONTEXT_CREATE_IMAGES,
      omnibox::ToolMode::TOOL_MODE_UNSPECIFIED, file_infos, max_num_files,
      OmniboxPopupState::kNone));
  EXPECT_FALSE(controller()->IsCommandIdEnabledHelper(
      IDC_OMNIBOX_CONTEXT_CANVAS, omnibox::ToolMode::TOOL_MODE_UNSPECIFIED,
      file_infos, max_num_files, OmniboxPopupState::kNone));
  EXPECT_TRUE(controller()->IsCommandIdEnabledHelper(
      IDC_OMNIBOX_CONTEXT_ADD_IMAGE, omnibox::ToolMode::TOOL_MODE_UNSPECIFIED,
      file_infos, max_num_files, OmniboxPopupState::kNone));
}

TEST_F(OmniboxContextMenuControllerTest,
       IsCommandIdEnabledHelper_WithNonImageFile) {
  std::vector<contextual_search::FileInfo> file_infos;
  int max_num_files = 5;

  contextual_search::FileInfo pdf_file;
  pdf_file.mime_type = lens::MimeType::kPdf;
  file_infos.push_back(pdf_file);

  EXPECT_FALSE(controller()->IsCommandIdEnabledHelper(
      IDC_OMNIBOX_CONTEXT_CREATE_IMAGES,
      omnibox::ToolMode::TOOL_MODE_UNSPECIFIED, file_infos, max_num_files,
      OmniboxPopupState::kNone));
  EXPECT_FALSE(controller()->IsCommandIdEnabledHelper(
      IDC_OMNIBOX_CONTEXT_CANVAS, omnibox::ToolMode::TOOL_MODE_UNSPECIFIED,
      file_infos, max_num_files, OmniboxPopupState::kNone));
  EXPECT_TRUE(controller()->IsCommandIdEnabledHelper(
      IDC_OMNIBOX_CONTEXT_ADD_IMAGE, omnibox::ToolMode::TOOL_MODE_UNSPECIFIED,
      file_infos, max_num_files, OmniboxPopupState::kNone));
}

TEST_F(OmniboxContextMenuControllerTest, IsCommandIdEnabledHelper_MaxFiles) {
  std::vector<contextual_search::FileInfo> file_infos;
  int max_num_files = 5;
  contextual_search::FileInfo image_file;
  image_file.mime_type = lens::MimeType::kImage;

  for (int i = 0; i < max_num_files; ++i) {
    file_infos.push_back(image_file);
  }
  EXPECT_FALSE(controller()->IsCommandIdEnabledHelper(
      IDC_OMNIBOX_CONTEXT_ADD_IMAGE, omnibox::ToolMode::TOOL_MODE_UNSPECIFIED,
      file_infos, max_num_files, OmniboxPopupState::kNone));
  EXPECT_FALSE(controller()->IsCommandIdEnabledHelper(
      IDC_OMNIBOX_CONTEXT_ADD_FILE, omnibox::ToolMode::TOOL_MODE_UNSPECIFIED,
      file_infos, max_num_files, OmniboxPopupState::kNone));
}

TEST_F(OmniboxContextMenuControllerTest, GetMaxTabSuggestions_UsesServerLimit) {
  base::FieldTrialParams params;
  params[omnibox::kContextMenuMaxTabSuggestions.name] = "2";
  base::test::ScopedFeatureList feature_list;
  feature_list.InitAndEnableFeatureWithParameters(
      omnibox::internal::kWebUIOmniboxAimPopup, params);

  // Initially should use feature param limit.
  EXPECT_EQ(controller()->GetMaxTabSuggestions(), std::optional<size_t>(2u));

  // Set server-provided limit.
  omnibox::InputState state;
  state.max_inputs_by_type[omnibox::InputType::INPUT_TYPE_BROWSER_TAB] = 1;
  controller()->OnGetInputState(state);

  EXPECT_EQ(controller()->GetMaxTabSuggestions(), std::optional<size_t>(1u));

  // Fallback to feature param limit if not in map.
  state.max_inputs_by_type.erase(omnibox::InputType::INPUT_TYPE_BROWSER_TAB);
  controller()->OnGetInputState(state);

  EXPECT_EQ(controller()->GetMaxTabSuggestions(), std::optional<size_t>(2u));
}

TEST_F(OmniboxContextMenuControllerTest, GetIconForInputType_Drive) {
#if BUILDFLAG(GOOGLE_CHROME_BRANDING)
  ui::ImageModel expected_icon = ui::ImageModel::FromVectorIcon(
      vector_icons::kGoogleDriveMonochromeIcon, ui::kColorMenuIcon,
      ui::SimpleMenuModel::kDefaultIconSize);
#else
  ui::ImageModel expected_icon = ui::ImageModel();
#endif
  EXPECT_EQ(
      controller()->GetIconForInputType(omnibox::InputType::INPUT_TYPE_DRIVE),
      expected_icon);
}

TEST_F(OmniboxContextMenuControllerTest,
       GetIconForModel_UseSearchboxConfigIconIds) {
  base::test::ScopedFeatureList feature_list;
  feature_list.InitAndEnableFeature(omnibox::kAimUseSearchboxConfigIconIds);

  omnibox::InputState state;
  omnibox::ModelConfig regular_config;
  regular_config.set_model(omnibox::ModelMode::MODEL_MODE_GEMINI_REGULAR);
  regular_config.mutable_icon()->set_icon_id(omnibox::IconResourceIds::BOLT);
  state.model_configs.push_back(regular_config);

  controller()->OnGetInputState(state);

  ui::ImageModel expected_icon = ui::ImageModel::FromVectorIcon(
      features::IsRoundedIconsEnabled() ? kBoltIcon : kBoltOldIcon,
      ui::kColorMenuIcon, ui::SimpleMenuModel::kDefaultIconSize);
  EXPECT_EQ(controller()->GetIconForModel(
                omnibox::ModelMode::MODEL_MODE_GEMINI_REGULAR),
            expected_icon);

  omnibox::ModelConfig drive_config;
  drive_config.set_model(omnibox::ModelMode::MODEL_MODE_GEMINI_PRO);
  drive_config.mutable_icon()->set_icon_id(omnibox::IconResourceIds::DRIVE);
  state.model_configs.push_back(drive_config);

  omnibox::ModelConfig photo_prints_config;
  photo_prints_config.set_model(
      omnibox::ModelMode::MODEL_MODE_GEMINI_PRO_AUTOROUTE);
  photo_prints_config.mutable_icon()->set_icon_id(
      omnibox::IconResourceIds::PHOTO_PRINTS);
  state.model_configs.push_back(photo_prints_config);

  controller()->OnGetInputState(state);

#if BUILDFLAG(GOOGLE_CHROME_BRANDING)
  ui::ImageModel expected_drive_icon = ui::ImageModel::FromVectorIcon(
      vector_icons::kGoogleDriveMonochromeIcon, ui::kColorMenuIcon,
      ui::SimpleMenuModel::kDefaultIconSize);
#else
  ui::ImageModel expected_drive_icon = ui::ImageModel();
#endif
  EXPECT_EQ(controller()->GetIconForModel(
                omnibox::ModelMode::MODEL_MODE_GEMINI_PRO),
            expected_drive_icon);
  EXPECT_EQ(controller()->GetIconForModel(
                omnibox::ModelMode::MODEL_MODE_GEMINI_PRO_AUTOROUTE),
            ui::ImageModel());
}

TEST_F(OmniboxContextMenuControllerTest, GetIconForModel_LegacyFallback) {
  base::test::ScopedFeatureList feature_list;
  feature_list.InitAndDisableFeature(omnibox::kAimUseSearchboxConfigIconIds);

  omnibox::InputState state;
  omnibox::ModelConfig regular_config;
  regular_config.set_model(omnibox::ModelMode::MODEL_MODE_GEMINI_REGULAR);
  regular_config.mutable_icon()->set_icon_id(omnibox::IconResourceIds::BOLT);
  state.model_configs.push_back(regular_config);

  controller()->OnGetInputState(state);

  ui::ImageModel expected_legacy_icon = ui::ImageModel::FromVectorIcon(
      kAcuteIcon, ui::kColorMenuIcon, ui::SimpleMenuModel::kDefaultIconSize);
  EXPECT_EQ(controller()->GetIconForModel(
                omnibox::ModelMode::MODEL_MODE_GEMINI_REGULAR),
            expected_legacy_icon);
}

TEST_F(OmniboxContextMenuControllerTest, ExecuteCommand_DriveInputType) {
  base::test::ScopedFeatureList feature_list;
  feature_list.InitWithFeatures(
      {omnibox::kAimUsePecApi, omnibox::kComposeboxDriveContextMenuOption}, {});

  omnibox::InputState state;
  state.allowed_input_types.push_back(omnibox::InputType::INPUT_TYPE_DRIVE);
  controller()->OnGetInputState(state);
  controller()->AddContextualInputItems();

  // Find the command ID for DRIVE.
  int drive_command_id = -1;
  for (const auto& pair : controller()->input_type_for_command_id_) {
    if (pair.second == omnibox::InputType::INPUT_TYPE_DRIVE) {
      drive_command_id = pair.first;
      break;
    }
  }
  ASSERT_NE(drive_command_id, -1);

  TestOmniboxPopupFileSelector* test_selector =
      static_cast<TestOmniboxPopupFileSelector*>(file_selector_.get());
  int initial_calls = test_selector->open_file_upload_dialog_calls();

  // Execute command.
  controller()->ExecuteCommand(drive_command_id, 0);

  // Verify that OpenFileUploadDialog was NOT called.
  EXPECT_EQ(test_selector->open_file_upload_dialog_calls(), initial_calls);
}

TEST_F(OmniboxContextMenuControllerTest, IsTabCommandId_HandlesInfinity) {
  // Test 1: Feature param limit = 2
  {
    base::test::ScopedFeatureList feature_list;
    base::FieldTrialParams params;
    params[omnibox::kContextMenuMaxTabSuggestions.name] = "2";
    feature_list.InitAndEnableFeatureWithParameters(
        omnibox::internal::kWebUIOmniboxAimPopup, params);

    EXPECT_EQ(controller()->GetMaxTabSuggestions(), std::optional<size_t>(2u));
    EXPECT_TRUE(controller()->IsTabCommandId(33000));
    EXPECT_TRUE(controller()->IsTabCommandId(33001));
    EXPECT_FALSE(controller()->IsTabCommandId(33002));
    EXPECT_FALSE(controller()->IsTabCommandId(32999));
    EXPECT_FALSE(controller()->IsTabCommandId(54010));
  }

  // Test 2: ContextManagementInComposebox and ContextManagementInOmnibox
  // features enabled -> returns std::nullopt
  {
    base::test::ScopedFeatureList feature_list;
    feature_list.InitWithFeatures({omnibox::kContextManagementInComposebox,
                                   omnibox::kContextManagementInOmnibox},
                                  {});

    EXPECT_EQ(controller()->GetMaxTabSuggestions(), std::nullopt);
    EXPECT_TRUE(controller()->IsTabCommandId(33000));
    EXPECT_TRUE(controller()->IsTabCommandId(33001));
    EXPECT_TRUE(controller()->IsTabCommandId(33002));
    EXPECT_TRUE(controller()->IsTabCommandId(33005));
    EXPECT_FALSE(controller()->IsTabCommandId(32999));
    EXPECT_FALSE(controller()->IsTabCommandId(54010));
  }

  // Test 3: InputState limit = -1 -> returns std::nullopt
  {
    omnibox::InputState state;
    state.max_inputs_by_type[omnibox::InputType::INPUT_TYPE_BROWSER_TAB] = -1;
    controller()->OnGetInputState(state);

    EXPECT_EQ(controller()->GetMaxTabSuggestions(), std::nullopt);
    EXPECT_TRUE(controller()->IsTabCommandId(33000));
    EXPECT_TRUE(controller()->IsTabCommandId(33001));
    EXPECT_TRUE(controller()->IsTabCommandId(33002));
    EXPECT_TRUE(controller()->IsTabCommandId(33005));
    EXPECT_FALSE(controller()->IsTabCommandId(32999));
    EXPECT_FALSE(controller()->IsTabCommandId(54010));
  }
}

TEST_F(OmniboxContextMenuControllerTest, SmartTabSharingTogglesState) {
  FakeContextualSearchboxHandler fake_handler(profile_.get(),
                                              web_contents_.get());
  controller()->SetContextualSearchboxHandler(&fake_handler);

  OmniboxPopupWebContentsHelper::CreateForWebContents(web_contents_.get());
  fake_handler.active_ = false;

  // Execute toggle command
  controller()->ExecuteCommand(IDC_OMNIBOX_CONTEXT_SMART_TAB_SHARING, 0);

  // Verification: state is toggled to true
  EXPECT_TRUE(fake_handler.IsSmartTabSharingActive());

  // Execute toggle command again
  controller()->ExecuteCommand(IDC_OMNIBOX_CONTEXT_SMART_TAB_SHARING, 0);

  // Verification: state is toggled back to false
  EXPECT_FALSE(fake_handler.IsSmartTabSharingActive());
}

TEST_F(OmniboxContextMenuControllerTest,
       SharedTabsSubmenuDisabledWhenSmartTabSharingActive) {
  base::test::ScopedFeatureList feature_list;
  feature_list.InitWithFeatures({omnibox::kContextManagementInComposebox,
                                 omnibox::kContextManagementInOmnibox},
                                {});

  FakeContextualSearchboxHandler fake_handler(profile_.get(),
                                              web_contents_.get());
  controller()->SetContextualSearchboxHandler(&fake_handler);

  // 1. Smart Tab Sharing NOT active -> submenu is enabled
  fake_handler.active_ = false;
  EXPECT_TRUE(controller()->IsCommandIdEnabled(
      IDC_OMNIBOX_CONTEXT_SHARED_TABS_SUBMENU));

  // 2. Smart Tab Sharing active -> submenu is disabled
  fake_handler.active_ = true;
  EXPECT_FALSE(controller()->IsCommandIdEnabled(
      IDC_OMNIBOX_CONTEXT_SHARED_TABS_SUBMENU));
}

TEST_F(OmniboxContextMenuControllerTest,
       SmartTabSharingToggleLayoutWhenActive) {
  base::test::ScopedFeatureList feature_list;
  feature_list.InitFromCommandLine(
      "ContextManagementInComposebox,ContextManagementInOmnibox,"
      "ContextualTasksForceEntryPointEligibility,"
      "ContextualTasksContext<ContextualTasksContextStudy."
      "ContextualTasksContextGroup:ContextualTasksContextSmartTabSharing/true",
      "AimUsePecApi");

  FakeContextualSearchboxHandler fake_handler(profile_.get(),
                                              web_contents_.get());
  controller()->SetContextualSearchboxHandler(&fake_handler);

  // Set up mock tabs so AddRecentTabItems compiles the tab sections
  std::vector<OmniboxContextMenuController::TabInfo> mock_tabs;
  OmniboxContextMenuController::TabInfo tab;
  tab.tab_id = 1;
  tab.title = u"Tab 1";
  tab.url = GURL("https://example.com");
  mock_tabs.push_back(tab);
  controller()->SetMockTabs(mock_tabs);

  // 1. Set Smart Tab Sharing active
  fake_handler.active_ = true;
  controller()->RebuildMenu();

  // 2. Verify: Toggle is in main menu
  EXPECT_TRUE(controller()
                  ->menu_model()
                  ->GetIndexOfCommandId(IDC_OMNIBOX_CONTEXT_SMART_TAB_SHARING)
                  .has_value());

  // 3. Verify: Submenu is NOT in main menu
  EXPECT_FALSE(
      controller()
          ->menu_model()
          ->GetIndexOfCommandId(IDC_OMNIBOX_CONTEXT_SHARED_TABS_SUBMENU)
          .has_value());

  // 4. Verify: Minor icon (checkmark on the right) is present
  size_t index =
      controller()
          ->menu_model()
          ->GetIndexOfCommandId(IDC_OMNIBOX_CONTEXT_SMART_TAB_SHARING)
          .value();
  EXPECT_FALSE(controller()->menu_model()->GetMinorIconAt(index).IsEmpty());

  // 5. Verify: Left icon has the screensaver auto vector icon
  ui::ImageModel icon = controller()->menu_model()->GetIconAt(index);
  EXPECT_FALSE(icon.IsEmpty());
  EXPECT_TRUE(icon.IsVectorIcon());
  EXPECT_EQ(icon.GetVectorIcon().vector_icon(), &kScreensaverAutoIcon);
}

TEST_F(OmniboxContextMenuControllerTest,
       SmartTabSharingToggleLayoutWhenInactive) {
  base::test::ScopedFeatureList feature_list;
  feature_list.InitFromCommandLine(
      "ContextManagementInComposebox,ContextManagementInOmnibox,"
      "ContextualTasksForceEntryPointEligibility,"
      "ContextualTasksContext<ContextualTasksContextStudy."
      "ContextualTasksContextGroup:ContextualTasksContextSmartTabSharing/true",
      "AimUsePecApi");

  FakeContextualSearchboxHandler fake_handler(profile_.get(),
                                              web_contents_.get());
  controller()->SetContextualSearchboxHandler(&fake_handler);

  // Set up mock tabs
  std::vector<OmniboxContextMenuController::TabInfo> mock_tabs;
  OmniboxContextMenuController::TabInfo tab;
  tab.tab_id = 1;
  tab.title = u"Tab 1";
  tab.url = GURL("https://example.com");
  mock_tabs.push_back(tab);
  controller()->SetMockTabs(mock_tabs);

  // 1. Set Smart Tab Sharing inactive
  fake_handler.active_ = false;
  controller()->RebuildMenu();

  // 2. Verify: Toggle is NOT in main menu
  EXPECT_FALSE(controller()
                   ->menu_model()
                   ->GetIndexOfCommandId(IDC_OMNIBOX_CONTEXT_SMART_TAB_SHARING)
                   .has_value());

  // 3. Verify: Submenu IS in main menu
  EXPECT_TRUE(controller()
                  ->menu_model()
                  ->GetIndexOfCommandId(IDC_OMNIBOX_CONTEXT_SHARED_TABS_SUBMENU)
                  .has_value());

  // 4. Verify: Toggle IS in submenu at index 0
  ASSERT_TRUE(controller()->shared_tabs_menu_model());
  EXPECT_EQ(controller()->shared_tabs_menu_model()->GetIndexOfCommandId(
                IDC_OMNIBOX_CONTEXT_SMART_TAB_SHARING),
            0u);

  // 5. Verify: Minor icon is empty (unchecked)
  size_t index =
      controller()
          ->shared_tabs_menu_model()
          ->GetIndexOfCommandId(IDC_OMNIBOX_CONTEXT_SMART_TAB_SHARING)
          .value();
  EXPECT_TRUE(
      controller()->shared_tabs_menu_model()->GetMinorIconAt(index).IsEmpty());

  // 6. Verify: Left icon has the screensaver auto vector icon
  ui::ImageModel icon =
      controller()->shared_tabs_menu_model()->GetIconAt(index);
  EXPECT_FALSE(icon.IsEmpty());
  EXPECT_TRUE(icon.IsVectorIcon());
  EXPECT_EQ(icon.GetVectorIcon().vector_icon(), &kScreensaverAutoIcon);
}

TEST_F(OmniboxContextMenuControllerTest,
       SmartTabSharingToggleLayoutWhenDisabled) {
  base::test::ScopedFeatureList feature_list;
  feature_list.InitWithFeatures(
      {omnibox::kContextManagementInComposebox,
       omnibox::kContextManagementInOmnibox},
      {contextual_tasks::kContextualTasksContext, omnibox::kAimUsePecApi});

  FakeContextualSearchboxHandler fake_handler(profile_.get(),
                                              web_contents_.get());
  controller()->SetContextualSearchboxHandler(&fake_handler);

  // Set up mock tabs
  std::vector<OmniboxContextMenuController::TabInfo> mock_tabs;
  OmniboxContextMenuController::TabInfo tab;
  tab.tab_id = 1;
  tab.title = u"Tab 1";
  tab.url = GURL("https://example.com");
  mock_tabs.push_back(tab);
  controller()->SetMockTabs(mock_tabs);

  // Rebuild menu
  controller()->RebuildMenu();

  // 1. Verify: Toggle is NOT in main menu
  EXPECT_FALSE(controller()
                   ->menu_model()
                   ->GetIndexOfCommandId(IDC_OMNIBOX_CONTEXT_SMART_TAB_SHARING)
                   .has_value());

  // 2. Verify: Submenu IS in main menu
  EXPECT_TRUE(controller()
                  ->menu_model()
                  ->GetIndexOfCommandId(IDC_OMNIBOX_CONTEXT_SHARED_TABS_SUBMENU)
                  .has_value());

  // 3. Verify: Toggle is NOT in submenu
  ASSERT_TRUE(controller()->shared_tabs_menu_model());
  EXPECT_FALSE(controller()
                   ->shared_tabs_menu_model()
                   ->GetIndexOfCommandId(IDC_OMNIBOX_CONTEXT_SMART_TAB_SHARING)
                   .has_value());
}

TEST_F(OmniboxContextMenuControllerTest,
       TabSharingCommandsDisabledWhenTabContextDisabled) {
  base::test::ScopedFeatureList feature_list;
  feature_list.InitWithFeatures({omnibox::kContextManagementInComposebox,
                                 omnibox::kContextManagementInOmnibox},
                                {omnibox::kAimUsePecApi});

  FakeContextualSearchboxHandler fake_handler(profile_.get(),
                                              web_contents_.get());
  controller()->SetContextualSearchboxHandler(&fake_handler);

  std::vector<OmniboxContextMenuController::TabInfo> mock_tabs;
  OmniboxContextMenuController::TabInfo tab1;
  tab1.tab_id = 1;
  tab1.title = u"Tab 1";
  tab1.url = GURL("https://example.com");
  mock_tabs.push_back(tab1);
  controller()->SetMockTabs(mock_tabs);

  // Disable tab context.
  controller()->SetTabContextEnabled(false);
  controller()->RebuildMenu();

  // Verify submenu is present.
  EXPECT_NE(std::nullopt, controller()->menu_model()->GetIndexOfCommandId(
                              IDC_OMNIBOX_CONTEXT_SHARED_TABS_SUBMENU));

  // Verify submenu is disabled.
  EXPECT_FALSE(controller()->IsCommandIdEnabled(
      IDC_OMNIBOX_CONTEXT_SHARED_TABS_SUBMENU));

  // Verify smart tab sharing toggle is disabled.
  EXPECT_FALSE(
      controller()->IsCommandIdEnabled(IDC_OMNIBOX_CONTEXT_SMART_TAB_SHARING));
}

TEST_F(OmniboxContextMenuControllerTest,
       HandleDriveUploadResponse_CancelledWhenAiModeClosed_RemainsNone) {
  auto* omnibox_controller =
      OmniboxContextMenuController::GetOmniboxController(web_contents_.get());
  ASSERT_TRUE(omnibox_controller);
  ASSERT_EQ(omnibox_controller->popup_state_manager()->popup_state(),
            OmniboxPopupState::kNone);

  auto response = searchbox::mojom::DriveUploadResponse::New();

  OmniboxContextMenuController::HandleDriveUploadResponse(
      /*was_ai_mode_open=*/false, web_contents_->GetWeakPtr(),
      std::move(response));

  // If upload is cancelled and AI mode was not open, state should remain
  // `kNone`.
  EXPECT_EQ(omnibox_controller->popup_state_manager()->popup_state(),
            OmniboxPopupState::kNone);
}

TEST_F(OmniboxContextMenuControllerTest,
       HandleDriveUploadResponse_CancelledWhenAiModeOpen_RemainsAim) {
  auto* omnibox_controller =
      OmniboxContextMenuController::GetOmniboxController(web_contents_.get());
  ASSERT_TRUE(omnibox_controller);
  // Set initial state to `kAim`.
  omnibox_controller->edit_model()->OpenAiMode(
      OmniboxEditModel::AimActivation::kContextMenu);
  ASSERT_EQ(omnibox_controller->popup_state_manager()->popup_state(),
            OmniboxPopupState::kAim);

  // Set an empty/cancelled response.
  auto response = searchbox::mojom::DriveUploadResponse::New();

  OmniboxContextMenuController::HandleDriveUploadResponse(
      /*was_ai_mode_open=*/true, web_contents_->GetWeakPtr(),
      std::move(response));

  // If upload is cancelled and AI mode was not open, state should remain
  // `kAim`.
  EXPECT_EQ(omnibox_controller->popup_state_manager()->popup_state(),
            OmniboxPopupState::kAim);
}

TEST_F(OmniboxContextMenuControllerTest,
       HandleDriveUploadResponse_SuccessTransitionsState) {
  auto* omnibox_controller =
      OmniboxContextMenuController::GetOmniboxController(web_contents_.get());
  ASSERT_TRUE(omnibox_controller);
  ASSERT_EQ(omnibox_controller->popup_state_manager()->popup_state(),
            OmniboxPopupState::kNone);

  auto response = searchbox::mojom::DriveUploadResponse::New();
  auto file = searchbox::mojom::DriveFile::New();
  file->token = base::UnguessableToken::Create();
  file->file_name = "test.txt";
  file->mime_type = "text/plain";
  response->files.push_back(std::move(file));

  OmniboxContextMenuController::HandleDriveUploadResponse(
      /*was_ai_mode_open=*/false, web_contents_->GetWeakPtr(),
      std::move(response));

  // If upload is successful, state should transition to `kAim`.
  EXPECT_EQ(omnibox_controller->popup_state_manager()->popup_state(),
            OmniboxPopupState::kAim);
}
