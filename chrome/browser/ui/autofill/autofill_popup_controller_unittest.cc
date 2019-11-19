// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <stddef.h>

#include <memory>

#include "base/macros.h"
#include "base/memory/weak_ptr.h"
#include "base/optional.h"
#include "base/strings/utf_string_conversions.h"
#include "build/build_config.h"
#include "chrome/browser/accessibility/accessibility_state_utils.h"
#include "chrome/browser/ui/autofill/autofill_popup_controller_impl.h"
#include "chrome/browser/ui/autofill/autofill_popup_view.h"
#include "chrome/browser/ui/autofill/popup_view_common.h"
#include "chrome/browser/ui/autofill/popup_view_test_helpers.h"
#include "chrome/test/base/chrome_render_view_host_test_harness.h"
#include "chrome/test/base/testing_profile.h"
#include "components/autofill/content/browser/content_autofill_driver.h"
#include "components/autofill/content/browser/content_autofill_driver_factory.h"
#include "components/autofill/core/browser/autofill_external_delegate.h"
#include "components/autofill/core/browser/autofill_manager.h"
#include "components/autofill/core/browser/autofill_test_utils.h"
#include "components/autofill/core/browser/test_autofill_client.h"
#include "components/autofill/core/browser/ui/popup_item_ids.h"
#include "components/prefs/pref_service.h"
#include "content/public/browser/web_contents.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "ui/accessibility/ax_active_popup.h"
#include "ui/accessibility/ax_node.h"
#include "ui/accessibility/ax_tree_id.h"
#include "ui/accessibility/ax_tree_manager.h"
#include "ui/accessibility/ax_tree_manager_map.h"
#include "ui/accessibility/platform/ax_platform_node_base.h"
#include "ui/accessibility/platform/ax_platform_node_delegate_base.h"
#include "ui/gfx/geometry/rect.h"
#include "ui/gfx/text_utils.h"

#if !defined(OS_CHROMEOS)
#include "content/public/browser/browser_accessibility_state.h"
#endif

using ::testing::_;
using ::testing::AtLeast;
using ::testing::Mock;
using ::testing::NiceMock;
using ::testing::StrictMock;
using base::ASCIIToUTF16;
using base::WeakPtr;

namespace autofill {
namespace {

const char kAppLocale[] = "en-US";
const AutofillManager::AutofillDownloadManagerState kDownloadState =
    AutofillManager::DISABLE_AUTOFILL_DOWNLOAD_MANAGER;

class MockAutofillClient : public autofill::TestAutofillClient {
 public:
  MockAutofillClient() : prefs_(autofill::test::PrefServiceForTesting()) {}
  ~MockAutofillClient() override = default;

  PrefService* GetPrefs() override { return prefs_.get(); }

 private:
  std::unique_ptr<PrefService> prefs_;

  DISALLOW_COPY_AND_ASSIGN(MockAutofillClient);
};

class MockAutofillDriver : public ContentAutofillDriver {
 public:
  MockAutofillDriver(content::RenderFrameHost* rfh, MockAutofillClient* client)
      : ContentAutofillDriver(rfh,
                              client,
                              kAppLocale,
                              kDownloadState,
                              nullptr) {}

  ~MockAutofillDriver() override = default;
  MOCK_CONST_METHOD0(GetAxTreeId, ui::AXTreeID());

 private:
  DISALLOW_COPY_AND_ASSIGN(MockAutofillDriver);
};

class MockAutofillManager : public AutofillManager {
 public:
  MockAutofillManager(AutofillDriver* driver, MockAutofillClient* client)
      : AutofillManager(driver,
                        client,
                        client->GetPersonalDataManager(),
                        client->GetAutocompleteHistoryManager()) {}
  ~MockAutofillManager() override = default;

 private:
  DISALLOW_COPY_AND_ASSIGN(MockAutofillManager);
};

class MockAutofillExternalDelegate : public AutofillExternalDelegate {
 public:
  MockAutofillExternalDelegate(AutofillManager* autofill_manager,
                               AutofillDriver* autofill_driver)
      : AutofillExternalDelegate(autofill_manager, autofill_driver) {}
  ~MockAutofillExternalDelegate() override = default;

  void DidSelectSuggestion(const base::string16& value,
                           int identifier) override {}
  bool RemoveSuggestion(const base::string16& value, int identifier) override {
    return true;
  }
  base::WeakPtr<AutofillExternalDelegate> GetWeakPtr() {
    return AutofillExternalDelegate::GetWeakPtr();
  }

  MOCK_METHOD0(ClearPreviewedForm, void());
  MOCK_METHOD0(OnPopupSuppressed, void());
};

class MockAutofillPopupView : public AutofillPopupView {
 public:
  MockAutofillPopupView() = default;
  ~MockAutofillPopupView() override = default;

  MOCK_METHOD0(Show, void());
  MOCK_METHOD0(Hide, void());
  MOCK_METHOD2(OnSelectedRowChanged,
               void(base::Optional<int> previous_row_selection,
                    base::Optional<int> current_row_selection));
  MOCK_METHOD0(OnSuggestionsChanged, void());
  MOCK_METHOD0(GetAxUniqueId, base::Optional<int32_t>());

 private:
  DISALLOW_COPY_AND_ASSIGN(MockAutofillPopupView);
};

class TestAutofillPopupController : public AutofillPopupControllerImpl {
 public:
  TestAutofillPopupController(
      base::WeakPtr<AutofillExternalDelegate> external_delegate,
      const gfx::RectF& element_bounds)
      : AutofillPopupControllerImpl(external_delegate,
                                    NULL,
                                    NULL,
                                    element_bounds,
                                    base::i18n::UNKNOWN_DIRECTION) {
    LayoutModelForTesting().SetUpForTesting(
        std::make_unique<MockPopupViewCommonForUnitTesting>());
  }
  ~TestAutofillPopupController() override = default;

  // Making protected functions public for testing
  using AutofillPopupControllerImpl::element_bounds;
  using AutofillPopupControllerImpl::FireControlsChangedEvent;
  using AutofillPopupControllerImpl::GetElidedLabelAt;
  using AutofillPopupControllerImpl::GetElidedValueAt;
  using AutofillPopupControllerImpl::GetLineCount;
  using AutofillPopupControllerImpl::GetRootAXPlatformNodeForWebContents;
  using AutofillPopupControllerImpl::GetSuggestionAt;
  using AutofillPopupControllerImpl::GetWeakPtr;
  using AutofillPopupControllerImpl::popup_bounds;
  using AutofillPopupControllerImpl::RemoveSelectedLine;
  using AutofillPopupControllerImpl::selected_line;
  using AutofillPopupControllerImpl::SelectNextLine;
  using AutofillPopupControllerImpl::SelectPreviousLine;
  using AutofillPopupControllerImpl::SetSelectedLine;
  using AutofillPopupControllerImpl::SetValues;
  MOCK_METHOD0(OnSuggestionsChanged, void());
  MOCK_METHOD0(Hide, void());
  MOCK_METHOD0(GetRootAXPlatformNodeForWebContents, ui::AXPlatformNode*());

  void DoHide() {
    AutofillPopupControllerImpl::Hide();
  }
};

class MockAxTreeManager : public ui::AXTreeManager {
 public:
  MockAxTreeManager() = default;
  ~MockAxTreeManager() = default;

  MOCK_CONST_METHOD2(GetNodeFromTree,
                     ui::AXNode*(const ui::AXTreeID tree_id,
                                 const int32_t node_id));
  MOCK_CONST_METHOD2(GetDelegate,
                     ui::AXPlatformNodeDelegate*(const ui::AXTreeID tree_id,
                                                 const int32_t node_id));
  MOCK_CONST_METHOD1(GetRootDelegate,
                     ui::AXPlatformNodeDelegate*(const ui::AXTreeID tree_id));
  MOCK_CONST_METHOD0(GetTreeID, ui::AXTreeID());
  MOCK_CONST_METHOD0(GetParentTreeID, ui::AXTreeID());
  MOCK_CONST_METHOD0(GetRootAsAXNode, ui::AXNode*());
  MOCK_CONST_METHOD0(GetParentNodeFromParentTreeAsAXNode, ui::AXNode*());

 private:
  DISALLOW_COPY_AND_ASSIGN(MockAxTreeManager);
};

class MockAxPlatformNodeDelegate : public ui::AXPlatformNodeDelegateBase {
 public:
  MockAxPlatformNodeDelegate() = default;
  ~MockAxPlatformNodeDelegate() override = default;

  MOCK_METHOD1(GetFromNodeID, ui::AXPlatformNode*(int32_t id));
  MOCK_METHOD2(GetFromTreeIDAndNodeID,
               ui::AXPlatformNode*(const ui::AXTreeID& tree_id, int32_t id));

 private:
  DISALLOW_COPY_AND_ASSIGN(MockAxPlatformNodeDelegate);
};

class MockAxPlatformNode : public ui::AXPlatformNodeBase {
 public:
  MockAxPlatformNode() = default;
  ~MockAxPlatformNode() override = default;

  MOCK_CONST_METHOD0(GetDelegate, ui::AXPlatformNodeDelegate*());

 private:
  DISALLOW_COPY_AND_ASSIGN(MockAxPlatformNode);
};

static constexpr base::Optional<int> kNoSelection;

}  // namespace

class AutofillPopupControllerUnitTest : public ChromeRenderViewHostTestHarness {
 public:
  AutofillPopupControllerUnitTest()
      : autofill_client_(new MockAutofillClient()),
        autofill_popup_controller_(NULL) {}
  ~AutofillPopupControllerUnitTest() override = default;

  void SetUp() override {
    ChromeRenderViewHostTestHarness::SetUp();
    external_delegate_ = CreateExternalDelegate();
    autofill_popup_view_ = std::make_unique<NiceMock<MockAutofillPopupView>>();
    autofill_popup_controller_ = new NiceMock<TestAutofillPopupController>(
        external_delegate_->GetWeakPtr(), gfx::RectF());
    autofill_popup_controller_->SetViewForTesting(autofill_popup_view());
  }

  void TearDown() override {
    // This will make sure the controller and the view (if any) are both
    // cleaned up.
    if (autofill_popup_controller_)
      autofill_popup_controller_->DoHide();

    external_delegate_.reset();
    ChromeRenderViewHostTestHarness::TearDown();
  }

  virtual std::unique_ptr<NiceMock<MockAutofillExternalDelegate>>
  CreateExternalDelegate() {
    ContentAutofillDriverFactory::CreateForWebContentsAndDelegate(
        web_contents(), autofill_client_.get(), "en-US",
        AutofillManager::ENABLE_AUTOFILL_DOWNLOAD_MANAGER);
    // Make sure RenderFrame is created.
    NavigateAndCommit(GURL("about:blank"));
    ContentAutofillDriverFactory* factory =
        ContentAutofillDriverFactory::FromWebContents(web_contents());
    ContentAutofillDriver* driver =
        factory->DriverForFrame(web_contents()->GetMainFrame());
    return std::make_unique<NiceMock<MockAutofillExternalDelegate>>(
        driver->autofill_manager(), driver);
  }

  TestAutofillPopupController* popup_controller() {
    return autofill_popup_controller_;
  }

  MockAutofillExternalDelegate* delegate() {
    return external_delegate_.get();
  }

  MockAutofillPopupView* autofill_popup_view() {
    return autofill_popup_view_.get();
  }

 protected:
  std::unique_ptr<MockAutofillClient> autofill_client_;
  std::unique_ptr<NiceMock<MockAutofillExternalDelegate>> external_delegate_;
  std::unique_ptr<NiceMock<MockAutofillPopupView>> autofill_popup_view_;
  NiceMock<TestAutofillPopupController>* autofill_popup_controller_;
};

#if !defined(OS_CHROMEOS)
class AutofillPopupControllerAccessibilityUnitTest
    : public AutofillPopupControllerUnitTest {
 public:
  AutofillPopupControllerAccessibilityUnitTest() = default;
  ~AutofillPopupControllerAccessibilityUnitTest() override = default;

  void SetUp() override {
    AutofillPopupControllerUnitTest::SetUp();
    content::BrowserAccessibilityState::GetInstance()
        ->AddAccessibilityModeFlags(ui::AXMode::kScreenReader);
  }

  void TearDown() override {
    content::BrowserAccessibilityState::GetInstance()
        ->RemoveAccessibilityModeFlags(ui::AXMode::kScreenReader);
    AutofillPopupControllerUnitTest::TearDown();
  }

  std::unique_ptr<NiceMock<MockAutofillExternalDelegate>>
  CreateExternalDelegate() override {
    autofill_driver_ = std::make_unique<NiceMock<MockAutofillDriver>>(
        web_contents()->GetMainFrame(), autofill_client_.get());
    autofill_manager_ = std::make_unique<MockAutofillManager>(
        autofill_driver_.get(), autofill_client_.get());
    return std::make_unique<NiceMock<MockAutofillExternalDelegate>>(
        autofill_manager_.get(), autofill_driver_.get());
  }

 protected:
  std::unique_ptr<MockAutofillManager> autofill_manager_;
  std::unique_ptr<NiceMock<MockAutofillDriver>> autofill_driver_;

 private:
  DISALLOW_COPY_AND_ASSIGN(AutofillPopupControllerAccessibilityUnitTest);
};
#endif

TEST_F(AutofillPopupControllerUnitTest, ChangeSelectedLine) {
  // Set up the popup.
  std::vector<Suggestion> suggestions;
  suggestions.push_back(Suggestion("", "", "", 0));
  suggestions.push_back(Suggestion("", "", "", 0));
  autofill_popup_controller_->Show(suggestions,
                                   /*autoselect_first_suggestion=*/false,
                                   PopupType::kUnspecified);

  EXPECT_FALSE(autofill_popup_controller_->selected_line());
  // Check that there are at least 2 values so that the first and last selection
  // are different.
  EXPECT_GE(2,
      static_cast<int>(autofill_popup_controller_->GetLineCount()));

  // Test wrapping before the front.
  autofill_popup_controller_->SelectPreviousLine();
  EXPECT_EQ(autofill_popup_controller_->GetLineCount() - 1,
            autofill_popup_controller_->selected_line().value());

  // Test wrapping after the end.
  autofill_popup_controller_->SelectNextLine();
  EXPECT_EQ(0, *autofill_popup_controller_->selected_line());
}

TEST_F(AutofillPopupControllerUnitTest, RedrawSelectedLine) {
  // Set up the popup.
  std::vector<Suggestion> suggestions;
  suggestions.push_back(Suggestion("", "", "", 0));
  suggestions.push_back(Suggestion("", "", "", 0));
  autofill_popup_controller_->Show(suggestions,
                                   /*autoselect_first_suggestion=*/false,
                                   PopupType::kUnspecified);

  // Make sure that when a new line is selected, it is invalidated so it can
  // be updated to show it is selected.
  base::Optional<int> selected_line = 0;
  EXPECT_CALL(*autofill_popup_view_,
              OnSelectedRowChanged(kNoSelection, selected_line));

  autofill_popup_controller_->SetSelectedLine(selected_line);

  // Ensure that the row isn't invalidated if it didn't change.
  EXPECT_CALL(*autofill_popup_view_, OnSelectedRowChanged(_, _)).Times(0);
  autofill_popup_controller_->SetSelectedLine(selected_line);

  // Change back to no selection.
  EXPECT_CALL(*autofill_popup_view_,
              OnSelectedRowChanged(selected_line, kNoSelection));

  autofill_popup_controller_->SetSelectedLine(kNoSelection);
}

TEST_F(AutofillPopupControllerUnitTest, RemoveLine) {
  // Set up the popup.
  std::vector<Suggestion> suggestions;
  suggestions.push_back(Suggestion("", "", "", 1));
  suggestions.push_back(Suggestion("", "", "", 1));
  suggestions.push_back(Suggestion("", "", "", POPUP_ITEM_ID_AUTOFILL_OPTIONS));
  autofill_popup_controller_->Show(suggestions,
                                   /*autoselect_first_suggestion=*/false,
                                   PopupType::kUnspecified);

  // Generate a popup, so it can be hidden later. It doesn't matter what the
  // external_delegate thinks is being shown in the process, since we are just
  // testing the popup here.
  test::GenerateTestAutofillPopup(external_delegate_.get());

  // No line is selected so the removal should fail.
  EXPECT_FALSE(autofill_popup_controller_->RemoveSelectedLine());

  // Select the first entry.
  base::Optional<int> selected_line(0);
  EXPECT_CALL(*autofill_popup_view_,
              OnSelectedRowChanged(kNoSelection, selected_line));
  autofill_popup_controller_->SetSelectedLine(selected_line);
  Mock::VerifyAndClearExpectations(autofill_popup_view());

  // Remove the first entry. The popup should be redrawn since its size has
  // changed.
  EXPECT_CALL(*autofill_popup_view_, OnSelectedRowChanged(_, _)).Times(0);
  EXPECT_CALL(*autofill_popup_controller_, OnSuggestionsChanged());
  EXPECT_TRUE(autofill_popup_controller_->RemoveSelectedLine());
  Mock::VerifyAndClearExpectations(autofill_popup_view());

  // Select the last entry.
  EXPECT_CALL(*autofill_popup_view_,
              OnSelectedRowChanged(kNoSelection, selected_line));
  autofill_popup_controller_->SetSelectedLine(selected_line);
  Mock::VerifyAndClearExpectations(autofill_popup_view());

  // Remove the last entry. The popup should then be hidden since there are
  // no Autofill entries left.
  EXPECT_CALL(*autofill_popup_view_, OnSelectedRowChanged(_, _)).Times(0);
  EXPECT_CALL(*autofill_popup_controller_, Hide());
  EXPECT_TRUE(autofill_popup_controller_->RemoveSelectedLine());
}

TEST_F(AutofillPopupControllerUnitTest, RemoveOnlyLine) {
  // Set up the popup.
  std::vector<Suggestion> suggestions;
  suggestions.push_back(Suggestion("", "", "", 1));
  autofill_popup_controller_->Show(suggestions,
                                   /*autoselect_first_suggestion=*/false,
                                   PopupType::kUnspecified);

  // Generate a popup.
  test::GenerateTestAutofillPopup(external_delegate_.get());

  // No selection immediately after drawing popup.
  EXPECT_FALSE(autofill_popup_controller_->selected_line());

  // Select the only line.
  base::Optional<int> selected_line(0);
  EXPECT_CALL(*autofill_popup_view_,
              OnSelectedRowChanged(kNoSelection, selected_line));
  autofill_popup_controller_->SetSelectedLine(selected_line);
  Mock::VerifyAndClearExpectations(autofill_popup_view());

  // Remove the only line. The popup should then be hidden since there are no
  // Autofill entries left.
  EXPECT_CALL(*autofill_popup_controller_, Hide());
  EXPECT_CALL(*autofill_popup_view_, OnSelectedRowChanged(_, _)).Times(0);
  EXPECT_TRUE(autofill_popup_controller_->RemoveSelectedLine());
}

TEST_F(AutofillPopupControllerUnitTest, SkipSeparator) {
  // Set up the popup.
  std::vector<Suggestion> suggestions;
  suggestions.push_back(Suggestion("", "", "", 1));
  suggestions.push_back(Suggestion("", "", "", POPUP_ITEM_ID_SEPARATOR));
  suggestions.push_back(Suggestion("", "", "", POPUP_ITEM_ID_AUTOFILL_OPTIONS));
  autofill_popup_controller_->Show(suggestions,
                                   /*autoselect_first_suggestion=*/false,
                                   PopupType::kUnspecified);

  autofill_popup_controller_->SetSelectedLine(0);

  // Make sure next skips the unselectable separator.
  autofill_popup_controller_->SelectNextLine();
  EXPECT_EQ(2, *autofill_popup_controller_->selected_line());

  // Make sure previous skips the unselectable separator.
  autofill_popup_controller_->SelectPreviousLine();
  EXPECT_EQ(0, *autofill_popup_controller_->selected_line());
}

TEST_F(AutofillPopupControllerUnitTest, SkipInsecureFormWarning) {
  std::vector<Suggestion> suggestions;
  suggestions.push_back(Suggestion("", "", "", 1));
  suggestions.push_back(Suggestion("", "", "", POPUP_ITEM_ID_SEPARATOR));
  suggestions.push_back(Suggestion(
      "", "", "", POPUP_ITEM_ID_INSECURE_CONTEXT_PAYMENT_DISABLED_MESSAGE));
  autofill_popup_controller_->Show(suggestions,
                                   /*autoselect_first_suggestion=*/false,
                                   PopupType::kUnspecified);

  // Make sure previous skips the unselectable form warning when there is no
  // selection.
  autofill_popup_controller_->SelectPreviousLine();
  EXPECT_FALSE(autofill_popup_controller_->selected_line());

  autofill_popup_controller_->SetSelectedLine(0);
  EXPECT_EQ(0, *autofill_popup_controller_->selected_line());

  // Make sure previous skips the unselectable form warning when there is a
  // selection.
  autofill_popup_controller_->SelectPreviousLine();
  EXPECT_FALSE(autofill_popup_controller_->selected_line());
}

TEST_F(AutofillPopupControllerUnitTest, UpdateDataListValues) {
  std::vector<Suggestion> suggestions;
  suggestions.push_back(Suggestion("", "", "", 1));
  autofill_popup_controller_->Show(suggestions,
                                   /*autoselect_first_suggestion=*/false,
                                   PopupType::kUnspecified);

  // Add one data list entry.
  base::string16 value1 = ASCIIToUTF16("data list value 1");
  std::vector<base::string16> data_list_values{value1};
  base::string16 label1 = ASCIIToUTF16("data list label 1");
  std::vector<base::string16> data_list_labels{label1};

  autofill_popup_controller_->UpdateDataListValues(data_list_values,
                                                   data_list_labels);

  ASSERT_EQ(3, autofill_popup_controller_->GetLineCount());

  Suggestion result0 = autofill_popup_controller_->GetSuggestionAt(0);
  EXPECT_EQ(value1, result0.value);
  EXPECT_EQ(value1, autofill_popup_controller_->GetElidedValueAt(0));
  EXPECT_EQ(label1, result0.label);
  EXPECT_EQ(base::string16(), result0.additional_label);
  EXPECT_EQ(label1, autofill_popup_controller_->GetElidedLabelAt(0));
  EXPECT_EQ(POPUP_ITEM_ID_DATALIST_ENTRY, result0.frontend_id);

  Suggestion result1 = autofill_popup_controller_->GetSuggestionAt(1);
  EXPECT_EQ(base::string16(), result1.value);
  EXPECT_EQ(base::string16(), result1.label);
  EXPECT_EQ(base::string16(), result1.additional_label);
  EXPECT_EQ(POPUP_ITEM_ID_SEPARATOR, result1.frontend_id);

  Suggestion result2 = autofill_popup_controller_->GetSuggestionAt(2);
  EXPECT_EQ(base::string16(), result2.value);
  EXPECT_EQ(base::string16(), result2.label);
  EXPECT_EQ(base::string16(), result2.additional_label);
  EXPECT_EQ(1, result2.frontend_id);

  // Add two data list entries (which should replace the current one).
  base::string16 value2 = ASCIIToUTF16("data list value 2");
  data_list_values.push_back(value2);
  base::string16 label2 = ASCIIToUTF16("data list label 2");
  data_list_labels.push_back(label2);

  autofill_popup_controller_->UpdateDataListValues(data_list_values,
                                                   data_list_labels);
  ASSERT_EQ(4, autofill_popup_controller_->GetLineCount());

  // Original one first, followed by new one, then separator.
  EXPECT_EQ(value1, autofill_popup_controller_->GetSuggestionAt(0).value);
  EXPECT_EQ(value1, autofill_popup_controller_->GetElidedValueAt(0));
  EXPECT_EQ(label1, autofill_popup_controller_->GetSuggestionAt(0).label);
  EXPECT_EQ(base::string16(),
            autofill_popup_controller_->GetSuggestionAt(0).additional_label);
  EXPECT_EQ(value2, autofill_popup_controller_->GetSuggestionAt(1).value);
  EXPECT_EQ(value2, autofill_popup_controller_->GetElidedValueAt(1));
  EXPECT_EQ(label2, autofill_popup_controller_->GetSuggestionAt(1).label);
  EXPECT_EQ(base::string16(),
            autofill_popup_controller_->GetSuggestionAt(1).additional_label);
  EXPECT_EQ(POPUP_ITEM_ID_SEPARATOR,
            autofill_popup_controller_->GetSuggestionAt(2).frontend_id);

  // Clear all data list values.
  data_list_values.clear();
  autofill_popup_controller_->UpdateDataListValues(data_list_values,
                                                   data_list_labels);

  ASSERT_EQ(1, autofill_popup_controller_->GetLineCount());
  EXPECT_EQ(1, autofill_popup_controller_->GetSuggestionAt(0).frontend_id);
}

TEST_F(AutofillPopupControllerUnitTest, PopupsWithOnlyDataLists) {
  // Create the popup with a single datalist element.
  std::vector<Suggestion> suggestions;
  suggestions.push_back(Suggestion("", "", "", POPUP_ITEM_ID_DATALIST_ENTRY));
  autofill_popup_controller_->Show(suggestions,
                                   /*autoselect_first_suggestion=*/false,
                                   PopupType::kUnspecified);

  // Replace the datalist element with a new one.
  base::string16 value1 = ASCIIToUTF16("data list value 1");
  std::vector<base::string16> data_list_values{value1};
  base::string16 label1 = ASCIIToUTF16("data list label 1");
  std::vector<base::string16> data_list_labels{label1};

  autofill_popup_controller_->UpdateDataListValues(data_list_values,
                                                   data_list_labels);

  ASSERT_EQ(1, autofill_popup_controller_->GetLineCount());
  EXPECT_EQ(value1, autofill_popup_controller_->GetSuggestionAt(0).value);
  EXPECT_EQ(label1, autofill_popup_controller_->GetSuggestionAt(0).label);
  EXPECT_EQ(base::string16(),
            autofill_popup_controller_->GetSuggestionAt(0).additional_label);
  EXPECT_EQ(POPUP_ITEM_ID_DATALIST_ENTRY,
            autofill_popup_controller_->GetSuggestionAt(0).frontend_id);

  // Clear datalist values and check that the popup becomes hidden.
  EXPECT_CALL(*autofill_popup_controller_, Hide());
  data_list_values.clear();
  autofill_popup_controller_->UpdateDataListValues(data_list_values,
                                                   data_list_values);
}

TEST_F(AutofillPopupControllerUnitTest, GetOrCreate) {
  ContentAutofillDriverFactory* factory =
      ContentAutofillDriverFactory::FromWebContents(web_contents());
  ContentAutofillDriver* driver =
      factory->DriverForFrame(web_contents()->GetMainFrame());
  MockAutofillExternalDelegate delegate(driver->autofill_manager(), driver);

  WeakPtr<AutofillPopupControllerImpl> controller =
      AutofillPopupControllerImpl::GetOrCreate(
          WeakPtr<AutofillPopupControllerImpl>(), delegate.GetWeakPtr(), NULL,
          NULL, gfx::RectF(), base::i18n::UNKNOWN_DIRECTION);
  EXPECT_TRUE(controller.get());

  controller->Hide();

  controller = AutofillPopupControllerImpl::GetOrCreate(
      WeakPtr<AutofillPopupControllerImpl>(), delegate.GetWeakPtr(), NULL, NULL,
      gfx::RectF(), base::i18n::UNKNOWN_DIRECTION);
  EXPECT_TRUE(controller.get());

  WeakPtr<AutofillPopupControllerImpl> controller2 =
      AutofillPopupControllerImpl::GetOrCreate(
          controller, delegate.GetWeakPtr(), NULL, NULL, gfx::RectF(),
          base::i18n::UNKNOWN_DIRECTION);
  EXPECT_EQ(controller.get(), controller2.get());
  controller->Hide();

  NiceMock<TestAutofillPopupController>* test_controller =
      new NiceMock<TestAutofillPopupController>(delegate.GetWeakPtr(),
                                                gfx::RectF());
  EXPECT_CALL(*test_controller, Hide());

  gfx::RectF bounds(0.f, 0.f, 1.f, 2.f);
  base::WeakPtr<AutofillPopupControllerImpl> controller3 =
      AutofillPopupControllerImpl::GetOrCreate(
          test_controller->GetWeakPtr(),
          delegate.GetWeakPtr(),
          NULL,
          NULL,
          bounds,
          base::i18n::UNKNOWN_DIRECTION);
  EXPECT_EQ(
      bounds,
      static_cast<AutofillPopupController*>(controller3.get())->
          element_bounds());
  controller3->Hide();

  // Hide the test_controller to delete it.
  test_controller->DoHide();

  test_controller = new NiceMock<TestAutofillPopupController>(
      delegate.GetWeakPtr(), gfx::RectF());
  EXPECT_CALL(*test_controller, Hide()).Times(0);

  const base::WeakPtr<AutofillPopupControllerImpl> controller4 =
      AutofillPopupControllerImpl::GetOrCreate(
          test_controller->GetWeakPtr(), delegate.GetWeakPtr(), nullptr,
          nullptr, bounds, base::i18n::UNKNOWN_DIRECTION);
  EXPECT_EQ(bounds,
            static_cast<const AutofillPopupController*>(controller4.get())
                ->element_bounds());
  delete test_controller;
}

TEST_F(AutofillPopupControllerUnitTest, ProperlyResetController) {
  std::vector<Suggestion> suggestions;
  suggestions.push_back(Suggestion("", "", "", 0));
  suggestions.push_back(Suggestion("", "", "", 0));
  popup_controller()->Show(suggestions, false, PopupType::kUnspecified);
  popup_controller()->SetSelectedLine(0);

  // Now show a new popup with the same controller, but with fewer items.
  WeakPtr<AutofillPopupControllerImpl> controller =
      AutofillPopupControllerImpl::GetOrCreate(
          popup_controller()->GetWeakPtr(), delegate()->GetWeakPtr(), NULL,
          NULL, gfx::RectF(), base::i18n::UNKNOWN_DIRECTION);
  EXPECT_FALSE(controller->selected_line());
  EXPECT_EQ(0, controller->GetLineCount());
}

TEST_F(AutofillPopupControllerUnitTest, HidingClearsPreview) {
  // Create a new controller, because hiding destroys it and we can't destroy it
  // twice.
  ContentAutofillDriverFactory* factory =
      ContentAutofillDriverFactory::FromWebContents(web_contents());
  ContentAutofillDriver* driver =
      factory->DriverForFrame(web_contents()->GetMainFrame());
  StrictMock<MockAutofillExternalDelegate> delegate(driver->autofill_manager(),
                                                    driver);
  StrictMock<TestAutofillPopupController>* test_controller =
      new StrictMock<TestAutofillPopupController>(delegate.GetWeakPtr(),
                                                  gfx::RectF());

  EXPECT_CALL(delegate, ClearPreviewedForm());
  // Hide() also deletes the object itself.
  test_controller->DoHide();
}

#if !defined(OS_ANDROID)
TEST_F(AutofillPopupControllerUnitTest, ElideText) {
  std::vector<Suggestion> suggestions;
  suggestions.push_back(
      Suggestion("Text that will need to be trimmed",
      "Label that will be trimmed", "genericCC", 0));
  suggestions.push_back(
      Suggestion("untrimmed", "Untrimmed", "genericCC", 0));

  autofill_popup_controller_->SetValues(suggestions);

  // Ensure the popup will be too small to display all of the first row.
  int popup_max_width =
      gfx::GetStringWidth(
          suggestions[0].value,
          autofill_popup_controller_->layout_model().GetValueFontListForRow(
              0)) +
      gfx::GetStringWidth(
          suggestions[0].label,
          autofill_popup_controller_->layout_model().GetLabelFontListForRow(
              0)) -
      25;

  autofill_popup_controller_->ElideValueAndLabelForRow(0, popup_max_width);

  // The first element was long so it should have been trimmed.
  EXPECT_NE(autofill_popup_controller_->GetSuggestionAt(0).value,
            autofill_popup_controller_->GetElidedValueAt(0));
  EXPECT_NE(autofill_popup_controller_->GetSuggestionAt(0).label,
            autofill_popup_controller_->GetElidedLabelAt(0));

  autofill_popup_controller_->ElideValueAndLabelForRow(1, popup_max_width);

  // The second element was shorter so it should be unchanged.
  EXPECT_EQ(autofill_popup_controller_->GetSuggestionAt(1).value,
            autofill_popup_controller_->GetElidedValueAt(1));
  EXPECT_EQ(autofill_popup_controller_->GetSuggestionAt(1).label,
            autofill_popup_controller_->GetElidedLabelAt(1));
}
#endif

#if !defined(OS_CHROMEOS)
TEST_F(AutofillPopupControllerAccessibilityUnitTest, FireControlsChangedEvent) {
  StrictMock<MockAxPlatformNodeDelegate> mock_ax_platform_node_delegate;
  StrictMock<MockAxPlatformNode> mock_ax_platform_node;
  const ui::AXTreeID& test_tree_id = ui::AXTreeID::CreateNewAXTreeID();

  // Test for successfully firing controls changed event for popup show/hide.
  {
    EXPECT_CALL(*autofill_driver_, GetAxTreeId())
        .Times(2)
        .WillRepeatedly(testing::Return(test_tree_id));
    EXPECT_CALL(*autofill_popup_view_, GetAxUniqueId)
        .Times(2)
        .WillRepeatedly(testing::Return(base::Optional<int32_t>(123)));
    EXPECT_CALL(*autofill_popup_controller_,
                GetRootAXPlatformNodeForWebContents)
        .WillRepeatedly(testing::Return(&mock_ax_platform_node));
    EXPECT_CALL(mock_ax_platform_node, GetDelegate)
        .WillRepeatedly(testing::Return(&mock_ax_platform_node_delegate));
    EXPECT_CALL(mock_ax_platform_node_delegate, GetFromTreeIDAndNodeID)
        .Times(2)
        .WillRepeatedly(testing::Return(&mock_ax_platform_node));

    // Fire event for popup show and active popup ax unique id is set.
    autofill_popup_controller_->FireControlsChangedEvent(true);
    EXPECT_EQ(123, ui::GetActivePopupAxUniqueId());

    // Fire event for popup hide and active popup ax unique id is cleared.
    autofill_popup_controller_->FireControlsChangedEvent(false);
    EXPECT_EQ(base::nullopt, ui::GetActivePopupAxUniqueId());
  }

  // Test for attempting to fire controls changed event when ax tree manager
  // fails to retrieve the ax platform node associated with the popup.
  // No event is fired and global active popup ax unique id is not set.
  {
    EXPECT_CALL(*autofill_driver_, GetAxTreeId())
        .WillOnce(testing::Return(test_tree_id));
    EXPECT_CALL(*autofill_popup_view_, GetAxUniqueId)
        .WillOnce(testing::Return(base::Optional<int32_t>(123)));
    EXPECT_CALL(*autofill_popup_controller_,
                GetRootAXPlatformNodeForWebContents)
        .WillOnce(testing::Return(&mock_ax_platform_node));
    EXPECT_CALL(mock_ax_platform_node, GetDelegate)
        .WillOnce(testing::Return(&mock_ax_platform_node_delegate));
    EXPECT_CALL(mock_ax_platform_node_delegate, GetFromTreeIDAndNodeID)
        .WillOnce(testing::Return(nullptr));

    // No controls changed event is fired and active popup ax unique id is not
    // set.
    autofill_popup_controller_->FireControlsChangedEvent(true);
    EXPECT_EQ(base::nullopt, ui::GetActivePopupAxUniqueId());
  }

  // Test for attempting to fire controls changed event when failing to retrieve
  // the ax platform node associated with the popup.
  // No event is fired and global active popup ax unique id is not set.
  {
    EXPECT_CALL(*autofill_popup_controller_,
                GetRootAXPlatformNodeForWebContents)
        .WillRepeatedly(testing::Return(&mock_ax_platform_node));
    EXPECT_CALL(*autofill_driver_, GetAxTreeId())
        .WillOnce(testing::Return(test_tree_id));
    EXPECT_CALL(mock_ax_platform_node, GetDelegate)
        .WillRepeatedly(testing::Return(&mock_ax_platform_node_delegate));
    EXPECT_CALL(mock_ax_platform_node_delegate, GetFromTreeIDAndNodeID)
        .WillOnce(testing::Return(nullptr));
    EXPECT_CALL(*autofill_popup_view_, GetAxUniqueId)
        .WillOnce(testing::Return(base::Optional<int32_t>(123)));

    // No controls changed event is fired and active popup ax unique id is not
    // set.
    autofill_popup_controller_->FireControlsChangedEvent(true);
    EXPECT_EQ(base::nullopt, ui::GetActivePopupAxUniqueId());
  }

  // Test for attempting to fire controls changed event when failing to retrieve
  // the autofill popup's ax unique id.
  // No event is fired and global active popup ax unique id is not set.
  {
    EXPECT_CALL(*autofill_driver_, GetAxTreeId())
        .WillOnce(testing::Return(test_tree_id));
    EXPECT_CALL(*autofill_popup_controller_,
                GetRootAXPlatformNodeForWebContents)
        .WillRepeatedly(testing::Return(&mock_ax_platform_node));
    EXPECT_CALL(mock_ax_platform_node, GetDelegate)
        .WillRepeatedly(testing::Return(&mock_ax_platform_node_delegate));
    EXPECT_CALL(mock_ax_platform_node_delegate, GetFromTreeIDAndNodeID)
        .WillOnce(testing::Return(&mock_ax_platform_node));
    EXPECT_CALL(*autofill_popup_view_, GetAxUniqueId)
        .WillOnce(testing::Return(base::nullopt));

    // No controls changed event is fired and active popup ax unique id is not
    // set.
    autofill_popup_controller_->FireControlsChangedEvent(true);
    EXPECT_EQ(base::nullopt, ui::GetActivePopupAxUniqueId());
  }
}
#endif

}  // namespace autofill
