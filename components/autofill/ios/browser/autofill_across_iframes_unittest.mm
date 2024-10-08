// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import <string>
#import <vector>

#import "base/containers/contains.h"
#import "base/containers/flat_set.h"
#import "base/strings/strcat.h"
#import "base/strings/utf_string_conversions.h"
#import "base/test/ios/wait_util.h"
#import "base/test/scoped_feature_list.h"
#import "base/time/time.h"
#import "base/types/id_type.h"
#import "components/autofill/core/browser/autofill_test_utils.h"
#import "components/autofill/core/browser/browser_autofill_manager.h"
#import "components/autofill/core/browser/test_autofill_client.h"
#import "components/autofill/core/browser/test_autofill_manager_waiter.h"
#import "components/autofill/core/common/autofill_features.h"
#import "components/autofill/core/common/form_data_test_api.h"
#import "components/autofill/core/common/form_field_data.h"
#import "components/autofill/core/common/mojom/autofill_types.mojom-shared.h"
#import "components/autofill/core/common/unique_ids.h"
#import "components/autofill/ios/browser/autofill_agent.h"
#import "components/autofill/ios/browser/autofill_driver_ios.h"
#import "components/autofill/ios/browser/autofill_driver_ios_factory.h"
#import "components/autofill/ios/browser/autofill_java_script_feature.h"
#import "components/autofill/ios/browser/autofill_util.h"
#import "components/autofill/ios/browser/mock_password_autofill_agent_delegate.h"
#import "components/autofill/ios/browser/new_frame_catcher.h"
#import "components/autofill/ios/browser/test_autofill_manager_injector.h"
#import "components/autofill/ios/form_util/autofill_test_with_web_state.h"
#import "components/autofill/ios/form_util/child_frame_registrar.h"
#import "components/autofill/ios/form_util/form_handlers_java_script_feature.h"
#import "components/autofill/ios/form_util/form_util_java_script_feature.h"
#import "components/prefs/testing_pref_service.h"
#import "ios/testing/embedded_test_server_handlers.h"
#import "ios/web/public/js_messaging/web_frame.h"
#import "ios/web/public/js_messaging/web_frames_manager.h"
#import "ios/web/public/test/fakes/fake_web_client.h"
#import "ios/web/public/test/navigation_test_util.h"
#import "net/test/embedded_test_server/embedded_test_server.h"
#import "net/test/embedded_test_server/request_handler_util.h"
#import "testing/gmock/include/gmock/gmock.h"
#import "testing/gtest/include/gtest/gtest.h"
#import "url/gurl.h"

using autofill::test::NewFrameCatcher;
using base::test::ios::kWaitForJSCompletionTimeout;
using net::test_server::EmbeddedTestServer;
using ::testing::AllOf;
using ::testing::AssertionFailure;
using ::testing::AssertionResult;
using ::testing::AssertionSuccess;
using ::testing::Each;
using ::testing::IsEmpty;
using ::testing::IsTrue;
using ::testing::Property;
using ::testing::SizeIs;
using ::testing::UnorderedElementsAre;
using testing::VariantWith;

namespace autofill {

namespace {
// Returns the FormFieldData pointer for the field in `fields` that has the
// corresponding `placeholder`. Returns nullptr if there is no field to be
// found.
FormFieldData* GetFieldWithPlaceholder(const std::u16string& placeholder,
                                       std::vector<FormFieldData>* fields) {
  auto it =
      base::ranges::find(*fields, placeholder, &FormFieldData::placeholder);
  return it != fields->end() ? &(*it) : nullptr;
}

// Gets a mutable pointer to the first field with `id_attr` among `fields`.
FormFieldData* GetMutableFieldWithId(const std::string& id_attr,
                                     std::vector<FormFieldData>* fields) {
  auto it = base::ranges::find(*fields, base::UTF8ToUTF16(id_attr),
                               &FormFieldData::id_attribute);
  return it != fields->end() ? &*it : nullptr;
}

// Gets a const pointer to the first field with `id_attr` among `fields`.
const FormFieldData* GetFieldWithId(const std::string& id_attr,
                                    const std::vector<FormFieldData>& fields) {
  auto it = base::ranges::find(fields, base::UTF8ToUTF16(id_attr),
                               &FormFieldData::id_attribute);
  return it != fields.end() ? &(*it) : nullptr;
}

// Set the fill data for the `field`.
void SetFillDataForField(
    const std::u16string& value,
    FieldType field_type,
    FormFieldData* field,
    base::flat_map<FieldGlobalId, FieldType>* field_type_map) {
  CHECK(field);
  field->set_value(value);
  field->set_is_autofilled(true);
  field->set_is_user_edited(false);
  (*field_type_map)[field->global_id()] = field_type;
}

// Waits on the input field that corresponds to `field_id` in the `frame` DOM to
// be filled with `expected_value`. Returns AssertionSuccess() on success or
// AssertionFailure() with an error message when it times out.
[[nodiscard]] ::testing::AssertionResult WaitOnFieldFilledWithValue(
    web::WebFrame* frame,
    const std::string& field_id,
    const std::u16string& expected_value) {
  __block bool execute_script = true;
  __block std::u16string value;
  bool res = base::test::ios::WaitUntilConditionOrTimeout(
      kWaitForJSCompletionTimeout, ^() {
        if (execute_script) {
          const std::u16string script =
              base::StrCat({u"document.getElementById('",
                            base::UTF8ToUTF16(field_id), u"').value;"});
          execute_script = false;
          frame->ExecuteJavaScript(
              script, base::BindOnce(^(const base::Value* result) {
                // Script execution is done, re-arm.
                execute_script = true;

                if (!result || !result->is_string()) {
                  return;
                }
                value = base::UTF8ToUTF16(result->GetString());
              }));
        }
        return value == expected_value;
      });

  return res ? AssertionSuccess()
             : AssertionFailure() << "field with id \"" + field_id +
                                         "\"wasn't filled with expected value";
}

// Executes `script` in the specified `frame`, wait until execution is done,
// then pass the execution result to the provided `callback`.
[[nodiscard]] bool ExecuteJavaScriptInFrame(
    web::WebFrame* frame,
    const std::u16string& script,
    base::OnceCallback<void(const base::Value*)> callback =
        base::DoNothingAs<void(const base::Value*)>()) {
  __block bool done = false;

  frame->ExecuteJavaScript(script,
                           base::BindOnce(
                               ^(base::OnceCallback<void(const base::Value*)> c,
                                 const base::Value* result) {
                                 done = true;
                                 std::move(c).Run(result);
                               },
                               std::move(callback)));
  return base::test::ios::WaitUntilConditionOrTimeout(
      kWaitForJSCompletionTimeout, ^() {
        return done;
      });
}

// Contains the template information to construct an input field for testing
// along with helpers.
struct TestFieldInfo {
  std::string id_attribute;
  std::string autocomplete_attribute;
  std::string fill_value;
  bool should_be_filled;
  // Attributes that can only be set when the field is rendered.
  FieldGlobalId global_id;
  LocalFrameToken host_frame;

  // Parses the field info to a HTML <input> field element.
  std::string ToHtmlInput() const {
    CHECK(!id_attribute.empty() && !autocomplete_attribute.empty());
    return base::StrCat({"<input type=\"text\" autocomplete=\"",
                         autocomplete_attribute, "\" id=\"", id_attribute,
                         "\">"});
  }

  // Parses the field info to a HTML <form> element.
  std::string ToHtmlForm() const {
    return "<form>" + ToHtmlInput() + "</form>";
  }
};

struct TestCreditCardForm {
  TestFieldInfo name_field;
  TestFieldInfo cc_number_field;
  TestFieldInfo exp_field;
  TestFieldInfo cvc_field;

  // Returns all fields in the credit card form.
  std::vector<TestFieldInfo> all_fields() const {
    return {name_field, cc_number_field, exp_field, cvc_field};
  }

  // Verifies that the fields corresponding to `filled_field_ids` are filled in
  // the renderer content with their fill value and that they aren't filled with
  // anything if not listed.
  [[nodiscard]] AssertionResult VerifyFieldsAreCorrectlyFilled(
      web::WebFramesManager* frames_manager,
      const base::flat_set<FieldGlobalId>& filled_field_ids) {
    std::vector<TestFieldInfo> fields = {name_field, cc_number_field, exp_field,
                                         cvc_field};

    for (const auto& field : fields) {
      const std::string frame_id = field.host_frame->ToString();
      web::WebFrame* frame = frames_manager->GetFrameWithId(frame_id);
      if (!frame) {
        return AssertionFailure()
               << "frame with id " << frame_id << " couldn't be found";
      }
      const bool should_be_filled =
          base::Contains(filled_field_ids, field.global_id);

      const std::u16string expected_filled_value =
          should_be_filled ? base::UTF8ToUTF16(field.fill_value) : u"";
      if (!should_be_filled) {
        // Wait some time to make sure that the field is indeed not filled.
        base::test::ios::SpinRunLoopWithMinDelay(base::Seconds(2));
      }
      if (AssertionResult result = WaitOnFieldFilledWithValue(
              frame, field.id_attribute, expected_filled_value);
          !result) {
        return result;
      }
    }
    return AssertionSuccess();
  }

  // Set the fill data in `fields` that map with the fields in this test form.
  [[nodiscard]] AssertionResult SetFillData(
      std::vector<FormFieldData>* fields,
      base::flat_map<FieldGlobalId, FieldType>* field_type_map) {
    auto fields_to_fill = {
        std::make_pair(FieldType::CREDIT_CARD_NAME_FULL, &name_field),
        std::make_pair(FieldType::CREDIT_CARD_NUMBER, &cc_number_field),
        std::make_pair(FieldType::CREDIT_CARD_EXP_DATE_4_DIGIT_YEAR,
                       &exp_field),
        std::make_pair(FieldType::CREDIT_CARD_VERIFICATION_CODE, &cvc_field)};
    for (auto [field_type, field_info] : fields_to_fill) {
      FormFieldData* field =
          GetMutableFieldWithId(field_info->id_attribute, fields);
      if (!field) {
        return AssertionFailure()
               << "\"" << field_info->id_attribute << "\" field not found";
      }
      SetFillDataForField(base::UTF8ToUTF16(field_info->fill_value), field_type,
                          field, field_type_map);
      field_info->global_id = field->global_id();
      field_info->host_frame = field->host_frame();
    }
    return AssertionSuccess();
  }
};

// Gets the representation of a credit card form for testing.
TestCreditCardForm GetTestCreditCardForm() {
  return {.name_field = {.id_attribute = "cc-name-field-id",
                         .autocomplete_attribute = "cc-name",
                         .fill_value = "Bob Bobbertson"},
          .cc_number_field = {.id_attribute = "cc-number-field-id",
                              .autocomplete_attribute = "cc-number",
                              .fill_value = "4545454545454545"},
          .exp_field = {.id_attribute = "cc-exp-field-id",
                        .autocomplete_attribute = "cc-exp",
                        .fill_value = "07/2028"},
          .cvc_field = {.id_attribute = "cc-cvc-field-id",
                        .autocomplete_attribute = "cc-csc",
                        .fill_value = "123"}};
}

}  // namespace

// Version of AutofillManager that caches the FormData it receives so we can
// examine them. The public API deals with FormStructure, the post-parsing
// data structure, but we want to intercept the FormData and ensure we're
// providing the right inputs to the parsing process.
class TestAutofillManager : public BrowserAutofillManager {
 public:
  explicit TestAutofillManager(AutofillDriverIOS* driver)
      : BrowserAutofillManager(driver, "en-US") {}

  [[nodiscard]] testing::AssertionResult WaitForFormsSeen(
      int min_num_awaited_calls) {
    return forms_seen_waiter_.Wait(min_num_awaited_calls);
  }

  [[nodiscard]] testing::AssertionResult WaitForFormsFilled(
      int min_num_awaited_calls) {
    return did_fill_forms_waiter_.Wait(min_num_awaited_calls);
  }

  [[nodiscard]] testing::AssertionResult WaitForFormsSubmitted(
      int min_num_awaited_calls) {
    return did_submit_forms_waiter_.Wait(min_num_awaited_calls);
  }

  [[nodiscard]] testing::AssertionResult WaitForFormsAskedForFillData(
      int min_num_awaited_calls) {
    return ask_for_filldata_forms_waiter_.Wait(min_num_awaited_calls);
  }

  [[nodiscard]] testing::AssertionResult WaitOnTextFieldDidChange(
      int min_num_awaited_calls) {
    return text_field_did_change_forms_waiter_.Wait(min_num_awaited_calls);
  }

  void OnFormsSeen(const std::vector<FormData>& updated_forms,
                   const std::vector<FormGlobalId>& removed_forms) override {
    seen_forms_.insert(seen_forms_.end(), updated_forms.begin(),
                       updated_forms.end());
    removed_forms_.insert(removed_forms_.end(), removed_forms.begin(),
                          removed_forms.end());
    BrowserAutofillManager::OnFormsSeen(updated_forms, removed_forms);
  }

  void OnDidFillAutofillFormData(const FormData& form,
                                 base::TimeTicks timestamp) override {
    filled_forms_.push_back(form);
    BrowserAutofillManager::OnDidFillAutofillFormData(form, timestamp);
  }

  void OnFormSubmitted(const FormData& form,
                       const bool known_success,
                       const mojom::SubmissionSource source) override {
    submitted_forms_.emplace_back(form);
    BrowserAutofillManager::OnFormSubmitted(form, known_success, source);
  }

  void OnAskForValuesToFill(
      const FormData& form,
      const FieldGlobalId& field_id,
      const gfx::Rect& caret_bounds,
      AutofillSuggestionTriggerSource trigger_source) override {
    ask_for_filldata_forms_.emplace_back(form);
    BrowserAutofillManager::OnAskForValuesToFill(form, field_id, caret_bounds,
                                                 trigger_source);
  }

  void OnTextFieldDidChange(const FormData& form,
                            const FieldGlobalId& field_id,
                            const base::TimeTicks timestamp) override {
    text_field_did_change_forms_.emplace_back(form);
    BrowserAutofillManager::OnTextFieldDidChange(form, field_id, timestamp);
  }

  const std::vector<FormData>& seen_forms() { return seen_forms_; }
  const std::vector<FormGlobalId>& removed_forms() { return removed_forms_; }
  const std::vector<FormData>& filled_forms() { return filled_forms_; }
  const std::vector<FormData>& submitted_forms() { return submitted_forms_; }
  const std::vector<FormData>& ask_for_filldata_forms() {
    return ask_for_filldata_forms_;
  }
  const std::vector<FormData>& text_filled_did_change_forms() {
    return text_field_did_change_forms_;
  }

  void ResetTestState() {
    seen_forms_.clear();
    removed_forms_.clear();
    filled_forms_.clear();
    submitted_forms_.clear();
    ask_for_filldata_forms_.clear();
    text_field_did_change_forms_.clear();
  }

 private:
  std::vector<FormData> seen_forms_;
  std::vector<FormGlobalId> removed_forms_;
  std::vector<FormData> filled_forms_;
  std::vector<FormData> submitted_forms_;
  std::vector<FormData> ask_for_filldata_forms_;
  std::vector<FormData> text_field_did_change_forms_;

  TestAutofillManagerWaiter forms_seen_waiter_{
      *this,
      {AutofillManagerEvent::kFormsSeen}};

  TestAutofillManagerWaiter did_fill_forms_waiter_{
      *this,
      {AutofillManagerEvent::kDidFillAutofillFormData}};

  TestAutofillManagerWaiter did_submit_forms_waiter_{
      *this,
      {AutofillManagerEvent::kFormSubmitted}};

  TestAutofillManagerWaiter ask_for_filldata_forms_waiter_{
      *this,
      {AutofillManagerEvent::kAskForValuesToFill}};

  TestAutofillManagerWaiter text_field_did_change_forms_waiter_{
      *this,
      {AutofillManagerEvent::kTextFieldDidChange}};
};

// A mock child frame registrar observer.
class MockRegistrarObserver : public autofill::ChildFrameRegistrarObserver {
 public:
  MOCK_METHOD(void,
              OnDidDoubleRegistration,
              (LocalFrameToken local),
              (override));
};

class AutofillAcrossIframesTest : public AutofillTestWithWebState {
 public:
  AutofillAcrossIframesTest()
      : AutofillTestWithWebState(std::make_unique<web::FakeWebClient>()),
        feature_list_(features::kAutofillAcrossIframesIos) {}

  void SetUp() override {
    AutofillTestWithWebState::SetUp();

    web::FakeWebClient* web_client =
        static_cast<web::FakeWebClient*>(GetWebClient());
    web_client->SetJavaScriptFeatures(
        {AutofillJavaScriptFeature::GetInstance(),
         FormUtilJavaScriptFeature::GetInstance(),
         FormHandlersJavaScriptFeature::GetInstance()});

    // We need an AutofillAgent to exist or else the form will never get parsed.
    prefs_ = autofill::test::PrefServiceForTesting();
    autofill_agent_ = [[AutofillAgent alloc] initWithPrefService:prefs_.get()
                                                        webState:web_state()];

    // Driver factory needs to exist before any call to
    // `AutofillDriverIOS::FromWebStateAndWebFrame`, or we crash.
    autofill::AutofillDriverIOSFactory::CreateForWebState(
        web_state(), &autofill_client_, /*bridge=*/autofill_agent_,
        /*locale=*/"en");

    // Password autofill agent needs to exist before any call to fill data.
    autofill::PasswordAutofillAgent::CreateForWebState(web_state(),
                                                       &delegate_mock_);

    autofill_manager_injector_ =
        std::make_unique<TestAutofillManagerInjector<TestAutofillManager>>(
            web_state());
  }

  web::WebFrame* WaitForMainFrame() {
    __block web::WebFrame* main_frame = nullptr;
    EXPECT_TRUE(base::test::ios::WaitUntilConditionOrTimeout(
        kWaitForJSCompletionTimeout, ^bool {
          main_frame = web_frames_manager()->GetMainWebFrame();
          return main_frame != nullptr;
        }));
    return main_frame;
  }

  // Wait for a new frame to become available.
  web::WebFrame* WaitForNewFrame() {
    NewFrameCatcher catcher(web_frames_manager());
    NewFrameCatcher* catcher_ptr = &catcher;
    EXPECT_TRUE(base::test::ios::WaitUntilConditionOrTimeout(
        kWaitForJSCompletionTimeout, ^bool {
          return !!catcher_ptr->latest_new_frame();
        }));
    return catcher_ptr->latest_new_frame();
  }

  // Wait for the browser form to be considered as completed (fully constructed)
  // based on `child_frames_count` and `fields_count`. It is in the hands of
  // the caller to decide when the browser form is deemed complete.
  [[nodiscard]] std::pair<FormData, ::testing::AssertionResult>
  WaitForCompleteBrowserForm(size_t child_frames_count, size_t fields_count) {
    main_frame_manager().ResetTestState();

    __block FormData form;
    bool res = base::test::ios::WaitUntilConditionOrTimeout(
        kWaitForJSCompletionTimeout, ^{
          if (main_frame_manager().seen_forms().empty()) {
            return false;
          }
          form = main_frame_manager().seen_forms().back();
          return form.child_frames().size() == child_frames_count &&
                 form.fields().size() == fields_count;
        });

    // Wait for all pending calls to be done. No calls are awaited at this point
    // but there might be pending FormsSeen() calls that aren't fully completed
    // yet because of async tasks.
    auto wait_res = main_frame_manager().WaitForFormsSeen(0);
    if (!wait_res) {
      return std::make_pair(FormData{}, wait_res);
    }

    main_frame_manager().ResetTestState();
    return res ? std::make_pair(form, AssertionSuccess())
               : std::make_pair(FormData{}, AssertionFailure());
  }

  AutofillDriverIOS* main_frame_driver() {
    return AutofillDriverIOS::FromWebStateAndWebFrame(web_state(),
                                                      WaitForMainFrame());
  }

  TestAutofillManager& main_frame_manager() {
    return static_cast<TestAutofillManager&>(
        main_frame_driver()->GetAutofillManager());
  }

  web::WebFramesManager* web_frames_manager() {
    return GetWebFramesManagerForAutofill(web_state());
  }

  autofill::ChildFrameRegistrar* registrar() {
    return autofill::ChildFrameRegistrar::GetOrCreateForWebState(web_state());
  }

  // Serve document with `contents` accessible at `path` on main origin server.
  void ServeDocument(const std::string& path, const std::string& contents) {
    test_server_.RegisterRequestHandler(base::BindRepeating(
        &net::test_server::HandlePrefixedRequest, "/" + path,
        base::BindRepeating(&testing::HandlePageWithHtml, contents)));
  }

  // Functions for setting up the pages to be loaded. Tests should call one or
  // more of the `Add*` functions, then call `StartTestServerAndLoad`.

  // Adds an iframe loading `path` to the main frame's HTML, and registers a
  // handler on the test server to return `contents` when `path` is requested.
  void AddIframe(const std::string& path, const std::string& contents) {
    main_frame_html_ += "<iframe src=\"/" + path + "\"></iframe>";
    ServeDocument(path, contents);
  }

  // Setup `test_server` to serve `contents` accessible at `path`. You need to
  // start `test_server` before using AddXoriginIframe() to add an iframe
  // sourced at `path`.
  void ServeCrossOriginDocument(const std::string& path,
                                const std::string& contents,
                                EmbeddedTestServer* test_server) {
    test_server->RegisterRequestHandler(base::BindRepeating(
        &net::test_server::HandlePrefixedRequest, "/" + path,
        base::BindRepeating(&testing::HandlePageWithHtml, contents)));
  }

  // Add an iframe sourced at `path` from another origin hosted by `test_server`
  // different from the main frame origin.
  void AddCrossOriginIframe(const std::string& path,
                            EmbeddedTestServer* test_server) {
    const std::string absolute_path = test_server->GetURL("/" + path).spec();
    main_frame_html_ += "<iframe src=\"" + absolute_path + "\"></iframe>";
  }

  // Adds an input parsed from `field` to the main frame's HTML.
  void AddInput(const TestFieldInfo& field) {
    main_frame_html_ += field.ToHtmlInput();
  }

  // Adds an input of type `type` with placeholder `ph` to the main frame's
  // HTML.
  void AddInput(const std::string& type, const std::string& ph) {
    main_frame_html_ +=
        "<input type=\"" + type + "\" placeholder =\"" + ph + "\">";
  }

  // Starts the test server and loads a page containing `main_frame_html_` in
  // the main frame.
  void StartTestServerAndLoad(bool use_synthetic_form = false) {
    if (use_synthetic_form) {
      main_frame_html_ = base::StrCat({"<body>", main_frame_html_, "</body>"});
    } else {
      main_frame_html_ =
          base::StrCat({"<body><form>", main_frame_html_, "</form></body>"});
    }
    test_server_.RegisterRequestHandler(base::BindRepeating(
        &net::test_server::HandlePrefixedRequest, "/testpage",
        base::BindRepeating(&testing::HandlePageWithHtml, main_frame_html_)));
    ASSERT_TRUE(test_server_.Start());
    GURL url = test_server_.GetURL("/testpage");
    web::test::LoadUrl(web_state(), url);
    web_state()->WasShown();
    autofill_client_.set_last_committed_primary_main_frame_url(url);
  }

  // Returns the frame that corresponds to `frame_id`.
  web::WebFrame* GetFrameByID(const std::string& frame_id) {
    return web_frames_manager()->GetFrameWithId(frame_id);
  }

  // Gets the host frame of the first field with `id_attr` among `fields`.
  // Returns nullptr if the frame can't be found.
  web::WebFrame* GetFrameForFieldWithIdAttr(
      const std::string& id_attr,
      const std::vector<FormFieldData>& fields) {
    const FormFieldData* field = GetFieldWithId(id_attr, fields);
    if (!field) {
      return nullptr;
    }
    return web_frames_manager()->GetFrameWithId(
        field->host_frame()->ToString());
  }

  AutofillDriverIOS* GetDriverForFrame(web::WebFrame* frame) {
    auto* driver =
        AutofillDriverIOS::FromWebStateAndWebFrame(web_state(), frame);
    return driver;
  }

  TestAutofillManager* GetManagerForFrame(web::WebFrame* frame) {
    if (auto* driver =
            AutofillDriverIOS::FromWebStateAndWebFrame(web_state(), frame)) {
      return static_cast<TestAutofillManager*>(&driver->GetAutofillManager());
    }
    return nullptr;
  }

  // Fills the form represented by `browser_form` and that corresponds to
  // `cc_form_info` using ApplyFormAction() and verifies that the filled fields
  // correspond to `expected_filled_fields`. It is assumed that
  // the `browser_form` is a xframe form where each field are in their own frame
  // with one distinct form per field. The fill data of the `cc_form_info` will
  // be set when running this routine so subsequent verifications can be done
  // after this.
  void FillAndVerify(TestCreditCardForm& cc_form_info,
                     const FormData& browser_form,
                     const TestFieldInfo& trigger_field,
                     const std::vector<TestFieldInfo>& expected_filled_fields) {
    std::vector<FormFieldData> fields = browser_form.fields();

    base::flat_map<FieldGlobalId, FieldType> field_type_map;
    ASSERT_TRUE(cc_form_info.SetFillData(&fields, &field_type_map));

    // Extract the global ids of the fields that are expected to be filled.
    std::vector<FieldGlobalId> expected_filled_field_ids;
    for (const auto& expected_filled_field : expected_filled_fields) {
      expected_filled_field_ids.push_back(
          CHECK_DEREF(
              GetFieldWithId(expected_filled_field.id_attribute, fields))
              .global_id());
    }

    // Trigger fill.
    web::WebFrame* trigger_frame =
        GetFrameForFieldWithIdAttr(trigger_field.id_attribute, fields);
    ASSERT_TRUE(trigger_frame);
    url::Origin trigger_origin =
        url::Origin::Create(trigger_frame->GetSecurityOrigin());
    base::flat_set<FieldGlobalId> filled_field_ids =
        GetDriverForFrame(trigger_frame)
            ->ApplyFormAction(mojom::FormActionType::kFill,
                              mojom::ActionPersistence::kFill, fields,
                              trigger_origin, field_type_map);

    // Verify that filled fields correspond to the expected ones by comparing
    // their global ids.
    ASSERT_THAT(filled_field_ids, ::testing::UnorderedElementsAreArray(
                                      expected_filled_field_ids));

    // Wait that all the expected fields are filled, one field per frame and
    // form. The fill events are all routed to the frame hosting the browser
    // form, which corresponds to the root form in the forms structure.
    const size_t expected_filled_forms_count = expected_filled_field_ids.size();
    ASSERT_TRUE(
        main_frame_manager().WaitForFormsFilled(expected_filled_forms_count));
    ASSERT_THAT(main_frame_manager().filled_forms(),
                SizeIs(expected_filled_forms_count));

    // Verify that what is actually filled corresponds to what was anticipated.
    EXPECT_TRUE(cc_form_info.VerifyFieldsAreCorrectlyFilled(
        web_frames_manager(), filled_field_ids));

    main_frame_manager().ResetTestState();
  }

  std::unique_ptr<TestAutofillManagerInjector<TestAutofillManager>>
      autofill_manager_injector_;
  std::unique_ptr<PrefService> prefs_;
  autofill::TestAutofillClient autofill_client_;
  AutofillAgent* autofill_agent_;
  autofill::MockPasswordAutofillAgentDelegate delegate_mock_;
  base::test::ScopedFeatureList feature_list_;

  EmbeddedTestServer test_server_;
  std::string main_frame_html_;
};

// If a page has no child frames, the corresponding field in the saved form
// structure should be empty.
TEST_F(AutofillAcrossIframesTest, NoChildFrames) {
  AddInput("text", "name");
  AddInput("text", "address");
  StartTestServerAndLoad();

  ASSERT_TRUE(main_frame_manager().WaitForFormsSeen(1));
  ASSERT_EQ(main_frame_manager().seen_forms().size(), 1u);

  const FormData& form = main_frame_manager().seen_forms()[0];
  EXPECT_EQ(form.child_frames().size(), 0u);

  // The main frame driver should have the correct local frame token set even
  // without any child frames.
  LocalFrameToken token = main_frame_driver()->GetFrameToken();
  ASSERT_TRUE(token);
  web::WebFramesManager* frames_manager = web_frames_manager();
  ASSERT_TRUE(frames_manager);
  web::WebFrame* frame = frames_manager->GetFrameWithId(token.ToString());
  EXPECT_EQ(frame, main_frame_driver()->web_frame());
}

// Ensure that child frames are assigned a token during form extraction, are
// registered under that token with the registrar, and can be found in the
// WebFramesManager using the frame ID provided by the registrar.
TEST_F(AutofillAcrossIframesTest, WithChildFrames) {
  AddIframe("cf1", "child frame 1");
  AddInput("text", "name");
  AddIframe("cf2", "child frame 2");
  AddInput("text", "address");
  StartTestServerAndLoad();

  // Wait for the 3 forms to be reported as seen to the main frame that hosts
  // the browser form (which is the flattened representation of all forms in the
  // tree structure that share a common parent).
  ASSERT_TRUE(main_frame_manager().WaitForFormsSeen(3));
  ASSERT_EQ(main_frame_manager().seen_forms().size(), 3u);

  // Pick the last form that was seen which reflects the latest and most
  // complete state of the browser form, which contains all fields in the forms
  // tree (aka browser form).
  const FormData& form = main_frame_manager().seen_forms().back();
  ASSERT_THAT(form.child_frames(), SizeIs(2u));

  FrameTokenWithPredecessor remote_token1 = form.child_frames()[0];
  FrameTokenWithPredecessor remote_token2 = form.child_frames()[1];

  // Verify that tokens hold the right alternative, and the token objects are
  // valid (the bool cast checks this).
  EXPECT_THAT(remote_token1.token, VariantWith<RemoteFrameToken>(IsTrue()));
  EXPECT_THAT(remote_token2.token, VariantWith<RemoteFrameToken>(IsTrue()));

  // Veify that the predecessor of each token is correctly set. The predecessor
  // being the index of the last input field preceeding the frame. Set to -1 if
  // there is no predecessor.
  EXPECT_EQ(-1, remote_token1.predecessor);
  EXPECT_EQ(0, remote_token2.predecessor);

  auto* registrar =
      autofill::ChildFrameRegistrar::GetOrCreateForWebState(web_state());
  ASSERT_TRUE(registrar);

  // Get the frame tokens from the registrar. Wrap this in a block because the
  // registrar receives these from each frame in a separate JS message.
  __block std::optional<LocalFrameToken> local_token1, local_token2;
  ASSERT_TRUE(base::test::ios::WaitUntilConditionOrTimeout(
      kWaitForJSCompletionTimeout, ^bool {
        local_token1 = registrar->LookupChildFrame(
            absl::get<RemoteFrameToken>(remote_token1.token));
        local_token2 = registrar->LookupChildFrame(
            absl::get<RemoteFrameToken>(remote_token2.token));
        return local_token1.has_value() && local_token2.has_value();
      }));

  web::WebFramesManager* frames_manager = web_frames_manager();
  ASSERT_TRUE(frames_manager);

  web::WebFrame* frame1 =
      frames_manager->GetFrameWithId(local_token1->ToString());
  EXPECT_TRUE(frame1);

  web::WebFrame* frame2 =
      frames_manager->GetFrameWithId(local_token2->ToString());
  EXPECT_TRUE(frame2);

  // TODO(crbug.com/40266126): Check contents of frames to make sure they're the
  // right ones.

  // Also check that data relating to the frame was properly set on the form-
  // and field-level data when extracted.
  ASSERT_TRUE(form.host_frame());
  web::WebFrame* main_frame_from_form_data =
      frames_manager->GetFrameWithId(form.host_frame().ToString());
  ASSERT_TRUE(main_frame_from_form_data);
  EXPECT_TRUE(main_frame_from_form_data->IsMainFrame());

  // Verify that the form information in the fields corresponds to the
  // information that is actually in the form.
  FormSignature form_signature = CalculateFormSignature(form);
  url::Origin form_origin = url::Origin::Create(form.url());
  EXPECT_THAT(
      form.fields(),
      Each(AllOf(
          Property(&FormFieldData::host_frame, form.host_frame()),
          Property(&FormFieldData::host_form_id, form.renderer_id()),
          Property(&FormFieldData::origin, form_origin),
          Property(&FormFieldData::host_form_signature, form_signature))));
}

// Ensure that, for a synthetic form (an aggregate of standalone/unowned fields
// not associated with a form), child frames are assigned a token during form
// extraction. This doesn't test the full token registration flow which is
// covered by other tests.
TEST_F(AutofillAcrossIframesTest, WithChildFrames_SyntheticForm) {
  AddIframe("cf1", "child frame 1");
  AddInput("text", "name");
  AddIframe("cf2", "child frame 2");
  AddInput("text", "address");
  StartTestServerAndLoad(/*use_synthetic_form=*/true);

  // Wait for the 3 forms to be reported as seen to the main frame that hosts
  // the browser form (which is the flattened representation of all forms in the
  // tree structure that share a common parent).
  ASSERT_TRUE(main_frame_manager().WaitForFormsSeen(3));
  ASSERT_EQ(main_frame_manager().seen_forms().size(), 3u);

  // Pick the last form that was seen which reflects the latest and most
  // complete state of the browser form, which contains all fields in the forms
  // tree (aka browser form).
  const FormData& form = main_frame_manager().seen_forms().back();
  ASSERT_EQ(form.child_frames().size(), 2u);

  FrameTokenWithPredecessor remote_token1 = form.child_frames()[0];
  FrameTokenWithPredecessor remote_token2 = form.child_frames()[1];

  // Verify that tokens hold the right alternative, and the token objects are
  // valid (the bool cast checks this).
  EXPECT_THAT(remote_token1.token, VariantWith<RemoteFrameToken>(IsTrue()));
  EXPECT_THAT(remote_token2.token, VariantWith<RemoteFrameToken>(IsTrue()));
}

// Ensure that, for a synthetic form that is only composed of child frames
// without input elements, child frames are assigned a token during form
// extraction. This doesn't test the full token registration flow which is
// covered by other tests.
TEST_F(AutofillAcrossIframesTest,
       WithChildFrames_SyntheticForm_WithoutInputElements) {
  AddIframe("cf1", "child frame 1");
  AddIframe("cf2", "child frame 2");
  StartTestServerAndLoad(/*use_synthetic_form=*/true);

  // Wait for the 3 forms to be reported as seen to the main frame that hosts
  // the browser form (which is the flattened representation of all forms in the
  // tree structure that share a common parent).
  ASSERT_TRUE(main_frame_manager().WaitForFormsSeen(3));
  ASSERT_EQ(main_frame_manager().seen_forms().size(), 3u);

  // Pick the last form that was seen which reflects the latest and most
  // complete state of the browser form, which contains all fields in the forms
  // tree (aka browser form).
  const FormData& form = main_frame_manager().seen_forms().back();
  ASSERT_EQ(form.child_frames().size(), 2u);

  FrameTokenWithPredecessor remote_token1 = form.child_frames()[0];
  FrameTokenWithPredecessor remote_token2 = form.child_frames()[1];

  // Verify that tokens hold the right alternative, and the token objects are
  // valid (the bool cast checks this).
  EXPECT_THAT(remote_token1.token, VariantWith<RemoteFrameToken>(IsTrue()));
  EXPECT_THAT(remote_token2.token, VariantWith<RemoteFrameToken>(IsTrue()));
}

// Largely repeats `WithChildFrames` above, but exercises the Resolve method on
// AutofillDriverIOS.
TEST_F(AutofillAcrossIframesTest, Resolve) {
  AddIframe("cf1", "child frame 1");
  AddInput("text", "name");
  StartTestServerAndLoad();

  // Wait for a form with a child frame, and grab its remote token.
  ASSERT_TRUE(main_frame_manager().WaitForFormsSeen(1));
  ASSERT_EQ(main_frame_manager().seen_forms().size(), 1u);
  const FormData& form = main_frame_manager().seen_forms()[0];
  ASSERT_EQ(form.child_frames().size(), 1u);
  FrameTokenWithPredecessor remote_token = form.child_frames()[0];
  EXPECT_THAT(remote_token.token, VariantWith<RemoteFrameToken>(IsTrue()));

  // Wait for the child frame to register itself.
  auto* registrar =
      autofill::ChildFrameRegistrar::GetOrCreateForWebState(web_state());
  ASSERT_TRUE(registrar);
  ASSERT_TRUE(base::test::ios::WaitUntilConditionOrTimeout(
      kWaitForJSCompletionTimeout, ^bool {
        return registrar
            ->LookupChildFrame(absl::get<RemoteFrameToken>(remote_token.token))
            .has_value();
      }));

  // Verify that resolving the registered remote token returns a valid local
  // token that corresponds to a known frame.
  std::optional<LocalFrameToken> local_token =
      main_frame_driver()->Resolve(remote_token.token);
  ASSERT_TRUE(local_token.has_value());
  web::WebFramesManager* frames_manager = web_frames_manager();
  ASSERT_TRUE(frames_manager);
  EXPECT_TRUE(frames_manager->GetFrameWithId(local_token->ToString()));

  // Verify that resolving a local token is an identity operation.
  EXPECT_EQ(local_token, main_frame_driver()->Resolve(*local_token));

  // Verify that resolving a made-up remote token returns nullopt.
  RemoteFrameToken junk_remote_token =
      RemoteFrameToken(base::UnguessableToken::Create());
  std::optional<LocalFrameToken> shouldnt_exist =
      main_frame_driver()->Resolve(junk_remote_token);
  EXPECT_FALSE(shouldnt_exist.has_value());
}

TEST_F(AutofillAcrossIframesTest, SetAndGetParent) {
  AddIframe("cf1", "child frame 1");
  AddInput("text", "name");
  StartTestServerAndLoad();

  // Wait for a form with a child frame, and grab its remote token.
  ASSERT_TRUE(main_frame_manager().WaitForFormsSeen(1));
  ASSERT_EQ(main_frame_manager().seen_forms().size(), 1u);
  const FormData& form = main_frame_manager().seen_forms()[0];
  ASSERT_EQ(form.child_frames().size(), 1u);
  FrameTokenWithPredecessor remote_token = form.child_frames()[0];
  EXPECT_THAT(remote_token.token, VariantWith<RemoteFrameToken>(IsTrue()));

  // Wait for the child frame to register itself.
  auto* registrar =
      autofill::ChildFrameRegistrar::GetOrCreateForWebState(web_state());
  ASSERT_TRUE(registrar);
  ASSERT_TRUE(base::test::ios::WaitUntilConditionOrTimeout(
      kWaitForJSCompletionTimeout, ^bool {
        return registrar
            ->LookupChildFrame(absl::get<RemoteFrameToken>(remote_token.token))
            .has_value();
      }));

  // The main frame shouldn't have a parent – it's the root.
  EXPECT_FALSE(main_frame_driver()->GetParent());

  // The child frame should have the main frame as its parent.
  std::optional<LocalFrameToken> local_token =
      main_frame_driver()->Resolve(remote_token.token);
  ASSERT_TRUE(local_token);
  auto* child_frame_driver = AutofillDriverIOS::FromWebStateAndLocalFrameToken(
      web_state(), *local_token);
  ASSERT_TRUE(child_frame_driver);
  EXPECT_EQ(main_frame_driver(), child_frame_driver->GetParent());
}

TEST_F(AutofillAcrossIframesTest, TriggerExtractionInFrame) {
  AddInput("text", "name");
  AddIframe("cf1", "<form><input id='address'></form>");
  StartTestServerAndLoad();

  web::WebFramesManager* frames_manager = web_frames_manager();
  ASSERT_TRUE(frames_manager);

  // Wait for the main frame and the child frame to be known to the
  // WebFramesManager.
  ASSERT_TRUE(base::test::ios::WaitUntilConditionOrTimeout(
      kWaitForJSCompletionTimeout, ^bool {
        return frames_manager->GetAllWebFrames().size() == 2;
      }));

  for (web::WebFrame* frame : frames_manager->GetAllWebFrames()) {
    auto* driver =
        AutofillDriverIOS::FromWebStateAndWebFrame(web_state(), frame);
    auto& manager =
        static_cast<TestAutofillManager&>(driver->GetAutofillManager());

    // Extraction will have triggered on page load. Wait for this to complete.
    EXPECT_TRUE(manager.WaitForFormsSeen(1));
    manager.ResetTestState();

    // Manually retrigger extraction, and wait for a fresh FormsSeen event.
    test_api(*driver).TriggerFormExtractionInDriverFrame();
    EXPECT_TRUE(manager.WaitForFormsSeen(1));
  }
}

// Tests that extraction can be done across frames to constitute a browser form.
TEST_F(AutofillAcrossIframesTest, TriggerExtraction_AcrossFrames) {
  EmbeddedTestServer test_server1;

  ServeCrossOriginDocument("cf1", "<form><input id='address'></form>",
                           &test_server1);
  ASSERT_TRUE(test_server1.Start());

  AddInput("text", "name");
  AddCrossOriginIframe("cf1", &test_server1);

  StartTestServerAndLoad();

  // Verify that the browser form is fully constructed from the main frame
  // and the other cross origin frame, totalling 2 fields.
  ASSERT_TRUE(
      WaitForCompleteBrowserForm(/*child_frames_count=*/1u, /*fields_count=*/2u)
          .second);
}

// Tests that the feature does not break filling in the main frame.
TEST_F(AutofillAcrossIframesTest, Fill_MainFrameForm) {
  const std::u16string kNamePlaceholder = u"Name";
  const std::u16string kFakeName = u"Bob Bobbertson";
  const std::u16string kPhonePlaceholder = u"Phone";
  const std::u16string kFakePhone = u"18005551234";

  AddInput("text", base::UTF16ToUTF8(kNamePlaceholder));
  AddInput("text", base::UTF16ToUTF8(kPhonePlaceholder));
  StartTestServerAndLoad();

  ASSERT_TRUE(main_frame_manager().WaitForFormsSeen(1));
  ASSERT_EQ(main_frame_manager().seen_forms().size(), 1u);

  // Copy the extracted form and put a name and phone number in it.
  FormData form = main_frame_manager().seen_forms()[0];
  base::flat_map<FieldGlobalId, FieldType> field_type_map;

  for (FormFieldData& field : test_api(form).fields()) {
    if (field.placeholder() == kNamePlaceholder) {
      field.set_value(kFakeName);
      field_type_map[field.global_id()] = FieldType::NAME_FULL;
    } else if (field.placeholder() == kPhonePlaceholder) {
      field.set_value(kFakePhone);
      field_type_map[field.global_id()] = FieldType::NAME_FULL;
    } else {
      ADD_FAILURE() << "Found unexpected field with placeholder: "
                    << field.placeholder();
    }
    field.set_is_autofilled(true);
    field.set_is_user_edited(false);
  }

  main_frame_driver()->ApplyFormAction(
      mojom::FormActionType::kFill, mojom::ActionPersistence::kFill,
      form.fields(), form.main_frame_origin(), field_type_map);

  ASSERT_TRUE(main_frame_manager().WaitForFormsFilled(1));
  ASSERT_EQ(main_frame_manager().filled_forms().size(), 1u);

  // Inspect the extracted, filled form, and ensure the expected data was
  // filled into the desired fields.
  const FormData& filled_form = main_frame_manager().filled_forms()[0];
  ASSERT_EQ(filled_form.fields().size(), 2u);
  for (const FormFieldData& field : filled_form.fields()) {
    if (field.placeholder() == kNamePlaceholder) {
      EXPECT_EQ(field.value(), kFakeName);
    } else if (field.placeholder() == kPhonePlaceholder) {
      EXPECT_EQ(field.value(), kFakePhone);
    } else {
      ADD_FAILURE() << "Found unexpected field with placeholder: "
                    << field.placeholder();
    }
  }
}

// Tests filling across multiple frames in the same forms tree structure.
TEST_F(AutofillAcrossIframesTest, Fill_MultiFrameForm) {
  const std::u16string kNamePlaceholder = u"Name";
  const std::u16string kFakeName = u"Bob Bobbertson";
  const std::u16string kPhonePlaceholder = u"Phone";
  const std::u16string kFakePhone = u"18005551234";

  AddIframe("cf1", "<form><input type=\"text\" placeholder=\"" +
                       base::UTF16ToUTF8(kNamePlaceholder) + "\"></form>");
  AddIframe("cf2", "<form><input type=\"text\" placeholder=\"" +
                       base::UTF16ToUTF8(kPhonePlaceholder) + "\"></form>");
  StartTestServerAndLoad();

  // Wait for the 3 forms to be reported as seen to the main frame that hosts
  // the browser form (which is the flattened representation of all forms in the
  // tree structure that share the form in the main frame as a common root).
  ASSERT_TRUE(main_frame_manager().WaitForFormsSeen(3));
  ASSERT_EQ(main_frame_manager().seen_forms().size(), 3u);

  // Pick the last form that was seen which reflects the latest and most
  // complete state of the browser form.
  FormData form = main_frame_manager().seen_forms().back();
  ASSERT_EQ(form.child_frames().size(), 2u);
  ASSERT_EQ(form.fields().size(), 2u);

  base::flat_map<FieldGlobalId, FieldType> field_type_map;

  std::vector<FormFieldData> fields = form.fields();

  FormFieldData* name_field =
      GetFieldWithPlaceholder(kNamePlaceholder, &fields);
  FormFieldData* phone_field =
      GetFieldWithPlaceholder(kPhonePlaceholder, &fields);

  // Set fill data for name field.
  SetFillDataForField(kFakeName, FieldType::NAME_FULL, name_field,
                      &field_type_map);
  // Set fill data for phone field.
  SetFillDataForField(kFakePhone, FieldType::PHONE_HOME_NUMBER, phone_field,
                      &field_type_map);

  base::flat_set<FieldGlobalId> filled_field_ids =
      main_frame_driver()->ApplyFormAction(
          mojom::FormActionType::kFill, mojom::ActionPersistence::kFill, fields,
          form.main_frame_origin(), field_type_map);

  EXPECT_THAT(filled_field_ids, UnorderedElementsAre(name_field->global_id(),
                                                     phone_field->global_id()));

  // Wait that the 2 forms are filled, one for each frame. The fill events are
  // all routed to the frame hosting the browser form, which corresponds to the
  // root form in the forms structure.
  ASSERT_TRUE(main_frame_manager().WaitForFormsFilled(2));
  ASSERT_EQ(main_frame_manager().filled_forms().size(), 2u);

  // Inspect the extracted, filled form, and ensure the expected data was
  // filled into the desired fields, where the last form correspond to the
  // most up to date snapshot of the browser form, a virtual form flattened
  // across frames and forms in the same tree.
  FormData filled_form = main_frame_manager().filled_forms().back();
  EXPECT_THAT(
      filled_form.fields(),
      UnorderedElementsAre(
          // Verify the name field.
          AllOf(Property(&FormFieldData::placeholder, kNamePlaceholder),
                Property(&FormFieldData::value, kFakeName)),
          // Verify the phone field.
          AllOf(Property(&FormFieldData::placeholder, kPhonePlaceholder),
                Property(&FormFieldData::value, kFakePhone))));
}

// Tests filling fields singularly, one by one, in a multi frame form.
// This tests the scenario where the user fills the form with the fill data
// from a suggestion they've selected.
TEST_F(AutofillAcrossIframesTest, DISABLED_FillMultiFrameForm_SingleField) {
  const std::u16string kNamePlaceholder = u"Name";
  const std::u16string kFakeName = u"Bob Bobbertson";
  const std::u16string kPhonePlaceholder = u"Phone";
  const std::u16string kFakePhone = u"18005551234";

  AddIframe("cf1", "<form><input type=\"text\" placeholder=\"" +
                       base::UTF16ToUTF8(kNamePlaceholder) +
                       "\" id=\"name-field\"></form>");
  AddIframe("cf2", "<form><input type=\"text\" placeholder=\"" +
                       base::UTF16ToUTF8(kPhonePlaceholder) +
                       "\" id=\"phone-field\"></form>");
  StartTestServerAndLoad();

  // Wait for the 3 forms to be reported as seen to the main frame that hosts
  // the browser form (which is the flattened representation of all forms in the
  // tree structure that share a common parent).
  ASSERT_TRUE(main_frame_manager().WaitForFormsSeen(3));
  ASSERT_EQ(main_frame_manager().seen_forms().size(), 3u);

  // Pick the last form that was seen which reflects the latest and most
  // complete state of the browser form.
  FormData form = main_frame_manager().seen_forms().back();
  ASSERT_EQ(form.child_frames().size(), 2u);
  ASSERT_EQ(form.fields().size(), 2u);

  std::vector<FormFieldData> fields = form.fields();

  FormFieldData* name_field =
      GetFieldWithPlaceholder(kNamePlaceholder, &fields);
  ASSERT_TRUE(name_field);
  FormFieldData* phone_field =
      GetFieldWithPlaceholder(kPhonePlaceholder, &fields);
  ASSERT_TRUE(phone_field);

  // Fill each field individually, one by one.
  main_frame_driver()->ApplyFieldAction(mojom::FieldActionType::kReplaceAll,
                                        mojom::ActionPersistence::kFill,
                                        name_field->global_id(), kFakeName);
  main_frame_driver()->ApplyFieldAction(mojom::FieldActionType::kReplaceAll,
                                        mojom::ActionPersistence::kFill,
                                        phone_field->global_id(), kFakePhone);

  // Verify that the name field was filled.
  {
    web::WebFrame* frame = GetFrameByID(name_field->host_frame().ToString());
    ASSERT_TRUE(frame);
    EXPECT_TRUE(WaitOnFieldFilledWithValue(frame, "name-field", kFakeName));
  }

  // Verify that the phone field was filled.
  {
    web::WebFrame* frame = GetFrameByID(phone_field->host_frame().ToString());
    ASSERT_TRUE(frame);
    EXPECT_TRUE(WaitOnFieldFilledWithValue(frame, "phone-field", kFakePhone));
  }
}

// Tests that the data from the multi frame browser form is passed upon
// submission. This tests the scenario where the user submits the form where
// they might be asked whether they want to save their profile.
TEST_F(AutofillAcrossIframesTest, SubmitMultiFrameForm) {
  const std::u16string kNamePlaceholder = u"Name";
  const std::u16string kFakeName = u"Bob Bobbertson";
  const std::u16string kPhonePlaceholder = u"Phone";
  const std::u16string kFakePhone = u"18005551234";

  AddIframe("cf1", "<form><input type=\"text\" placeholder=\"" +
                       base::UTF16ToUTF8(kNamePlaceholder) + "\"></form>");
  AddIframe("cf2", "<form><input type=\"text\" placeholder=\"" +
                       base::UTF16ToUTF8(kPhonePlaceholder) + "\"></form>");
  StartTestServerAndLoad();

  // Wait for the 3 forms to be reported as seen to the main frame that hosts
  // the browser form (which is the flattened representation of all forms in the
  // tree structure that share the form in the main frame as a common root).
  ASSERT_TRUE(main_frame_manager().WaitForFormsSeen(3));
  ASSERT_EQ(main_frame_manager().seen_forms().size(), 3u);

  // Pick the last form that was seen which reflects the latest and most
  // complete state of the browser form.
  const FormData& form = main_frame_manager().seen_forms().back();
  ASSERT_EQ(form.child_frames().size(), 2u);
  ASSERT_EQ(form.fields().size(), 2u);

  std::vector<FieldGlobalId> field_global_ids(form.fields().size());
  base::ranges::transform(
      form.fields(), field_global_ids.begin(),
      [](const FormFieldData& field) { return field.global_id(); });

  main_frame_driver()->FormSubmitted(main_frame_manager().seen_forms().front(),
                                     /*known_success=*/true,
                                     mojom::SubmissionSource::FORM_SUBMISSION);

  // Wait on the main frame form to report itself as submitted, which is the
  // only form in the forms tree that was submitted.
  ASSERT_TRUE(main_frame_manager().WaitForFormsSubmitted(1));
  ASSERT_EQ(main_frame_manager().submitted_forms().size(), 1u);

  // Verify that the submitted form represent the browser form across frames.
  const FormData& submitted_form = main_frame_manager().submitted_forms()[0];
  EXPECT_THAT(submitted_form.fields(),
              UnorderedElementsAre(
                  Property(&FormFieldData::global_id, field_global_ids[0]),
                  Property(&FormFieldData::global_id, field_global_ids[1])));
}

// Tests that, when asked for, there is a query made to retrive fill data for
// the entire browser form, across frames. This tests the scenario where
// Autofill suggestions are provided to the user upon taping on one of the
// fields in the form.
TEST_F(AutofillAcrossIframesTest, AskForFillDataOnMultiFrameForm) {
  const std::u16string kNamePlaceholder = u"Name";
  const std::u16string kFakeName = u"Bob Bobbertson";
  const std::u16string kPhonePlaceholder = u"Phone";
  const std::u16string kFakePhone = u"18005551234";

  AddIframe("cf1", "<form><input type=\"text\" placeholder=\"" +
                       base::UTF16ToUTF8(kNamePlaceholder) + "\"></form>");
  AddIframe("cf2", "<form><input type=\"text\" placeholder=\"" +
                       base::UTF16ToUTF8(kPhonePlaceholder) + "\"></form>");
  StartTestServerAndLoad();

  // Wait for the 3 forms to be reported as seen to the main frame that hosts
  // the browser form (which is the flattened representation of all forms in the
  // tree structure that share the form in the main frame as a common root).
  ASSERT_TRUE(main_frame_manager().WaitForFormsSeen(3));
  ASSERT_EQ(main_frame_manager().seen_forms().size(), 3u);

  // Pick the last form that was seen which reflects the latest and most
  // complete state of the browser form.
  FormData form = main_frame_manager().seen_forms().back();
  ASSERT_EQ(form.child_frames().size(), 2u);
  ASSERT_EQ(form.fields().size(), 2u);

  std::vector<FieldGlobalId> field_global_ids(form.fields().size());
  base::ranges::transform(
      form.fields(), field_global_ids.begin(),
      [](const FormFieldData& field) { return field.global_id(); });

  std::vector<FormFieldData> fields = form.fields();

  FormFieldData* name_field =
      GetFieldWithPlaceholder(kNamePlaceholder, &fields);

  main_frame_driver()->AskForValuesToFill(form, name_field->global_id());

  // Wait on the main frame form to report itself as having fill data for the
  // entire browser form, across frames.
  ASSERT_TRUE(main_frame_manager().WaitForFormsAskedForFillData(1));
  ASSERT_EQ(main_frame_manager().ask_for_filldata_forms().size(), 1u);

  // Verify that the form that we ask fill data for represents the browser form
  // across frames.
  const FormData& filldata_form =
      main_frame_manager().ask_for_filldata_forms()[0];
  EXPECT_THAT(filldata_form.fields(),
              UnorderedElementsAre(
                  Property(&FormFieldData::global_id, field_global_ids[0]),
                  Property(&FormFieldData::global_id, field_global_ids[1])));
}

// Tests that any text change on one of the child frames is correctly routed
// to the parent form where it represents the whole browser form.
TEST_F(AutofillAcrossIframesTest, TextChangeOnMultiFrameForm) {
  const std::u16string kNamePlaceholder = u"Name";
  const std::u16string kFakeName = u"Bob Bobbertson";
  const std::u16string kPhonePlaceholder = u"Phone";
  const std::u16string kFakePhone = u"18005551234";

  AddIframe("cf1", "<form><input type=\"text\" placeholder=\"" +
                       base::UTF16ToUTF8(kNamePlaceholder) + "\"></form>");
  AddIframe("cf2", "<form><input type=\"text\" placeholder=\"" +
                       base::UTF16ToUTF8(kPhonePlaceholder) + "\"></form>");
  StartTestServerAndLoad();

  // Wait for the 3 forms to be reported as seen to the main frame that hosts
  // the browser form (which is the flattened representation of all forms in the
  // tree structure that share the form in the main frame as a common root).
  ASSERT_TRUE(main_frame_manager().WaitForFormsSeen(3));
  ASSERT_EQ(main_frame_manager().seen_forms().size(), 3u);

  // Pick the last form that was seen which reflects the latest and most
  // complete state of the browser form.
  FormData form = main_frame_manager().seen_forms().back();
  ASSERT_EQ(form.child_frames().size(), 2u);
  ASSERT_EQ(form.fields().size(), 2u);

  std::vector<FieldGlobalId> field_global_ids(form.fields().size());
  base::ranges::transform(
      form.fields(), field_global_ids.begin(),
      [](const FormFieldData& field) { return field.global_id(); });

  std::vector<FormFieldData> fields = form.fields();

  FormFieldData* name_field =
      GetFieldWithPlaceholder(kNamePlaceholder, &fields);

  main_frame_driver()->TextFieldDidChange(form, name_field->global_id(),
                                          base::TimeTicks::Now());

  // Wait on the main frame form to report itself as having fill data for the
  // entire browser form, across frames.
  ASSERT_TRUE(main_frame_manager().WaitOnTextFieldDidChange(1));
  ASSERT_EQ(main_frame_manager().text_filled_did_change_forms().size(), 1u);

  // Verify that the form that we ask fill data for represents the browser form
  // across frames.
  const FormData& text_filled_form =
      main_frame_manager().text_filled_did_change_forms()[0];
  EXPECT_THAT(text_filled_form.fields(),
              UnorderedElementsAre(
                  Property(&FormFieldData::global_id, field_global_ids[0]),
                  Property(&FormFieldData::global_id, field_global_ids[1])));
}

// Tests that frame deletion is taken into consideration where the browser form
// is updated accordingly.
TEST_F(AutofillAcrossIframesTest, UpdateOnFrameDeletion) {
  const std::u16string kNamePlaceholder = u"Name";
  const std::u16string kFakeName = u"Bob Bobbertson";
  const std::u16string kPhonePlaceholder = u"Phone";
  const std::u16string kFakePhone = u"18005551234";

  AddIframe("cf1", "<form><input type=\"text\" placeholder=\"" +
                       base::UTF16ToUTF8(kNamePlaceholder) + "\"></form>");
  AddIframe("cf2", "<form><input type=\"text\" placeholder=\"" +
                       base::UTF16ToUTF8(kPhonePlaceholder) + "\"></form>");
  StartTestServerAndLoad();

  // Wait for the 3 forms to be reported as seen to the main frame that hosts
  // the browser form (which is the flattened representation of all forms in the
  // tree structure that share the form in the main frame as a common root).
  ASSERT_TRUE(main_frame_manager().WaitForFormsSeen(3));
  ASSERT_EQ(main_frame_manager().seen_forms().size(), 3u);

  // Pick the last form that was seen which reflects the latest and most
  // complete state of the browser form.
  FormData form = main_frame_manager().seen_forms().back();
  ASSERT_EQ(form.child_frames().size(), 2u);
  ASSERT_EQ(form.fields().size(), 2u);

  ASSERT_TRUE(ExecuteJavaScriptInFrame(
      WaitForMainFrame(),
      u"document.forms[0].getElementsByTagName('iframe')[0].remove();"));

  base::flat_map<FieldGlobalId, FieldType> field_type_map;

  std::vector<FormFieldData> fields = form.fields();

  FormFieldData* name_field =
      GetFieldWithPlaceholder(kNamePlaceholder, &fields);
  FormFieldData* phone_field =
      GetFieldWithPlaceholder(kPhonePlaceholder, &fields);

  // Set fill data for name field.
  SetFillDataForField(kFakeName, FieldType::NAME_FULL, name_field,
                      &field_type_map);
  // Set fill data for phone field.
  SetFillDataForField(kFakePhone, FieldType::PHONE_HOME_NUMBER, phone_field,
                      &field_type_map);

  // Attempt to fill the 2 fields in the browser form while there is actually
  // only one.
  ASSERT_THAT(main_frame_driver()->ApplyFormAction(
                  mojom::FormActionType::kFill, mojom::ActionPersistence::kFill,
                  fields, form.main_frame_origin(), field_type_map),
              SizeIs(1));

  // Wait on the fill to be done.
  ASSERT_TRUE(main_frame_manager().WaitForFormsFilled(1));
  ASSERT_EQ(main_frame_manager().filled_forms().size(), 1u);

  // Verify that the form was updated to take into consideration the deleted
  // frame where the is only one field that is actually filled.
  FormData filled_form = main_frame_manager().filled_forms().back();
  EXPECT_THAT(
      filled_form.fields(),
      UnorderedElementsAre(
          // Verify the phone field.
          AllOf(Property(&FormFieldData::placeholder, kPhonePlaceholder),
                Property(&FormFieldData::value, kFakePhone))));
}

// Tests that form deletion in a child frame is taken into consideration where
// the parent browser form is updated accordingly.
TEST_F(AutofillAcrossIframesTest, UpdateOnFormDeletion) {
  AddIframe("cf1", "<form><input type=\"text\"></form>");
  AddIframe("cf2", "<form><input type=\"text\"></form>");
  StartTestServerAndLoad();

  // Wait for the 3 forms to be reported as seen to the main frame that hosts
  // the browser form (which is the flattened representation of all forms in the
  // tree structure that share the form in the main frame as a common root).
  ASSERT_TRUE(main_frame_manager().WaitForFormsSeen(3));
  ASSERT_EQ(main_frame_manager().seen_forms().size(), 3u);

  FormGlobalId browser_form_global_id;
  {
    const FormData& form = main_frame_manager().seen_forms().back();
    browser_form_global_id = form.global_id();
    // There should be 2 fields in the initial browser form.
    ASSERT_EQ(2u, form.fields().size());
  }

  main_frame_manager().ResetTestState();

  {
    // Remove form in the top child frame.
    const std::u16string script =
        u"const frame = document.querySelector('iframe'); "
        "frame.contentWindow.eval('document.forms[0].remove()');";
    ASSERT_TRUE(ExecuteJavaScriptInFrame(WaitForMainFrame(), script));
  }

  // Wait for the deleted form to be reported as seen to the main frame that
  // hosts the browser form.
  ASSERT_TRUE(main_frame_manager().WaitForFormsSeen(1));
  ASSERT_EQ(main_frame_manager().removed_forms().size(), 1u);

  // Verify that the field count is now 1 for the xframes browser form since
  // there was one form containing one field that was deleted.
  FormStructure* form =
      main_frame_manager().FindCachedFormById(browser_form_global_id);
  ASSERT_TRUE(form);
  EXPECT_EQ(1u, form->field_count());
}

// Tests that synthethic form deletion in a child frame is taken into
// consideration where the parent browser form is updated accordingly.
TEST_F(AutofillAcrossIframesTest, UpdateOnFormDeletion_Synthetic) {
  AddIframe("cf1", "<div id=\"form1\"><input type=\"text\">"
                   "<input type=\"text\"></div>");
  AddIframe("cf2", "<div id=\"form1\"><input type=\"text\">"
                   "<input type=\"text\"></div>");
  StartTestServerAndLoad();

  // Wait for the 3 forms to be reported as seen to the main frame that hosts
  // the browser form (which is the flattened representation of all forms in the
  // tree structure that share the form in the main frame as a common root).
  ASSERT_TRUE(main_frame_manager().WaitForFormsSeen(3));
  ASSERT_EQ(main_frame_manager().seen_forms().size(), 3u);

  FormGlobalId browser_form_global_id;
  {
    const FormData& form = main_frame_manager().seen_forms().back();
    browser_form_global_id = form.global_id();
    // There should be 4 fields in the initial browser form, 2 per frame.
    ASSERT_EQ(4u, form.fields().size());
  }

  main_frame_manager().ResetTestState();

  {
    // Remove all input fields in the top child frame, to repoduce synthetic
    // form deletion.
    const std::u16string script =
        u"const frame = document.querySelector('iframe'); "
        "frame.contentWindow.eval(\"document.getElementById('form1')"
        ".remove()\");";
    ASSERT_TRUE(ExecuteJavaScriptInFrame(WaitForMainFrame(), script));
  }

  // Wait for the deleted form to be reported as seen to the main frame that
  // hosts the browser form.
  ASSERT_TRUE(main_frame_manager().WaitForFormsSeen(1));
  ASSERT_EQ(main_frame_manager().removed_forms().size(), 1u);

  // Verify that the field count is now 2 for the xframe browser form since
  // the synthetic form in one of the frames was deleted.
  FormStructure* form =
      main_frame_manager().FindCachedFormById(browser_form_global_id);
  ASSERT_TRUE(form);
  EXPECT_EQ(2u, form->field_count());
}

// Tests that the partial deletion of fields in the synthethic form of a child
// frame isn't reported as form removal since the synthetic still remains.
TEST_F(AutofillAcrossIframesTest, UpdateOnFormDeletion_Synthetic_Partial) {
  AddIframe("cf1", "<input type=\"text\">"
                   "<input type=\"text\">");
  AddIframe("cf2", "<input type=\"text\">"
                   "<input type=\"text\">");
  StartTestServerAndLoad();

  // Wait for the 3 forms to be reported as seen to the main frame that hosts
  // the browser form (which is the flattened representation of all forms in the
  // tree structure that share the form in the main frame as a common root).
  ASSERT_TRUE(main_frame_manager().WaitForFormsSeen(3));
  ASSERT_EQ(main_frame_manager().seen_forms().size(), 3u);

  FormGlobalId browser_form_global_id;
  {
    const FormData& form = main_frame_manager().seen_forms().back();
    browser_form_global_id = form.global_id();
    // There should be 4 fields in the initial browser form, 2 per frame.
    ASSERT_EQ(4u, form.fields().size());
  }

  main_frame_manager().ResetTestState();

  {
    // Remove one input field in the top child frame without entirely deleting
    // the synthethic form.
    const std::u16string script =
        u"const frame = document.querySelector('iframe'); "
        "frame.contentWindow.eval(\"document.querySelector('input').remove()\")"
        ";";
    ASSERT_TRUE(ExecuteJavaScriptInFrame(WaitForMainFrame(), script));
  }

  // Give some time to handle the deleted input field.
  base::test::ios::SpinRunLoopWithMinDelay(base::Milliseconds(200));

  // Verify that there we no forms reported as removed.
  EXPECT_EQ(main_frame_manager().removed_forms().size(), 0u);

  // Verify that the field count is still 4 for the xframe browser form since
  // the synthetic form in one of the frames was deleted.
  FormStructure* form =
      main_frame_manager().FindCachedFormById(browser_form_global_id);
  ASSERT_TRUE(form);
  EXPECT_EQ(4u, form->field_count());
}

// Tests that double registration is correctly notified.
TEST_F(AutofillAcrossIframesTest, FrameDoubleRegistration_Notify) {
  const std::u16string kNamePlaceholder = u"Name";
  const std::u16string kFakeName = u"Bob Bobbertson";
  const std::u16string kPhonePlaceholder = u"Phone";
  const std::u16string kFakePhone = u"18005551234";

  AddIframe("cf1", "<form><input type=\"text\" placeholder=\"" +
                       base::UTF16ToUTF8(kNamePlaceholder) + "\"></form>");
  AddIframe("cf2", "<form><input type=\"text\" placeholder=\"" +
                       base::UTF16ToUTF8(kPhonePlaceholder) + "\"></form>");
  StartTestServerAndLoad();

  // Wait for the 3 forms to be reported as seen to the main frame that hosts
  // the browser form (which is the flattened representation of all forms in the
  // tree structure that share the form in the main frame as a common root).
  ASSERT_TRUE(main_frame_manager().WaitForFormsSeen(3));
  ASSERT_EQ(main_frame_manager().seen_forms().size(), 3u);

  // Pick the last form that was seen which reflects the latest and most
  // complete state of the browser form.
  FormData form = main_frame_manager().seen_forms().back();
  ASSERT_EQ(form.child_frames().size(), 2u);
  ASSERT_EQ(form.fields().size(), 2u);

  main_frame_manager().ResetTestState();

  // Inject the spoofy frame that will attempt double registration.
  {
    std::u16string script = u"const doc = `<body><form>"
                            "<input type=\"text\" placeholder=\"Stolen Name\">"
                            "</form></body>`;"
                            "const iframe = document.createElement('iframe');"
                            "iframe.srcdoc = doc;"
                            "document.body.appendChild(iframe); true";
    ASSERT_TRUE(ExecuteJavaScriptInFrame(WaitForMainFrame(), script));
  }

  web::WebFrame* spoofy_frame = WaitForNewFrame();
  ASSERT_TRUE(spoofy_frame);

  TestAutofillManager* spoofy_manager = GetManagerForFrame(spoofy_frame);
  ASSERT_TRUE(spoofy_manager);

  // Wait for the spoofy frame forms to be seen so they were be ingested by the
  // system.
  ASSERT_TRUE(spoofy_manager->WaitForFormsSeen(1));
  ASSERT_EQ(spoofy_manager->seen_forms().size(), 1u);

  // Pick the last form that was seen which reflects the latest and most
  // complete state of the browser form, but should be the spoofy form in this
  // case as this is in a separate tree from the other browser form (a single
  // node in this case).
  FormData spoofy_form = spoofy_manager->seen_forms().back();
  ASSERT_EQ(spoofy_form.fields().size(), 1u);

  MockRegistrarObserver registrar_observer;
  base::ScopedObservation<autofill::ChildFrameRegistrar,
                          autofill::ChildFrameRegistrarObserver>
      registrar_scoped_observation{&registrar_observer};
  registrar_scoped_observation.Observe(registrar());

  RemoteFrameToken stolen_remote_token =
      absl::get<RemoteFrameToken>(form.child_frames()[0].token);
  std::optional<LocalFrameToken> attacked_frame =
      registrar()->LookupChildFrame(stolen_remote_token);
  ASSERT_TRUE(attacked_frame);

  // Expect that double registration is notified for the frame that was attacked
  // which has its remote token stolen.
  EXPECT_CALL(registrar_observer, OnDidDoubleRegistration(*attacked_frame))
      .Times(1);

  {
    const std::u16string script = base::StrCat(
        {u"__gCrWeb.common.sendWebKitMessage('FormHandlersMessage', "
         u"{'command': 'registerAsChildFrame', 'local_frame_id': "
         u"__gCrWeb.frameId, 'remote_frame_id':'",
         base::UTF8ToUTF16(stolen_remote_token.ToString()), u"'});"});
    ASSERT_TRUE(ExecuteJavaScriptInFrame(spoofy_frame, script));
  }
}

// Tests that a frame can be unregistered without necessarily being deleted when
// detecting a spoofing attempt for example.
TEST_F(AutofillAcrossIframesTest, FrameDoubleRegistration_Unregister) {
  const std::u16string kNamePlaceholder = u"Name";
  const std::u16string kFakeName = u"Bob Bobbertson";
  const std::u16string kPhonePlaceholder = u"Phone";
  const std::u16string kFakePhone = u"18005551234";

  AddIframe("cf1", "<form><input type=\"text\" placeholder=\"" +
                       base::UTF16ToUTF8(kNamePlaceholder) + "\"></form>");
  AddIframe("cf2", "<form><input type=\"text\" placeholder=\"" +
                       base::UTF16ToUTF8(kPhonePlaceholder) + "\"></form>");
  StartTestServerAndLoad();

  // Wait for the 3 forms to be reported as seen to the main frame that hosts
  // the browser form (which is the flattened representation of all forms in the
  // tree structure that share the form in the main frame as a common root).
  ASSERT_TRUE(main_frame_manager().WaitForFormsSeen(3));
  ASSERT_EQ(main_frame_manager().seen_forms().size(), 3u);

  // Pick the last form that was seen which reflects the latest and most
  // complete state of the browser form.
  FormData browser_form = main_frame_manager().seen_forms().back();
  ASSERT_EQ(browser_form.child_frames().size(), 2u);
  ASSERT_EQ(browser_form.fields().size(), 2u);

  std::vector<FormFieldData> fields_to_fill = browser_form.fields();

  FormFieldData* name_field =
      GetFieldWithPlaceholder(kNamePlaceholder, &fields_to_fill);
  FormFieldData* phone_field =
      GetFieldWithPlaceholder(kPhonePlaceholder, &fields_to_fill);
  ASSERT_TRUE(name_field && phone_field);

  // Pick one non-main frame to unregister based on field, the name field in
  // this case. Since there is only one frame per field, we know that deleting
  // the frame will only concern that field.
  const LocalFrameToken frame_to_unregister = name_field->host_frame();

  // Unregister the frame (via the driver) of the name field.
  {
    auto* driver = AutofillDriverIOS::FromWebStateAndLocalFrameToken(
        web_state(), frame_to_unregister);
    ASSERT_TRUE(driver);
    driver->Unregister();
  }

  base::flat_map<FieldGlobalId, FieldType> field_type_map;

  // Set fill data for both fields.
  SetFillDataForField(kFakeName, FieldType::NAME_FULL, name_field,
                      &field_type_map);
  SetFillDataForField(kFakePhone, FieldType::PHONE_HOME_NUMBER, phone_field,
                      &field_type_map);

  // Verify that the only the phone field will be filled, where the name field
  // in the unregistered frame shouldn't be filled.
  EXPECT_THAT(
      main_frame_driver()->ApplyFormAction(
          mojom::FormActionType::kFill, mojom::ActionPersistence::kFill,
          fields_to_fill, browser_form.main_frame_origin(), field_type_map),
      UnorderedElementsAre(phone_field->global_id()));

  main_frame_manager().ResetTestState();
}

// Tests that forms aren't parsed when their host frame ID differs from the ID
// of the frame on which forms extraction was requested.
TEST_F(AutofillAcrossIframesTest, FrameAndFormIdsDontMatch) {
  // Serve form on main frame.
  AddInput("text", "name");
  AddInput("text", "address");
  StartTestServerAndLoad();

  // Verify that the form can be parsed, intially.
  ASSERT_TRUE(main_frame_manager().WaitForFormsSeen(1));
  ASSERT_EQ(main_frame_manager().seen_forms().size(), 1u);
  main_frame_manager().ResetTestState();

  // Change the ID of the main frame on the renderer side but not in the
  // browser, making the two IDs different.
  {
    web::WebFrame* main_frame = WaitForMainFrame();
    std::string new_frame_id = main_frame->GetFrameId();
    // Reverse the main frame id to make it a brand new id.
    std::reverse(new_frame_id.begin(), new_frame_id.end());

    // Change the frame ID provided by getFrameId() to simulate a different
    // frame receiving the forms extraction request.
    std::u16string script = u"__gCrWeb.message.getFrameId = () => "
                            "'1effd8f52a067c8d3a01762d3c41dfd8'; true";
    ASSERT_TRUE(ExecuteJavaScriptInFrame(main_frame, script));
  }

  // Trigger extraction on the `main_frame` where the frame ID obtained within
  // the script during extraction is different from the ID the main frame was
  // initially registered with.
  test_api(*main_frame_driver()).TriggerFormExtractionInDriverFrame();

  // Give enough time for the JS request to be done.
  base::test::ios::SpinRunLoopWithMinDelay(base::Seconds(2));

  // Verify that no forms could be parsed (hence seen) this time because the
  // forms had a different frame ID than the frame ID for the request hence the
  // extracted forms couldn't be parsed, resulting in no forms seen.
  ASSERT_EQ(main_frame_manager().seen_forms().size(), 0u);
}

// Ensure that disabling the feature actually disables the feature.
TEST_F(AutofillAcrossIframesTest, FeatureDisabled) {
  base::test::ScopedFeatureList disable;
  disable.InitAndDisableFeature(features::kAutofillAcrossIframesIos);

  AddIframe("cf1", "child frame 1");
  AddInput("text", "name");
  AddIframe("cf2", "child frame 2");
  AddInput("text", "address");
  StartTestServerAndLoad();

  ASSERT_TRUE(main_frame_manager().WaitForFormsSeen(1));
  ASSERT_EQ(main_frame_manager().seen_forms().size(), 1u);

  const FormData& form = main_frame_manager().seen_forms()[0];
  EXPECT_EQ(form.child_frames().size(), 0u);

  EXPECT_FALSE(
      autofill::ChildFrameRegistrar::GetOrCreateForWebState(web_state()));
}

// Suite of tests that focuses on testing the security of xframe filling.
//
// These tests verify that the filling horizon is respected for the ios
// implementation, where (1) the main frame can be filled with non-sensitive
// data, (2) rule (1) also applies to direct child frames of the main frame that
// are on the same origin as the main frame, and (3) the frames on the same
// origin as the trigger field can be filled. No special permissions can be
// granted from the element itself.
using AutofillAcrossIframesFillSecurityTest = AutofillAcrossIframesTest;

// Tests filling a credit card form from a cross origin frame as the trigger.
// Also tests that fields on the main origin can be filled with non-sensitive
// data.
//
// Representation of the tested xframe form structure with the expected outcome
// in [] next to each input field and trigger field indicated with <--:
// =======================================
// Main Frame
//   Input: name [filled]
//   Iframe (origin1):
//     Input: cc number [filled] <--
//   Iframe (main origin):
//     Input: exp date [filled]
//   Iframe (origin1):
//     Input: cvc [filled]
// =======================================
TEST_F(AutofillAcrossIframesFillSecurityTest, XoriginTrigger) {
  EmbeddedTestServer test_server1;

  TestCreditCardForm cc_form_info = GetTestCreditCardForm();

  // Serve the cc number and exp fields from the other origin. They are all on
  // the same origin.
  ServeCrossOriginDocument("cf1", cc_form_info.cc_number_field.ToHtmlForm(),
                           &test_server1);
  ServeCrossOriginDocument("cf3", cc_form_info.cvc_field.ToHtmlForm(),
                           &test_server1);
  ASSERT_TRUE(test_server1.Start());

  // Add the name input to the main frame.
  AddInput(cc_form_info.name_field);
  // Add iframe on the other origin that holds the credit card number.
  AddCrossOriginIframe("cf1", &test_server1);
  // Add iframe on main origin holding the exp field.
  AddIframe("cf2", cc_form_info.exp_field.ToHtmlForm());
  // Add iframe on the other holding the cvc field.
  AddCrossOriginIframe("cf3", &test_server1);

  // Start serving main frame content.
  StartTestServerAndLoad();

  // Wait on the browser form to be fully constructed from both the frame on the
  // main origin and the other cross origin frames, totalling 4 fields.
  const auto [browser_form, res] =
      WaitForCompleteBrowserForm(/*child_frames_count=*/3, /*fields_count=*/4);
  ASSERT_TRUE(res);

  // Fill and verify that all the fields are filled.
  FillAndVerify(cc_form_info, browser_form, cc_form_info.cc_number_field,
                cc_form_info.all_fields());
}

// Test that the shared-autofill permission isn't propagated to the nested
// frames on the main origin that aren't a direct children of the main
// frame. Fields on the same origin as the trigger field should be filled even
// if nested.
//
// Representation of the tested xframe form structure with the expected outcome
// in [] next to each input field and the trigger field indicated with <--:
// =======================================
// Main Frame
//   Iframe (main origin):
//     Iframe (main origin):
//       Input: name [not filled]
//   Iframe (origin1):
//     Input: cc number [filled] <--
//   Iframe (origin1):
//     Iframe (main origin):
//       Input: exp date [not filled]
//   Iframe (origin2):
//     Iframe (origin1)
//       Input: cvc [filled]
// =======================================
TEST_F(AutofillAcrossIframesFillSecurityTest, XoriginTrigger_NestedFrame) {
  EmbeddedTestServer test_server1;
  EmbeddedTestServer test_server2;

  TestCreditCardForm cc_form_info = GetTestCreditCardForm();

  // Serve documents in frames.

  // Serve the document with the name field on the main origin.
  ServeDocument("cf1a", cc_form_info.name_field.ToHtmlForm());
  // Serve document with the CC number field.
  ServeCrossOriginDocument("cf2", cc_form_info.cc_number_field.ToHtmlForm(),
                           &test_server1);
  // Serve empty document where we will inject the iframe with the expiry date
  // field later.
  ServeCrossOriginDocument("cf3", "<body></body>", &test_server1);
  // Serve the document with the exp field on the main origin.
  ServeDocument("cf3a", cc_form_info.exp_field.ToHtmlForm());
  // Serve empty document where we will inject the iframe with cvc field later.
  ServeCrossOriginDocument("cf4", "<body></body>", &test_server2);
  // Serve document with the CVC number field.
  ServeCrossOriginDocument("cf4a", cc_form_info.cvc_field.ToHtmlForm(),
                           &test_server1);
  ASSERT_TRUE(test_server1.Start());
  ASSERT_TRUE(test_server2.Start());

  // Add iframes to the main page.

  // Add iframe that hosts another nested iframe holding the name field, a
  // non-sensitive field. Both frames are from the main origin.
  AddIframe("cf1", "<body><iframe src='/cf1a'></iframe></body>");
  // Add iframe on another holding the credit card number field, a
  // sensitive field.
  AddCrossOriginIframe("cf2", &test_server1);
  // Add iframe on another origin that hosts another nested iframe on the main
  // origin holding the expiry date, a non-sensitive field.
  AddCrossOriginIframe("cf3", &test_server1);
  // Add iframe on another origin that hosts another nested iframe holding the
  // cvc field, a sensitive field.
  AddCrossOriginIframe("cf4", &test_server2);

  // Start serving main frame content.
  StartTestServerAndLoad();

  // Wait on the browser form to be fully constructed from both the frame on the
  // main origin and the other cross origin frames, totalling 2 fields. At this
  // state 2 of the 4 child frames are empty in which we will inject the missing
  // fields later.
  const auto [browser_form, res] =
      WaitForCompleteBrowserForm(/*child_frames_count=*/4, /*fields_count=*/2);
  ASSERT_TRUE(res);

  // Injects an iframe sourced from `server` at `path` as a child frame of the
  // frame that corresponds to `parent_remote_token`. Update the tree to take
  // the new frame.
  const auto InjectNewIframe = [&](RemoteFrameToken parent_remote_token,
                                   const EmbeddedTestServer& server,
                                   const std::string& path) {
    std::optional<LocalFrameToken> parent_frame_token =
        registrar()->LookupChildFrame(parent_remote_token);
    ASSERT_TRUE(parent_frame_token);
    web::WebFrame* parent_frame = GetFrameByID(parent_frame_token->ToString());
    ASSERT_TRUE(parent_frame);

    const std::u16string full_path =
        base::UTF8ToUTF16(server.GetURL(path).spec());

    // Inject nested frame in its parent frame.
    const std::u16string script =
        u"const iframe = document.createElement('iframe');"
        "iframe.src = '" +
        full_path +
        u"';"
        "document.body.appendChild(iframe); true";
    ASSERT_TRUE(ExecuteJavaScriptInFrame(parent_frame, script));

    web::WebFrame* new_frame = WaitForNewFrame();

    TestAutofillManager* new_frame_manager = GetManagerForFrame(new_frame);
    ASSERT_TRUE(new_frame);

    // Wait for the new frame forms to be seen so they can be ingested by
    // the system.
    ASSERT_TRUE(new_frame_manager->WaitForFormsSeen(1));
    ASSERT_EQ(new_frame_manager->seen_forms().size(), 1u);

    main_frame_manager().ResetTestState();

    // Re-trigger form extraction to add the new child frame to the tree.
    // This won't be needed anymore once we uncouple registration from form
    // extraction (crbug.com/358334625).
    auto* parent_frame_driver =
        AutofillDriverIOS::FromWebStateAndWebFrame(web_state(), parent_frame);
    test_api(*parent_frame_driver).TriggerFormExtractionInDriverFrame();
    ASSERT_TRUE(main_frame_manager().WaitForFormsSeen(2));
    ASSERT_EQ(main_frame_manager().seen_forms().size(), 2u);
  };

  ASSERT_THAT(browser_form.child_frames(), SizeIs(4));

  // Inject the frame holding the expiry date.
  InjectNewIframe(
      absl::get<RemoteFrameToken>(browser_form.child_frames()[2].token),
      test_server_, "/cf3a");
  // Inject the frame holding the cvc number.
  InjectNewIframe(
      absl::get<RemoteFrameToken>(browser_form.child_frames()[3].token),
      test_server1, "/cf4a");

  // Fill and verify that all the fields are filled.
  FillAndVerify(cc_form_info, main_frame_manager().seen_forms().back(),
                cc_form_info.cc_number_field,
                {cc_form_info.cc_number_field, cc_form_info.cvc_field});
}

// Tests that only the frame on the trigger origin is filled when all other
// frames are another origin that isn't the main frame origin. Testing that
// filling is correctly siloed. Triggers filling from each frame.
//
// Representation of the tested xframe form structure with the expected outcome
// in [] next to each input field and the trigger field indicated with <--:
// =======================================
// Main Frame
//   Iframe (origin1):
//     Input: name [filled] <-- #1
//   Iframe (origin2):
//     Input: cc number [filled] <-- #2
//   Iframe (origin3):
//     Input: exp date [filled] <-- #3
//   Iframe (origin4):
//     Input: cvc [filled] <-- #4
// =======================================
TEST_F(AutofillAcrossIframesFillSecurityTest,
       Fill_MultiFrameForm_XoriginTrigger_Siloed) {
  EmbeddedTestServer test_server1;
  EmbeddedTestServer test_server2;
  EmbeddedTestServer test_server3;
  EmbeddedTestServer test_server4;

  TestCreditCardForm cc_form_info = GetTestCreditCardForm();

  // Serve all fields their own specific origin.
  ServeCrossOriginDocument("cf1", cc_form_info.name_field.ToHtmlForm(),
                           &test_server1);
  ServeCrossOriginDocument("cf2", cc_form_info.cc_number_field.ToHtmlForm(),
                           &test_server2);
  ServeCrossOriginDocument("cf3", cc_form_info.exp_field.ToHtmlForm(),
                           &test_server3);
  ServeCrossOriginDocument("cf4", cc_form_info.cvc_field.ToHtmlForm(),
                           &test_server4);
  ASSERT_TRUE(test_server1.Start());
  ASSERT_TRUE(test_server2.Start());
  ASSERT_TRUE(test_server3.Start());
  ASSERT_TRUE(test_server4.Start());

  // Hold each field in an iframe on a different origin.
  AddCrossOriginIframe("cf1", &test_server1);
  AddCrossOriginIframe("cf2", &test_server2);
  AddCrossOriginIframe("cf3", &test_server3);
  AddCrossOriginIframe("cf4", &test_server4);

  // Start serving main frame content.
  StartTestServerAndLoad();

  // Wait on the browser form to be fully constructed including the fields
  // from all frames.
  const auto [browser_form, res] =
      WaitForCompleteBrowserForm(/*child_frames_count=*/4, /*fields_count=*/4);
  ASSERT_TRUE(res);

  std::vector<FormFieldData> fields = browser_form.fields();

  // Fill from each trigger field and verify that only the trigger field itself
  // is filled since this is the only field on the same origin as the trigger.
  // Verify that the fields in the form are only filled incrementaly, one by
  // one, as we are filling from the different origins.
  std::vector<FieldGlobalId> filled_fields_so_far;
  FillAndVerify(cc_form_info, browser_form, cc_form_info.name_field,
                {cc_form_info.name_field});
  filled_fields_so_far.push_back(
      CHECK_DEREF(GetFieldWithId(cc_form_info.name_field.id_attribute, fields))
          .global_id());
  EXPECT_TRUE(cc_form_info.VerifyFieldsAreCorrectlyFilled(
      web_frames_manager(), filled_fields_so_far));

  FillAndVerify(cc_form_info, browser_form, cc_form_info.cc_number_field,
                {cc_form_info.cc_number_field});
  filled_fields_so_far.push_back(
      CHECK_DEREF(
          GetFieldWithId(cc_form_info.cc_number_field.id_attribute, fields))
          .global_id());
  EXPECT_TRUE(cc_form_info.VerifyFieldsAreCorrectlyFilled(
      web_frames_manager(), filled_fields_so_far));

  FillAndVerify(cc_form_info, browser_form, cc_form_info.exp_field,
                {cc_form_info.exp_field});
  filled_fields_so_far.push_back(
      CHECK_DEREF(GetFieldWithId(cc_form_info.exp_field.id_attribute, fields))
          .global_id());
  EXPECT_TRUE(cc_form_info.VerifyFieldsAreCorrectlyFilled(
      web_frames_manager(), filled_fields_so_far));

  FillAndVerify(cc_form_info, browser_form, {cc_form_info.cvc_field},
                {cc_form_info.cvc_field});
  filled_fields_so_far.push_back(
      CHECK_DEREF(GetFieldWithId(cc_form_info.cvc_field.id_attribute, fields))
          .global_id());
  EXPECT_TRUE(cc_form_info.VerifyFieldsAreCorrectlyFilled(
      web_frames_manager(), filled_fields_so_far));
}

// Tests that sensitive information isn't filled on the main origin when the
// trigger is from another origin.
//
// Representation of the tested xframe form structure with the expected outcome
// in [] next to each input field and the trigger field indicated with <--:
// =======================================
// Main Frame
//   Iframe (origin1):
//     Input: name [filled] <--
//   Input: cc number [not filled]
//   Iframe (origin1):
//     Input: exp date [filled]
//   Iframe (main origin):
//     Input: cvc [not filled]
// =======================================
TEST_F(AutofillAcrossIframesFillSecurityTest,
       XoriginTrigger_SensitiveFieldsOnMainOrigin) {
  EmbeddedTestServer test_server1;

  TestCreditCardForm cc_form_info = GetTestCreditCardForm();

  // Serve the cc number and exp fields from the other origin.
  ServeCrossOriginDocument("cf1", cc_form_info.name_field.ToHtmlForm(),
                           &test_server1);
  ServeCrossOriginDocument("cf2", cc_form_info.exp_field.ToHtmlForm(),
                           &test_server1);
  ASSERT_TRUE(test_server1.Start());

  // Add iframe on another origin holding the name, a
  // non-sensitive field.
  AddCrossOriginIframe("cf1", &test_server1);
  // Add an input holding the credit card number on the main frame, a sensitive
  // field.
  AddInput(cc_form_info.cc_number_field);
  // Add iframe on another origin holding the expiry date, a
  // non-sensitive field.
  AddCrossOriginIframe("cf2", &test_server1);
  // Add iframe on the main frame origin holding the cvc field, a sensitive
  // field.
  AddIframe("cf3", cc_form_info.cvc_field.ToHtmlForm());

  // Start serving main frame content.
  StartTestServerAndLoad();

  // Wait on the browser form to be fully constructed from both the frame on the
  // main origin and the other cross origin frames, totalling 4 fields.
  const auto [browser_form, res] =
      WaitForCompleteBrowserForm(/*child_frames_count=*/3, /*fields_count=*/4);
  ASSERT_TRUE(res);

  std::vector<FormFieldData> fields = browser_form.fields();

  // Fill and verify that all the fields are filled.
  FillAndVerify(cc_form_info, browser_form, cc_form_info.name_field,
                {cc_form_info.name_field, cc_form_info.exp_field});
}

// Tests that sensitive information can be filled on the main origin when the
// trigger is also on the main origin. Fields on other origins shouldn't be
// filled regardless of their sensitivity.
//
// Representation of the tested xframe form structure with the expected outcome
// in [] next to each input field and the trigger field indicated with <--:
// =======================================
// Main Frame
//   Iframe (origin1):
//     Input: name [not filled]
//   Input: cc number [filled] <--
//   Iframe (origin1):
//     Input: exp date [not filled]
//   Iframe (main origin):
//     Input: cvc [filled]
// =======================================
TEST_F(AutofillAcrossIframesFillSecurityTest, MainOriginTrigger) {
  EmbeddedTestServer test_server1;

  TestCreditCardForm cc_form_info = GetTestCreditCardForm();

  // Serve the cc number and exp fields from the other origin.
  ServeCrossOriginDocument("cf1", cc_form_info.name_field.ToHtmlForm(),
                           &test_server1);
  ServeCrossOriginDocument("cf2", cc_form_info.exp_field.ToHtmlForm(),
                           &test_server1);
  ASSERT_TRUE(test_server1.Start());

  // Add iframe on another origin holding the name, a
  // non-sensitive field.
  AddCrossOriginIframe("cf1", &test_server1);

  // Add an input holding the credit card number on the main frame, a sensitive
  // field.
  AddInput(cc_form_info.cc_number_field);

  // Add iframe on another origin holding the expiry date, a
  // non-sensitive field.
  AddCrossOriginIframe("cf2", &test_server1);

  // Add iframe on the main frame origin holding the cvc field, a sensitive
  // field.
  AddIframe("cf3", cc_form_info.cvc_field.ToHtmlForm());

  // Start serving main frame content.
  StartTestServerAndLoad();

  // Wait on the browser form to be fully constructed from both the frame on the
  // main origin and the other cross origin frames, totalling 4 fields.
  const auto [browser_form, res] =
      WaitForCompleteBrowserForm(/*child_frames_count=*/3, /*fields_count=*/4);
  ASSERT_TRUE(res);

  std::vector<FormFieldData> fields = browser_form.fields();

  // Fill and verify that all the fields are filled.
  FillAndVerify(cc_form_info, browser_form, cc_form_info.cc_number_field,
                {cc_form_info.cc_number_field, cc_form_info.cvc_field});
}

}  // namespace autofill
