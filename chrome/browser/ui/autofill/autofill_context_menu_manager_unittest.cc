// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/autofill/autofill_context_menu_manager.h"
#include <memory>

#include "base/test/scoped_feature_list.h"
#include "chrome/browser/autofill/personal_data_manager_factory.h"
#include "chrome/browser/renderer_context_menu/render_view_context_menu_test_util.h"
#include "chrome/grit/generated_resources.h"
#include "chrome/test/base/chrome_render_view_host_test_harness.h"
#include "components/autofill/content/browser/content_autofill_driver_factory.h"
#include "components/autofill/content/browser/content_autofill_driver_factory_test_api.h"
#include "components/autofill/core/browser/autofill_test_utils.h"
#include "components/autofill/core/browser/test_autofill_client.h"
#include "components/autofill/core/browser/test_autofill_driver.h"
#include "components/autofill/core/browser/test_personal_data_manager.h"
#include "components/autofill/core/common/autofill_features.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/blink/public/mojom/context_menu/context_menu.mojom.h"
#include "ui/base/l10n/l10n_util.h"

using testing::_;

namespace autofill {

namespace {
// Generates a ContextMenuParams for the Autofill context menu options.
content::ContextMenuParams CreateContextMenuParams(
    absl::optional<autofill::FormRendererId> form_renderer_id = absl::nullopt,
    autofill::FieldRendererId field_render_id = autofill::FieldRendererId(0)) {
  content::ContextMenuParams rv;
  rv.is_editable = true;
  rv.page_url = GURL("http://test.page/");
  rv.input_field_type = blink::mojom::ContextMenuDataInputFieldType::kPlainText;
  if (form_renderer_id)
    rv.form_renderer_id = form_renderer_id->value();
  rv.field_renderer_id = field_render_id.value();
  return rv;
}

class MockAutofillDriver : public TestAutofillDriver {
 public:
  MockAutofillDriver() = default;
  MockAutofillDriver(const MockAutofillDriver&) = delete;
  MockAutofillDriver& operator=(const MockAutofillDriver&) = delete;

  // Mock methods to enable testability.
  MOCK_METHOD(void,
              RendererShouldFillFieldWithValue,
              (const FieldGlobalId& field_id, const std::u16string& value),
              (override));
  MOCK_METHOD(void,
              OnContextMenuShownInField,
              (const FormGlobalId& form_global_id,
               const FieldGlobalId& field_global_id),
              (override));
};

}  // namespace

class AutofillContextMenuManagerTest : public ChromeRenderViewHostTestHarness {
 public:
  AutofillContextMenuManagerTest() {
    feature_.InitWithFeatures(
        {features::kAutofillShowManualFallbackInContextMenu,
         features::kAutofillFeedback},
        {});
  }

  AutofillContextMenuManagerTest(const AutofillContextMenuManagerTest&) =
      delete;
  AutofillContextMenuManagerTest& operator=(
      const AutofillContextMenuManagerTest&) = delete;

  void SetUp() override {
    ChromeRenderViewHostTestHarness::SetUp();

    PersonalDataManagerFactory::GetInstance()->SetTestingFactory(
        profile(), BrowserContextKeyedServiceFactory::TestingFactory());

    auto pdm = std::make_unique<TestPersonalDataManager>();
    pdm->SetPrefService(profile()->GetPrefs());
    pdm->AddProfile(test::GetFullProfile());
    pdm->AddCreditCard(test::GetCreditCard());

    autofill_client_ = std::make_unique<TestAutofillClient>(std::move(pdm));
    menu_model_ = std::make_unique<ui::SimpleMenuModel>(nullptr);
    render_view_context_menu_ = std::make_unique<TestRenderViewContextMenu>(
        *main_rfh(), content::ContextMenuParams());
    render_view_context_menu_->Init();
    driver_ = InjectAutofillDriver(main_rfh(),
                                   std::make_unique<MockAutofillDriver>());

    autofill_context_menu_manager_ =
        std::make_unique<AutofillContextMenuManager>(
            autofill_client_->GetPersonalDataManager(),
            render_view_context_menu_.get(), menu_model_.get(), nullptr);
    autofill_context_menu_manager()->set_params_for_testing(
        CreateContextMenuParams());
  }

  void TearDown() override {
    autofill_context_menu_manager_.reset();
    render_view_context_menu_.reset();
    autofill_client_.reset();
    ChromeRenderViewHostTestHarness::TearDown();
  }

 protected:
  MockAutofillDriver* InjectAutofillDriver(
      content::RenderFrameHost* rfh,
      std::unique_ptr<MockAutofillDriver> driver) {
    auto* raw_driver = driver.get();
    ContentAutofillDriverFactory::CreateForWebContentsAndDelegate(
        web_contents(), autofill_client_.get(),
        ContentAutofillDriverFactory::DriverInitCallback());
    auto* cadf = ContentAutofillDriverFactory::FromWebContents(web_contents());
    ContentAutofillDriverFactoryTestApi(cadf).SetDriver(rfh, std::move(driver));
    return raw_driver;
  }

  ui::SimpleMenuModel* menu_model() const { return menu_model_.get(); }

  AutofillContextMenuManager* autofill_context_menu_manager() const {
    return autofill_context_menu_manager_.get();
  }

  MockAutofillDriver* driver() const { return driver_; }

 private:
  std::unique_ptr<TestAutofillClient> autofill_client_;
  std::unique_ptr<TestRenderViewContextMenu> render_view_context_menu_;
  std::unique_ptr<ui::SimpleMenuModel> menu_model_;
  std::unique_ptr<AutofillContextMenuManager> autofill_context_menu_manager_;
  base::test::ScopedFeatureList feature_;
  raw_ptr<MockAutofillDriver> driver_;
  test::AutofillEnvironment autofill_environment_;
};

// Tests that the Autofill context menu is correctly set up.
TEST_F(AutofillContextMenuManagerTest, AutofillContextMenuContents) {
  autofill_context_menu_manager()->AppendItems();
  std::vector<std::u16string> all_added_strings;

  // Check for top level menu with autofill options.
  ASSERT_EQ(5u, menu_model()->GetItemCount());
  ASSERT_EQ(u"Fill Address Info", menu_model()->GetLabelAt(0));
  ASSERT_EQ(u"Fill Payment", menu_model()->GetLabelAt(1));
  ASSERT_EQ(l10n_util::GetStringUTF16(IDS_CONTENT_CONTEXT_AUTOFILL_FEEDBACK),
            menu_model()->GetLabelAt(3));
  ASSERT_EQ(menu_model()->GetTypeAt(0), ui::MenuModel::ItemType::TYPE_SUBMENU);
  ASSERT_EQ(menu_model()->GetTypeAt(1), ui::MenuModel::ItemType::TYPE_SUBMENU);
  ASSERT_EQ(menu_model()->GetTypeAt(2),
            ui::MenuModel::ItemType::TYPE_SEPARATOR);
  ASSERT_EQ(menu_model()->GetTypeAt(3), ui::MenuModel::ItemType::TYPE_COMMAND);
  ASSERT_EQ(menu_model()->GetTypeAt(4),
            ui::MenuModel::ItemType::TYPE_SEPARATOR);

  // Check for submenu with address descriptions.
  auto* address_menu_model = menu_model()->GetSubmenuModelAt(0);
  ASSERT_EQ(address_menu_model->GetItemCount(), 3u);
  ASSERT_EQ(u"John H. Doe, 666 Erebus St.", address_menu_model->GetLabelAt(0));
  ASSERT_EQ(address_menu_model->GetTypeAt(0),
            ui::MenuModel::ItemType::TYPE_SUBMENU);
  ASSERT_EQ(address_menu_model->GetTypeAt(1),
            ui::MenuModel::ItemType::TYPE_SEPARATOR);
  ASSERT_EQ(u"Manage addresses", address_menu_model->GetLabelAt(2));

  // Check for submenu with address details.
  auto* address_details_submenu = address_menu_model->GetSubmenuModelAt(0);
  ASSERT_EQ(address_details_submenu->GetItemCount(), 10u);
  static constexpr std::array expected_address_values = {
      u"John H. Doe",
      u"",
      u"666 Erebus St.\nApt 8",
      u"Elysium",
      u"91111",
      u"",
      u"16502111111",
      u"johndoe@hades.com",
      u"",
      u"Other"};
  for (size_t i = 0; i < expected_address_values.size(); i++) {
    SCOPED_TRACE(testing::Message() << "Index " << i);
    ASSERT_EQ(address_details_submenu->GetLabelAt(i),
              expected_address_values[i]);
    all_added_strings.push_back(expected_address_values[i]);
  }

  // Check for submenu with address other section.
  auto* address_other_submenu = address_details_submenu->GetSubmenuModelAt(9);
  ASSERT_EQ(address_other_submenu->GetItemCount(), 5u);
  static constexpr std::array expected_address_other_section_values = {
      u"John", u"Doe", u"", u"666 Erebus St.", u"Apt 8"};
  for (size_t i = 0; i < expected_address_other_section_values.size(); i++) {
    SCOPED_TRACE(testing::Message() << "Index " << i);
    ASSERT_EQ(address_other_submenu->GetLabelAt(i),
              expected_address_other_section_values[i]);
    all_added_strings.push_back(expected_address_other_section_values[i]);
  }

  // Check for submenu with credit card descriptions.
  auto* card_menu_model = menu_model()->GetSubmenuModelAt(1);
  ASSERT_EQ(card_menu_model->GetItemCount(), 3u);
  ASSERT_EQ(
      u"Visa  "
      u"\x202A\x2022\x2060\x2006\x2060\x2022\x2060\x2006\x2060\x2022\x2060"
      u"\x2006\x2060\x2022\x2060\x2006\x2060"
      u"1111\x202C",
      card_menu_model->GetLabelAt(0));
  ASSERT_EQ(card_menu_model->GetTypeAt(0),
            ui::MenuModel::ItemType::TYPE_SUBMENU);
  ASSERT_EQ(card_menu_model->GetTypeAt(1),
            ui::MenuModel::ItemType::TYPE_SEPARATOR);
  ASSERT_EQ(u"Manage payment methods", card_menu_model->GetLabelAt(2));

  // Check for submenu with credit card details.
  auto* card_details_submenu = card_menu_model->GetSubmenuModelAt(0);
  ASSERT_EQ(card_details_submenu->GetItemCount(), 5u);
  static constexpr std::array expected_credit_card_values = {
      u"Test User",
      u"‪•⁠ ⁠•⁠ ⁠•⁠ ⁠•⁠ ⁠1111‬", u""};
  for (size_t i = 0; i < expected_credit_card_values.size(); i++) {
    SCOPED_TRACE(testing::Message() << "Index " << i);
    ASSERT_EQ(card_details_submenu->GetLabelAt(i),
              expected_credit_card_values[i]);
    all_added_strings.push_back(expected_credit_card_values[i]);
  }
  all_added_strings.push_back(base::ASCIIToUTF16(test::NextMonth().c_str()));
  ASSERT_EQ(card_details_submenu->GetLabelAt(3), all_added_strings.back());
  all_added_strings.push_back(
      base::ASCIIToUTF16(test::NextYear().c_str()).substr(2));
  ASSERT_EQ(card_details_submenu->GetLabelAt(4), all_added_strings.back());

  // Test all strings added to the command_id_to_menu_item_value_mapper were
  // added to the context menu.
  auto mapper = autofill_context_menu_manager()
                    ->command_id_to_menu_item_value_mapper_for_testing();
  base::ranges::sort(all_added_strings);
  EXPECT_TRUE(base::ranges::all_of(mapper, [&](const auto& p) {
    return base::Contains(all_added_strings, p.second.fill_value);
  }));
}

// For all the command ids that are used to set up the context menu, initiating
// filling for each one of them results in the call to
// `RendererShouldFillFieldWithValue`.
TEST_F(AutofillContextMenuManagerTest, ExecuteCommand) {
  DCHECK(driver());
  autofill_context_menu_manager()->AppendItems();
  auto mapper = autofill_context_menu_manager()
                    ->command_id_to_menu_item_value_mapper_for_testing();
  ASSERT_FALSE(mapper.empty());

  for (auto const& [command_id, map_value] : mapper) {
    // Requires a browser instance which is not available in this test.
    if (map_value.is_manage_item)
      continue;
    SCOPED_TRACE(testing::Message() << "Command " << *command_id);

    FieldRendererId field_renderer_id(test::MakeFieldRendererId());
    FieldGlobalId field_global_id{
        LocalFrameToken(main_rfh()->GetFrameToken().value()),
        field_renderer_id};

    autofill_context_menu_manager()->set_params_for_testing(
        CreateContextMenuParams(absl::nullopt, field_renderer_id));

    EXPECT_CALL(*driver(), RendererShouldFillFieldWithValue(
                               field_global_id, map_value.fill_value));
    autofill_context_menu_manager()->ExecuteCommand(command_id);
  }
}

// Tests that the Autofill's ContentAutofillDriver is called to record metrics
// when the context menu is triggered on a field.
TEST_F(AutofillContextMenuManagerTest, RecordContextMenuIsShownOnField) {
  FormRendererId form_renderer_id(test::MakeFormRendererId());
  FieldRendererId field_renderer_id(test::MakeFieldRendererId());
  autofill_context_menu_manager()->set_params_for_testing(
      CreateContextMenuParams(form_renderer_id, field_renderer_id));

  FormGlobalId form_global_id{
      LocalFrameToken(main_rfh()->GetFrameToken().value()), form_renderer_id};
  FieldGlobalId field_global_id{
      LocalFrameToken(main_rfh()->GetFrameToken().value()), field_renderer_id};

  EXPECT_CALL(*driver(),
              OnContextMenuShownInField(form_global_id, field_global_id));
  autofill_context_menu_manager()->AppendItems();
}

}  // namespace autofill
