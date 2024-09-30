// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <algorithm>
#include <memory>
#include <optional>
#include <string>
#include <vector>

#include "base/base_paths.h"
#include "base/files/file_enumerator.h"
#include "base/files/file_path.h"
#include "base/files/file_util.h"
#include "base/json/json_reader.h"
#include "base/json/json_writer.h"
#include "base/memory/scoped_refptr.h"
#include "base/path_service.h"
#include "base/strings/string_split.h"
#include "base/strings/utf_string_conversions.h"
#include "base/values.h"
#include "build/build_config.h"
#include "components/autofill/content/renderer/form_autofill_util.h"
#include "components/autofill/core/common/field_data_manager.h"
#include "components/autofill/core/common/form_data.h"
#include "components/autofill/core/common/form_field_data.h"
#include "content/public/test/render_view_test.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/blink/public/web/web_document.h"
#include "third_party/blink/public/web/web_form_element.h"
#include "third_party/blink/public/web/web_local_frame.h"

#if BUILDFLAG(IS_MAC)
#include "base/apple/foundation_util.h"
#endif

namespace autofill {

namespace {

// The `GetTestDataDir()` contains and DOMs in *.html files and the expected
// labels that Autofill is supposed to find in *.json files of the same name.
base::FilePath GetTestDataDir() {
  base::FilePath dir;
  base::PathService::Get(base::DIR_SRC_TEST_DATA_ROOT, &dir);
  return dir.AppendASCII("components")
      .AppendASCII("test")
      .AppendASCII("data")
      .AppendASCII("autofill")
      .AppendASCII("label-doms");
}

struct TestCase {
  // Path to an HTML file containing a DOM.
  base::FilePath dom_path;
  // Path to a JSON file containing a single list with the expected label of
  // every field in the DOM (the test setup ensures that extraction order is
  // consistent across runs).
  base::FilePath expected_output_path;
};

// Returns all tests found in `GetTestDataDir()` in consistent order.
std::vector<TestCase> GetTests() {
  base::FileEnumerator file_iterator(GetTestDataDir(),
                                     /*recursive=*/true,
                                     base::FileEnumerator::FILES);
  std::vector<TestCase> tests;
  for (base::FilePath file = file_iterator.Next(); !file.empty();
       file = file_iterator.Next()) {
    if (!file.MatchesExtension(FILE_PATH_LITERAL(".html"))) {
      continue;
    }
    tests.push_back({.dom_path = file,
                     .expected_output_path =
                         file.ReplaceExtension(FILE_PATH_LITERAL(".json"))});
  }
  std::ranges::sort(tests, [](const TestCase& a, const TestCase& b) {
    return a.dom_path < b.dom_path;
  });
#if BUILDFLAG(IS_MAC)
  base::apple::ClearAmIBundledCache();
#endif
  return tests;
}

std::string LabelSourceToString(FormFieldData::LabelSource source) {
  switch (source) {
    case FormFieldData::LabelSource::kUnknown:
      return "unknown";
    case FormFieldData::LabelSource::kLabelTag:
      return "labelTag";
    case FormFieldData::LabelSource::kPTag:
      return "pTag";
    case FormFieldData::LabelSource::kDivTable:
      return "divTable";
    case FormFieldData::LabelSource::kTdTag:
      return "tdTag";
    case FormFieldData::LabelSource::kDdTag:
      return "ddTag";
    case FormFieldData::LabelSource::kLiTag:
      return "liTag";
    case FormFieldData::LabelSource::kPlaceHolder:
      return "placeholder";
    case FormFieldData::LabelSource::kAriaLabel:
      return "ariaLabel";
    case FormFieldData::LabelSource::kCombined:
      return "combined";
    case FormFieldData::LabelSource::kValue:
      return "value";
    case FormFieldData::LabelSource::kForId:
      return "forId";
    case FormFieldData::LabelSource::kForName:
      return "forName";
    case FormFieldData::LabelSource::kForShadowHostId:
      return "forShadowHostId";
    case FormFieldData::LabelSource::kForShadowHostName:
      return "forShadowHostName";
    case FormFieldData::LabelSource::kOverlayingLabel:
      return "overlayingLabel";
  }
}

class DomLabelTest : public content::RenderViewTest,
                     public testing::WithParamInterface<TestCase> {
 public:
  void SetUp() override {
    content::RenderViewTest::SetUp();
    // Fail all requests to external resources (e.g. images).
    CreateFakeURLLoaderFactory();
  }

  // Returns all forms found on the page in consistent order.
  std::vector<FormData> ExtractFormDatas() {
    blink::WebDocument document = GetMainFrame()->GetDocument();
    // `GetTopLevelForms()` returns forms in DOM order.
    blink::WebVector<blink::WebFormElement> form_elements =
        document.GetTopLevelForms();
    // Add a null WebFormElement to extract unowned fields into a separate form.
    form_elements.emplace_back();
    std::vector<FormData> result;
    for (const blink::WebFormElement& form_element : form_elements) {
      // Forms might be too large for Autofill.
      if (std::optional<FormData> form = form_util::ExtractFormData(
              document, form_element, *field_data_manager_,
              CallTimerState{
                  .call_site = CallTimerState::CallSite::kExtractForm,
                  .last_autofill_agent_reset = {},
                  .last_dom_content_loaded = {},
              })) {
        result.push_back(form.value());
      }
    }
    return result;
  }

 private:
  scoped_refptr<FieldDataManager> field_data_manager_ =
      base::MakeRefCounted<FieldDataManager>();
};

TEST_P(DomLabelTest, DataDrivenLabels) {
  const TestCase& test = GetParam();
  std::string dom;
  ASSERT_TRUE(base::ReadFileToString(test.dom_path, &dom));
  LoadHTML(dom);

  // Aggregate the labels + metadata of all form fields.
  base::Value::List field_infos;
  for (const FormData& form : ExtractFormDatas()) {
    for (const FormFieldData& field : form.fields()) {
      base::Value::Dict field_info;
      field_info.Set("name", field.name());
      field_info.Set("label", field.label());
      field_info.Set("heuristic", LabelSourceToString(field.label_source()));
      field_infos.Append(std::move(field_info));
    }
  }

  // If no expected output exists, the `field_infos` become the expected output.
  if (!base::PathExists(test.expected_output_path)) {
    std::optional<std::string> output =
        base::WriteJsonWithOptions(field_infos, base::OPTIONS_PRETTY_PRINT);
    ASSERT_TRUE(output);
    ASSERT_TRUE(base::WriteFile(test.expected_output_path, *output));
    return;
  }

  // Check if the expected output matches `field_infos`.
  std::string expected_output_content;
  ASSERT_TRUE(base::ReadFileToString(test.expected_output_path,
                                     &expected_output_content));
  std::optional<base::Value> expected_output_json =
      base::JSONReader::Read(expected_output_content);
  ASSERT_TRUE(expected_output_json && expected_output_json->is_list());
  const base::Value::List& expected_field_infos =
      expected_output_json->GetList();
  ASSERT_EQ(field_infos.size(), expected_field_infos.size());
  for (size_t i = 0; i < field_infos.size(); i++) {
    EXPECT_EQ(field_infos[i], expected_field_infos[i]);
  }
}

std::string GenerateTestName(const testing::TestParamInfo<TestCase>& info) {
  std::string name =
      info.param.dom_path.BaseName().RemoveExtension().MaybeAsASCII();
  std::ranges::replace_if(name, [](char c) { return !std::isalnum(c); }, '_');
  return name;
}

INSTANTIATE_TEST_SUITE_P(,
                         DomLabelTest,
                         testing::ValuesIn(GetTests()),
                         GenerateTestName);

}  // namespace

}  // namespace autofill
