// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/autofill/content/renderer/html_based_username_detector.h"

#include <array>

#include "base/strings/stringprintf.h"
#include "components/autofill/content/renderer/form_autofill_util.h"
#include "components/autofill/core/common/field_data_manager.h"
#include "content/public/test/render_view_test.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/blink/public/web/web_document.h"
#include "third_party/blink/public/web/web_local_frame.h"

using blink::WebElement;
using blink::WebFormControlElement;
using blink::WebFormElement;
using blink::WebLocalFrame;
using blink::WebString;

namespace autofill {
namespace {

struct TextField {
  const char* name;
  const char* id;
  const char* value;
  const char* label;
};

constexpr char kTestForm[] = R"(
  <FORM name="Test" id="userform">
    <label for="%s">%s</label>
    <input type="text" name="%s" id="%s" value="%s" />

    <label for="%s">%s</label>
    <input type="text" name="%s" id="%s" value="%s" />

    <input type="password" id="password" value="v" />
    <input type="submit" value="submit" />
  </FORM>
)";

std::string GetFormHTML(const TextField& first_field,
                        const TextField& second_field) {
  return base::StringPrintf(
      kTestForm, first_field.id, first_field.label, first_field.name,
      first_field.id, first_field.value, second_field.id, second_field.label,
      second_field.name, second_field.id, second_field.value);
}

class HtmlBasedUsernameDetectorTest : public content::RenderViewTest {
 protected:
  struct TestCase {
    const TextField first_text_field_parameter;
    const TextField second_text_field_parameter;
    const WebString expected_username_id;
  };

  FormData LoadFormDataFromHtml(const std::string& html) {
    LoadHTML(html.data());
    return GetFormData(GetFormElement());
  }

  FormData GetFormData(const WebFormElement& form) {
    constexpr CallTimerState kCallTimerStateDummy = {
        .call_site = CallTimerState::CallSite::kUpdateFormCache,
        .last_autofill_agent_reset = {},
        .last_dom_content_loaded = {},
    };
    return *form_util::ExtractFormData(
        form.GetDocument(), form, *base::MakeRefCounted<FieldDataManager>(),
        kCallTimerStateDummy, /*extract_options=*/{});
  }

  FieldRendererId GetRendererIdFromWebElementId(const WebString& id) {
    const WebLocalFrame* frame = GetMainFrame();
    const WebElement& element = frame->GetDocument().GetElementById(id);
    EXPECT_TRUE(element);
    return form_util::GetFieldRendererId(element.To<blink::WebInputElement>());
  }

  WebFormElement GetFormElement() {
    const WebLocalFrame* frame = GetMainFrame();
    const blink::WebVector<WebFormElement>& forms =
        frame->GetDocument().GetTopLevelForms();
    EXPECT_EQ(1U, forms.size());
    EXPECT_TRUE(forms[0]);

    return forms[0];
  }

  std::vector<WebFormControlElement> GetFormControlElements() {
    const WebFormElement& form = GetFormElement();
    blink::WebVector<WebFormControlElement> control_elements =
        form.GetFormControlElements();
    return control_elements.ReleaseVector();
  }

  void PredictAndCheckUsernameId(const std::string& html,
                                 const WebString& expected_username_id) {
    const FormData& form_data = LoadFormDataFromHtml(html);

    // Get the expected renderer id from the expected username id.
    const FieldRendererId expected_renderer_id =
        GetRendererIdFromWebElementId(expected_username_id);

    // Run predictions and test the result.
    UsernameDetectorCache cache;
    const std::vector<FieldRendererId>& renderer_ids =
        GetPredictionsFieldBasedOnHtmlAttributes(form_data, &cache);

    ASSERT_EQ(1u, cache.size());
    ASSERT_FALSE(cache.begin()->second.empty());
    EXPECT_EQ(expected_renderer_id, cache.begin()->second[0]);
    ASSERT_FALSE(renderer_ids.empty());
    EXPECT_EQ(expected_renderer_id, renderer_ids[0]);
  }
};

TEST_F(HtmlBasedUsernameDetectorTest, DeveloperGroupAttributes) {
  // Each test case consists of a set of parameters to be plugged into
  // the TestCase struct, plus the corresponding expectations.  The test data
  // contains cases that are identified by HTML detector, and not by
  // base heuristic. Thus, username field does not necessarily have to
  // be right before password field.  These tests basically check
  // searching in developer group (i.e. name and id attribute,
  // concatenated, with "$" guard in between).
  const auto test_cases = std::to_array<TestCase>(
      {// There are both field name and id.
       TestCase{{"username", "x1d", "johnsmith"},
                {"email", "y1d", "js@google.com"},
                "x1d"},
       // there is no field id.
       TestCase{{"username", "x1d", "johnsmith"},
                {"email", "y1d", "js@google.com"},
                "x1d"},
       // Upper or mixed case shouldn't matter.
       TestCase{{"uSeRnAmE", "x1d", "johnsmith"},
                {"email", "y1d", "js@google.com"},
                "x1d"},
       // Check removal of special characters.
       TestCase{{"u1_s2-e3~r4/n5(a)6m#e", "x1d", "johnsmith"},
                {"email", "y1d", "js@google.com"},
                "x1d"},
       // Check guard between field name and field id.
       TestCase{{"us", "ername", "johnsmith"},
                {"email", "id", "js@google.com"},
                "id"},
       // Check removal of fields with latin negative words in developer group.
       TestCase{{"email", "x", "js@google.com"},
                {"fake_username", "y", "johnsmith"},
                "x"},
       TestCase{{"email", "mail", "js@google.com"},
                {"user_name", "fullname", "johnsmith"},
                "mail"},
       // Identify latin translations of "username".
       TestCase{{"benutzername", "x", "johnsmith"},
                {"email", "y", "js@google.com"},
                "x"},
       // Identify latin translations of "user".
       TestCase{{"utilizator", "x1d", "johnsmith"},
                {"email", "y1d", "js@google.com"},
                "x1d"},
       // Identify technical words.
       TestCase{{"loginid", "x1d", "johnsmith"},
                {"email", "y1d", "js@google.com"},
                "x1d"},
       // Identify weak words.
       TestCase{{"usrname", "x1d", "johnsmith"},
                {"email", "y1d", "js@google.com"},
                "y1d"},
       // If a word matches in maximum 2 fields, it is accepted.
       // First encounter is selected as username.
       TestCase{{"username", "x1d", "johnsmith"},
                {"repeat_username", "y1d", "johnsmith"},
                "x1d"},
       // A short word should be enclosed between delimiters. Otherwise, an
       // Occurrence doesn't count.
       TestCase{{"identity_name", "idn", "johnsmith"},
                {"id", "xid", "123"},
                "xid"}});

  for (size_t i = 0; i < std::size(test_cases); ++i) {
    SCOPED_TRACE(testing::Message() << "Iteration " << i);

    const std::string& form_html =
        GetFormHTML(test_cases[i].first_text_field_parameter,
                    test_cases[i].second_text_field_parameter);

    PredictAndCheckUsernameId(form_html, test_cases[i].expected_username_id);
  }
}

TEST_F(HtmlBasedUsernameDetectorTest, UserGroupAttributes) {
  // Each test case consists of a set of parameters to be plugged into
  // the TestCase struct, plus the corresponding expectations.  The test data
  // contains cases that are identified by HTML detector, and not by
  // base heuristic. Thus, username field does not necessarily have to
  // be right before password field.  These tests basically check
  // searching in user group
  const auto test_cases = std::to_array<TestCase>(
      {// Label information will decide username.
       TestCase{{"name1", "id1", "johnsmith", "Username:"},
                {"name2", "id2", "js@google.com", "Email:"},
                "id1"},
       // Placeholder information will decide username.
       TestCase{{"name1", "id1", "js@google.com", "Email:"},
                {"name2", "id2", "johnsmith", "Username:"},
                "id2"},
       // Check removal of special characters.
       TestCase{{"name1", "id1", "johnsmith", "U s er n a m e:"},
                {"name2", "id2", "js@google.com", "Email:"},
                "id1"},
       // Check removal of fields with latin negative words in user group.
       TestCase{{"name1", "id1", "johnsmith", "Username password:"},
                {"name2", "id2", "js@google.com", "Email:"},
                "id2"},
       // Check removal of fields with non-latin negative words in user group.
       TestCase{{"name1", "id1", "js@google.com", "Email:"},
                {"name2", "id2", "johnsmith", "የይለፍቃልየይለፍቃል:"},
                "id1"},
       // Identify latin translations of "username".
       TestCase{{"name1", "id1", "johnsmith", "Username:"},
                {"name2", "id2", "js@google.com", "Email:"},
                "id1"},
       // Identify non-latin translations of "username".
       TestCase{{"name1", "id1", "johnsmith", "用户名:"},
                {"name2", "id2", "js@google.com", "Email:"},
                "id1"},
       // Identify latin translations of "user".
       TestCase{{"name1", "id1", "johnsmith", "Wosuta:"},
                {"name2", "id2", "js@google.com", "Email:"},
                "id1"},
       // Identify non-latin translations of "user".
       TestCase{{"name1", "id1", "johnsmith", "истифода:"},
                {"name2", "id2", "js@google.com", "Email:"},
                "id1"},
       // Identify weak words.
       TestCase{{"name1", "id1", "johnsmith", "Insert your login details:"},
                {"name2", "id2", "js@google.com", "Insert your email:"},
                "id1"},
       // Check user group priority, compared to developer group.
       // User group should have higher priority than developer group.
       TestCase{{"email", "id1", "js@google.com", "Username:"},
                {"username", "id2", "johnsmith", "Email:"},
                "id1"},
       // Check treatment for short dictionary words. "uid" has higher priority,
       // but its occurrence is ignored because it is a part of another word.
       TestCase{{"name1", "noword", "johnsmith", "Insert your id:"},
                {"name2", "uidentical", "js@google.com", "Insert something:"},
                "noword"}});

  for (size_t i = 0; i < std::size(test_cases); ++i) {
    SCOPED_TRACE(testing::Message() << "Iteration " << i);

    const std::string& form_html =
        GetFormHTML(test_cases[i].first_text_field_parameter,
                    test_cases[i].second_text_field_parameter);

    PredictAndCheckUsernameId(form_html, test_cases[i].expected_username_id);
  }
}

TEST_F(HtmlBasedUsernameDetectorTest, SeveralDetections) {
  // If word matches in more than 2 fields, we don't match on it.
  // We search for match with another word.
  const std::string& test_form = R"(
    <form>
        <input type="text" name="address" id="xuser" value="addr" />
        <input type="text" name="loginid" id="yuser" value="johnsmith" />
        <input type="text" name="tel" id="zuser" value="sometel" />
        <input type="password" id="password" value="v" />
        <input type="submit" value="submit" />
    </form>
  )";
  PredictAndCheckUsernameId(test_form, "yuser");
}

TEST_F(HtmlBasedUsernameDetectorTest, HTMLDetectorCache) {
  const TextField text_fields[] = {
      {"unknown", "12345"},
      {"something", "smith"},
  };

  const std::string& form_html = GetFormHTML(text_fields[0], text_fields[1]);

  FormData form_data = LoadFormDataFromHtml(form_html);
  std::vector<WebFormControlElement> control_elements =
      GetFormControlElements();

  UsernameDetectorCache cache;
  std::vector<FieldRendererId> field_ids =
      GetPredictionsFieldBasedOnHtmlAttributes(form_data, &cache);

  // No signals from HTML attributes. The classifier found nothing and cached
  // it.
  ASSERT_EQ(1u, cache.size());
  EXPECT_TRUE(field_ids.empty());
  const WebFormElement& form = GetFormElement();
  EXPECT_EQ(form_util::GetFormRendererId(form), cache.begin()->first);
  EXPECT_TRUE(cache.begin()->second.empty());

  // Changing attributes would change the classifier's output. But the output
  // will be the same because it was cached in |username_detector_cache|.
  control_elements[0].SetAttribute("name", "id");
  form_data = GetFormData(GetFormElement());
  field_ids = GetPredictionsFieldBasedOnHtmlAttributes(form_data, &cache);
  ASSERT_EQ(1u, cache.size());
  EXPECT_TRUE(field_ids.empty());
  EXPECT_EQ(form_util::GetFormRendererId(form), cache.begin()->first);
  EXPECT_TRUE(cache.begin()->second.empty());

  // Clear the cache. The classifier will find username field and cache it.
  cache.clear();
  ASSERT_EQ(4u, control_elements.size());
  field_ids = GetPredictionsFieldBasedOnHtmlAttributes(form_data, &cache);
  ASSERT_EQ(1u, cache.size());
  EXPECT_EQ(1u, field_ids.size());
  EXPECT_EQ(form_util::GetFormRendererId(form), cache.begin()->first);
  ASSERT_EQ(1u, cache.begin()->second.size());
  EXPECT_EQ(form_util::GetFieldRendererId(control_elements[0]),
            cache.begin()->second[0]);

  // Change the attributes again ("username" is stronger signal than "id"),
  // but keep the cache. The classifier's output should be the same.
  control_elements[1].SetAttribute("name", "username");
  form_data = GetFormData(GetFormElement());
  field_ids = GetPredictionsFieldBasedOnHtmlAttributes(form_data, &cache);

  ASSERT_EQ(1u, cache.size());
  EXPECT_EQ(1u, field_ids.size());
  EXPECT_EQ(form_util::GetFormRendererId(form), cache.begin()->first);
  ASSERT_EQ(1u, cache.begin()->second.size());
  EXPECT_EQ(form_util::GetFieldRendererId(control_elements[0]),
            cache.begin()->second[0]);
}

}  // namespace
}  // namespace autofill
