// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/autofill/content/renderer/page_passwords_analyser.h"

#include "chrome/test/base/chrome_render_view_test.h"
#include "components/autofill/content/renderer/page_form_analyser_logger.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/blink/public/web/web_document.h"
#include "third_party/blink/public/web/web_element_collection.h"
#include "third_party/blink/public/web/web_form_element.h"

namespace autofill {
namespace {

class MockPageFormAnalyserLogger : public PageFormAnalyserLogger {
 public:
  MockPageFormAnalyserLogger() : PageFormAnalyserLogger(nullptr) {}
  virtual ~MockPageFormAnalyserLogger() {}

  void Send(std::string message,
            ConsoleLevel level,
            blink::WebNode node) override {
    Send(std::move(message), level,
         std::vector<blink::WebNode>{std::move(node)});
  }

  MOCK_METHOD3(Send,
               void(std::string message,
                    ConsoleLevel level,
                    std::vector<blink::WebNode> nodes));

  MOCK_METHOD0(Flush, void());
};

const char kExpectedDocumentationLink[] = " (More info: https://goo.gl/9p2vKq)";

const char kPasswordFieldNotInForm[] =
    "<input type='password' autocomplete='new-password'>";

const char kPasswordFormWithoutUsernameField[] =
    "<form>"
    "   <input type='password' autocomplete='new-password'>"
    "</form>";

const char kElementsWithDuplicateIds[] =
    "<input id='duplicate'>"
    "<input id='duplicate'>";

const char kPasswordFormTooComplex[] =
    "<form>"
    "   <input type='text' autocomplete='username'>"
    "   <input type='password' autocomplete='current-password'>"
    "   <input type='text' autocomplete='username'>"
    "   <input type='password' autocomplete='current-password'>"
    "   <input type='password' autocomplete='new-password'>"
    "   <input type='password' autocomplete='new-password'>"
    "</form>";

const char kInferredPasswordAutocompleteAttributes[] =
    // Login form.
    "<form>"
    "   <input type='text'>"
    "   <input type='password'>"
    "</form>"
    // Registration form.
    "<form>"
    "   <input type='text'>"
    "   <input type='password'>"
    "   <input type='password'>"
    "</form>"
    // Change password form.
    "<form>"
    "   <input type='text'>"
    "   <input type='password'>"
    "   <input type='password'>"
    "   <input type='password'>"
    "</form>";

const char kInferredUsernameAutocompleteAttributes[] =
    // Login form.
    "<form>"
    "   <input type='text'>"
    "   <input type='password' autocomplete='current-password'>"
    "</form>"
    // Registration form.
    "<form>"
    "   <input type='text'>"
    "   <input type='password' autocomplete='new-password'>"
    "   <input type='password' autocomplete='new-password'>"
    "</form>"
    // Change password form with username.
    "<form>"
    "   <input type='text'>"
    "   <input type='password' autocomplete='current-password'>"
    "   <input type='password' autocomplete='new-password'>"
    "   <input type='password' autocomplete='new-password'>"
    "</form>";

const char kPasswordFieldsWithAndWithoutAutocomplete[] =
    "<form>"
    "   <input type='password'>"
    "   <input type='text'>"
    "   <input type='password' autocomplete='current-password'>"
    "</form>";

const std::string AutocompleteSuggestionString(const std::string& suggestion) {
  return "Input elements should have autocomplete "
         "attributes (suggested: \"" +
         suggestion + "\"):";
}

class PagePasswordsAnalyserTest : public ChromeRenderViewTest {
 public:
  PagePasswordsAnalyserTest(const PagePasswordsAnalyserTest&) = delete;
  PagePasswordsAnalyserTest& operator=(const PagePasswordsAnalyserTest&) =
      delete;

 protected:
  PagePasswordsAnalyserTest()
      : mock_logger_(new MockPageFormAnalyserLogger()) {}

  void TearDown() override {
    elements_.clear();
    mock_logger_.reset();
    page_passwords_analyser.Reset();
    ChromeRenderViewTest::TearDown();
  }

  void LoadTestCase(const char* html) {
    elements_.clear();
    LoadHTML(html);
    blink::WebLocalFrame* frame = GetMainFrame();
    blink::WebElementCollection collection = frame->GetDocument().All();
    for (blink::WebElement element = collection.FirstItem(); element;
         element = collection.NextItem()) {
      elements_.push_back(element);
    }
    // Remove the <html>, <head> and <body> elements.
    elements_.erase(elements_.begin(), elements_.begin() + 3);
  }

  void Expect(const std::string& message,
              const ConsoleLevel level,
              const std::vector<size_t>& element_indices) {
    std::vector<blink::WebNode> nodes;
    std::string documented = message + kExpectedDocumentationLink;
    for (size_t index : element_indices)
      nodes.push_back(elements_[index]);
    EXPECT_CALL(*mock_logger_, Send(documented, level, nodes))
        .RetiresOnSaturation();
  }

  void RunTestCase() {
    EXPECT_CALL(*mock_logger_, Flush());
    page_passwords_analyser.AnalyseDocumentDOM(GetMainFrame(),
                                               mock_logger_.get());
  }

  PagePasswordsAnalyser page_passwords_analyser;

 private:
  std::vector<blink::WebElement> elements_;
  std::unique_ptr<MockPageFormAnalyserLogger> mock_logger_;
};

TEST_F(PagePasswordsAnalyserTest, PasswordFieldNotInForm) {
  LoadTestCase(kPasswordFieldNotInForm);

  Expect("Password field is not contained in a form:",
         PageFormAnalyserLogger::kVerbose, {0});

  RunTestCase();
}

TEST_F(PagePasswordsAnalyserTest, PasswordFormWithoutUsernameField) {
  LoadTestCase(kPasswordFormWithoutUsernameField);

  Expect(
      "Password forms should have (optionally hidden) "
      "username fields for accessibility:",
      PageFormAnalyserLogger::kVerbose, {0});

  RunTestCase();
}

TEST_F(PagePasswordsAnalyserTest, ElementsWithDuplicateIds) {
  LoadTestCase(kElementsWithDuplicateIds);

  Expect("Found 2 elements with non-unique id #duplicate:",
         PageFormAnalyserLogger::kWarning, {0, 1});

  RunTestCase();
}

TEST_F(PagePasswordsAnalyserTest, PasswordFormTooComplex) {
  LoadTestCase(kPasswordFormTooComplex);

  Expect(
      "Multiple forms should be contained in their own "
      "form elements; break up complex forms into ones that represent a "
      "single action:",
      PageFormAnalyserLogger::kVerbose, {0});

  RunTestCase();
}

TEST_F(PagePasswordsAnalyserTest, InferredPasswordAutocompleteAttributes) {
  LoadTestCase(kInferredPasswordAutocompleteAttributes);
  size_t element_index = 0;

  // Login form.
  element_index++;  // Skip form element.
  element_index++;  // Skip username field.
  Expect(AutocompleteSuggestionString("current-password"),
         PageFormAnalyserLogger::kVerbose, {element_index++});

  // Registration form.
  element_index++;  // Skip form element.
  element_index++;  // Skip username field.
  Expect(AutocompleteSuggestionString("new-password"),
         PageFormAnalyserLogger::kVerbose, {element_index++});
  Expect(AutocompleteSuggestionString("new-password"),
         PageFormAnalyserLogger::kVerbose, {element_index++});

  // Change password form.
  element_index++;  // Skip form element.
  element_index++;  // Skip username field.
  Expect(AutocompleteSuggestionString("current-password"),
         PageFormAnalyserLogger::kVerbose, {element_index++});
  Expect(AutocompleteSuggestionString("new-password"),
         PageFormAnalyserLogger::kVerbose, {element_index++});
  Expect(AutocompleteSuggestionString("new-password"),
         PageFormAnalyserLogger::kVerbose, {element_index++});

  RunTestCase();
}

TEST_F(PagePasswordsAnalyserTest, InferredUsernameAutocompleteAttributes) {
  LoadTestCase(kInferredUsernameAutocompleteAttributes);
  size_t element_index = 0;

  // Login form.
  element_index++;  // Skip form element.
  Expect(AutocompleteSuggestionString("username"),
         PageFormAnalyserLogger::kVerbose, {element_index++});
  element_index++;  // Skip already annotated password field.

  // Registration form.
  element_index++;  // Skip form element.
  Expect(AutocompleteSuggestionString("username"),
         PageFormAnalyserLogger::kVerbose, {element_index++});
  element_index++;  // Skip already annotated password field.
  element_index++;  // Skip already annotated password field.

  // Change password form with username.
  element_index++;  // Skip form element.
  Expect(AutocompleteSuggestionString("username"),
         PageFormAnalyserLogger::kVerbose, {element_index++});
  element_index++;  // Skip already annotated password field.
  element_index++;  // Skip already annotated password field.
  element_index++;  // Skip already annotated password field.

  RunTestCase();
}

TEST_F(PagePasswordsAnalyserTest, PasswordFieldWithAndWithoutAutocomplete) {
  LoadTestCase(kPasswordFieldsWithAndWithoutAutocomplete);
  Expect(
      "Multiple forms should be contained in their own "
      "form elements; break up complex forms into ones that represent a "
      "single action:",
      PageFormAnalyserLogger::kVerbose, {0});
  RunTestCase();
}

}  // namespace
}  // namespace autofill
