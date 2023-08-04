// Copyright 2014 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/password_manager/core/browser/browser_save_password_progress_logger.h"
#include "components/autofill/core/browser/autofill_test_utils.h"

#include "base/containers/flat_map.h"
#include "base/strings/utf_string_conversions.h"
#include "components/autofill/core/browser/autofill_type.h"
#include "components/autofill/core/browser/field_types.h"
#include "components/autofill/core/browser/form_structure.h"
#include "components/autofill/core/browser/logging/stub_log_manager.h"
#include "components/autofill/core/browser/proto/server.pb.h"
#include "components/autofill/core/common/save_password_progress_logger.h"
#include "components/autofill/core/common/unique_ids.h"
#include "components/password_manager/core/browser/password_form.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

using autofill::AutofillType;
using autofill::FieldGlobalId;
using autofill::ServerFieldType;
using autofill::StubLogManager;
using autofill::test::CreateFieldPrediction;
using base::UTF8ToUTF16;
using Logger = autofill::SavePasswordProgressLogger;

namespace password_manager {

namespace {

const char kTestString[] = "Message";  // Corresponds to STRING_MESSAGE.

class TestLogger : public BrowserSavePasswordProgressLogger {
 public:
  explicit TestLogger(autofill::LogManager* log_manager)
      : BrowserSavePasswordProgressLogger(log_manager) {}

  bool LogsContainSubstring(const std::string& substring) {
    return accumulated_log_.find(substring) != std::string::npos;
  }

  std::string accumulated_log() { return accumulated_log_; }

  void SendLog(const std::string& log) override {
    accumulated_log_.append(log);
  }

 private:
  std::string accumulated_log_;
};

class BrowserSavePasswordProgressLoggerTest : public testing::Test {
 public:
  BrowserSavePasswordProgressLoggerTest() {
    form_.url = GURL("http://myform.com/form.html");
    form_.action = GURL("http://m.myform.com/submit.html");
    form_.name = u"form_name";

    // Add a password field.
    autofill::FormFieldData field;
    field.name = u"password";
    field.form_control_type = "password";
    field.is_focusable = true;
    field.autocomplete_attribute = "new-password";
    field.unique_renderer_id = autofill::FieldRendererId(10);
    form_.fields.push_back(field);

    // Add a text field.
    field.name = u"email";
    field.form_control_type = "text";
    field.is_focusable = false;
    field.unique_renderer_id = autofill::FieldRendererId(42);
    field.value = u"a@example.com";
    field.autocomplete_attribute.clear();
    form_.fields.push_back(field);
  }

 protected:
  autofill::FormData form_;
};

}  // namespace

TEST_F(BrowserSavePasswordProgressLoggerTest, LogFormData) {
  StubLogManager log_manager;
  TestLogger logger(&log_manager);
  logger.LogFormData(Logger::STRING_FORM_PARSING_INPUT, form_);
  SCOPED_TRACE(testing::Message()
               << "Log string = [" << logger.accumulated_log() << "]");
  EXPECT_TRUE(logger.LogsContainSubstring("Origin: http://myform.com"));
  EXPECT_TRUE(logger.LogsContainSubstring("Action: http://m.myform.com"));
  EXPECT_TRUE(logger.LogsContainSubstring("Form name: form_name"));
  EXPECT_TRUE(logger.LogsContainSubstring("Form with form tag: true"));
  EXPECT_TRUE(logger.LogsContainSubstring("Form fields:"));
  EXPECT_TRUE(logger.LogsContainSubstring(
      "password: signature=2051817934, type=password, renderer_id=10, "
      "visible, empty, autocomplete=new-password"));
  EXPECT_TRUE(
      logger.LogsContainSubstring("email: signature=420638584, type=text, "
                                  "renderer_id=42, invisible, non-empty"));
}

TEST_F(BrowserSavePasswordProgressLoggerTest,
       LogFormDataWithServerPredictions) {
  StubLogManager log_manager;
  TestLogger logger(&log_manager);
  AutofillType::ServerPrediction password_prediction;
  password_prediction.server_predictions = {
      CreateFieldPrediction(ServerFieldType::NEW_PASSWORD)};
  base::flat_map<FieldGlobalId, AutofillType::ServerPrediction> predictions = {
      {form_.fields[0].global_id(), std::move(password_prediction)}};
  logger.LogFormDataWithServerPredictions(Logger::STRING_SERVER_PREDICTIONS,
                                          form_, predictions);

  SCOPED_TRACE(testing::Message()
               << "Log string = [" << logger.accumulated_log() << "]");
  EXPECT_TRUE(logger.LogsContainSubstring(
      "Signature of form: 3370253896397449141 - 503"));
  EXPECT_TRUE(logger.LogsContainSubstring("Origin: http://myform.com"));
  EXPECT_TRUE(logger.LogsContainSubstring("Action: http://m.myform.com"));
  EXPECT_TRUE(logger.LogsContainSubstring("Form fields:"));
  EXPECT_TRUE(logger.LogsContainSubstring(
      "password: signature=2051817934, type=password, renderer_id=10, "
      "visible, empty, autocomplete=new-password, Server Type= NEW_PASSWORD, "
      "All Server Predictions= [NEW_PASSWORD]"));
  EXPECT_TRUE(logger.LogsContainSubstring(
      "email: signature=420638584, type=text, renderer_id=42"));
}

TEST(SavePasswordProgressLoggerTest, LogPasswordForm) {
  StubLogManager log_manager;
  TestLogger logger(&log_manager);
  PasswordForm form;
  form.action = GURL("http://example.org/verysecret?verysecret");
  form.password_element = u"pwdelement";
  form.password_value = u"verysecret";
  form.username_value = u"verysecret";
  logger.LogPasswordForm(Logger::STRING_MESSAGE, form);
  SCOPED_TRACE(testing::Message()
               << "Log string = [" << logger.accumulated_log() << "]");
  EXPECT_TRUE(logger.LogsContainSubstring(kTestString));
  EXPECT_TRUE(logger.LogsContainSubstring("pwdelement"));
  EXPECT_TRUE(logger.LogsContainSubstring("http://example.org"));
  EXPECT_FALSE(logger.LogsContainSubstring("verysecret"));
}

TEST(SavePasswordProgressLoggerTest, LogPasswordFormElementID) {
  // Test filtering element IDs.
  StubLogManager log_manager;
  TestLogger logger(&log_manager);
  PasswordForm form;
  const std::string kHTMLInside("Username <script> element");
  const std::string kHTMLInsideExpected("Username__script__element");
  const std::string kIPAddressInside("y128.0.0.1Y");
  const std::string kIPAddressInsideExpected("y128_0_0_1Y");
  const std::string kSpecialCharsInside("X@#a$%B&*c()D;:e+!x");
  const std::string kSpecialCharsInsideExpected("X__a__B__c__D__e__x");
  form.username_element = UTF8ToUTF16(kHTMLInside);
  form.password_element = UTF8ToUTF16(kIPAddressInside);
  form.new_password_element = UTF8ToUTF16(kSpecialCharsInside);
  logger.LogPasswordForm(Logger::STRING_MESSAGE, form);
  SCOPED_TRACE(testing::Message()
               << "Log string = [" << logger.accumulated_log() << "]");
  EXPECT_TRUE(logger.LogsContainSubstring(kTestString));
  EXPECT_FALSE(logger.LogsContainSubstring(kHTMLInside));
  EXPECT_TRUE(logger.LogsContainSubstring(kHTMLInsideExpected));
  EXPECT_FALSE(logger.LogsContainSubstring(kIPAddressInside));
  EXPECT_TRUE(logger.LogsContainSubstring(kIPAddressInsideExpected));
  EXPECT_FALSE(logger.LogsContainSubstring(kSpecialCharsInside));
  EXPECT_TRUE(logger.LogsContainSubstring(kSpecialCharsInsideExpected));
}

}  // namespace password_manager
