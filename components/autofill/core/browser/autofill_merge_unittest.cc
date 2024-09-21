// Copyright 2013 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <stddef.h>

#include <map>
#include <memory>
#include <vector>

#include "base/feature_list.h"
#include "base/files/file_enumerator.h"
#include "base/files/file_path.h"
#include "base/no_destructor.h"
#include "base/path_service.h"
#include "base/ranges/algorithm.h"
#include "base/strings/string_split.h"
#include "base/strings/string_util.h"
#include "base/strings/utf_string_conversions.h"
#include "base/test/scoped_feature_list.h"
#include "base/test/task_environment.h"
#include "build/build_config.h"
#include "components/autofill/core/browser/address_data_manager.h"
#include "components/autofill/core/browser/address_data_manager_test_api.h"
#include "components/autofill/core/browser/autofill_test_utils.h"
#include "components/autofill/core/browser/autofill_type.h"
#include "components/autofill/core/browser/form_data_importer.h"
#include "components/autofill/core/browser/form_data_importer_test_api.h"
#include "components/autofill/core/browser/form_structure.h"
#include "components/autofill/core/browser/test_autofill_client.h"
#include "components/autofill/core/browser/test_personal_data_manager.h"
#include "components/autofill/core/common/autofill_features.h"
#include "components/autofill/core/common/form_data.h"
#include "components/autofill/core/common/form_data_test_api.h"
#include "testing/data_driven_testing/data_driven_test.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "url/gurl.h"

#if BUILDFLAG(IS_APPLE)
#include "base/apple/foundation_util.h"
#endif

namespace autofill {
namespace {

const base::FilePath::CharType kFeatureName[] = FILE_PATH_LITERAL("autofill");
const base::FilePath::CharType kTestName[] = FILE_PATH_LITERAL("merge");
const base::FilePath::CharType kFileNamePattern[] = FILE_PATH_LITERAL("*.in");

const char kFieldSeparator[] = ":";
const char kProfileSeparator[] = "---";

const FieldType kProfileFieldTypes[] = {NAME_FIRST,
                                        NAME_MIDDLE,
                                        NAME_LAST,
                                        NAME_FULL,
                                        EMAIL_ADDRESS,
                                        COMPANY_NAME,
                                        ADDRESS_HOME_STREET_ADDRESS,
                                        ADDRESS_HOME_CITY,
                                        ADDRESS_HOME_STATE,
                                        ADDRESS_HOME_ZIP,
                                        ADDRESS_HOME_COUNTRY,
                                        PHONE_HOME_WHOLE_NUMBER};

const base::FilePath& GetTestDataDir() {
  static base::NoDestructor<base::FilePath> dir([]() {
    base::FilePath dir;
    base::PathService::Get(base::DIR_SRC_TEST_DATA_ROOT, &dir);
    return dir.AppendASCII("components")
        .AppendASCII("test")
        .AppendASCII("data");
  }());
  return *dir;
}

const std::vector<base::FilePath> GetTestFiles() {
  base::FilePath dir = GetTestDataDir();

  dir = dir.AppendASCII("autofill").Append(kTestName).AppendASCII("input");
  base::FileEnumerator input_files(dir, false, base::FileEnumerator::FILES,
                                   kFileNamePattern);
  std::vector<base::FilePath> files;
  for (base::FilePath input_file = input_files.Next(); !input_file.empty();
       input_file = input_files.Next()) {
    files.push_back(input_file);
  }
  std::sort(files.begin(), files.end());

#if BUILDFLAG(IS_APPLE)
  base::apple::ClearAmIBundledCache();
#endif  // BUILDFLAG(IS_APPLE)

  return files;
}

// Fakes that a `form` has been seen (without its field value) and parsed and
// then values have been entered. Returns the resulting FormStructure.
std::unique_ptr<FormStructure> ConstructFormStructureFromFormData(
    const FormData& form) {
  auto cached_form_structure =
      std::make_unique<FormStructure>(test::WithoutValues(form));
  cached_form_structure->DetermineHeuristicTypes(GeoIpCountryCode(""), nullptr,
                                                 nullptr);

  auto form_structure = std::make_unique<FormStructure>(form);
  form_structure->RetrieveFromCache(
      *cached_form_structure,
      FormStructure::RetrieveFromCacheReason::kFormImport);
  return form_structure;
}

// Serializes the |profiles| into a string.
std::string SerializeProfiles(
    const std::vector<const AutofillProfile*>& profiles) {
  std::string result;
  for (const AutofillProfile* profile : profiles) {
    result += kProfileSeparator;
    result += "\n";
    for (const FieldType& type : kProfileFieldTypes) {
      std::u16string value = profile->GetRawInfo(type);
      result += FieldTypeToStringView(type);
      result += kFieldSeparator;
      if (!value.empty()) {
        base::ReplaceFirstSubstringAfterOffset(&value, 0, u"\\n", u"\n");
        result += " ";
        result += base::UTF16ToUTF8(value);
      }
      result += "\n";
    }
  }

  return result;
}

// A data-driven test for verifying merging of Autofill profiles. Each input is
// a structured dump of a set of implicitly detected autofill profiles. The
// corresponding output file is a dump of the saved profiles that result from
// importing the input profiles. The output file format is identical to the
// input format.
class AutofillMergeTest : public testing::DataDrivenTest,
                          public testing::TestWithParam<base::FilePath> {
 public:
  AutofillMergeTest(const AutofillMergeTest&) = delete;
  AutofillMergeTest& operator=(const AutofillMergeTest&) = delete;

 protected:
  AutofillMergeTest();
  ~AutofillMergeTest() override;

  // testing::Test:
  void SetUp() override;

  // DataDrivenTest:
  void GenerateResults(const std::string& input, std::string* output) override;

  // Deserializes a set of Autofill profiles from |profiles|, imports each
  // sequentially, and fills |merged_profiles| with the serialized result.
  void MergeProfiles(const std::string& profiles, std::string* merged_profiles);

  TestAddressDataManager& test_address_data_manager() {
    return autofill_client_.GetPersonalDataManager()
        ->test_address_data_manager();
  }

  base::test::TaskEnvironment task_environment_;
  base::test::ScopedFeatureList scoped_feature_list_;
  TestAutofillClient autofill_client_;
  std::unique_ptr<FormDataImporter> form_data_importer_;
};

AutofillMergeTest::AutofillMergeTest()
    : testing::DataDrivenTest(GetTestDataDir(), kFeatureName, kTestName) {}

AutofillMergeTest::~AutofillMergeTest() = default;

void AutofillMergeTest::SetUp() {
  test_api(test_address_data_manager()).set_auto_accept_address_imports(true);
  form_data_importer_ = std::make_unique<FormDataImporter>(
      &autofill_client_, /*history_service=*/nullptr, "en");
  scoped_feature_list_.InitWithFeatures(
      {features::kAutofillConsiderPhoneNumberSeparatorsValidLabels,
       features::kAutofillEnableSupportForPhoneNumberTrunkTypes,
       features::kAutofillInferCountryCallingCode},
      /*disabled_features=*/{});
}

void AutofillMergeTest::GenerateResults(const std::string& input,
                                        std::string* output) {
  MergeProfiles(input, output);
}

void AutofillMergeTest::MergeProfiles(const std::string& profiles,
                                      std::string* merged_profiles) {
  // Start with no saved profiles.
  test_address_data_manager().ClearProfiles();

  // Create a test form.
  FormData form;
  form.set_name(u"MyTestForm");
  form.set_url(GURL("https://www.example.com/origin.html"));
  form.set_action(GURL("https://www.example.com/action.html"));

  // Parse the input line by line.
  std::vector<std::string> lines = base::SplitString(
      profiles, "\n", base::KEEP_WHITESPACE, base::SPLIT_WANT_NONEMPTY);
  for (size_t i = 0; i < lines.size(); ++i) {
    std::string line = lines[i];
    if (line != kProfileSeparator) {
      // Add a field to the current profile.
      size_t separator_pos = line.find(kFieldSeparator);
      ASSERT_NE(std::string::npos, separator_pos)
          << "Wrong format for separator on line " << i;
      std::u16string field_type =
          base::UTF8ToUTF16(line.substr(0, separator_pos));
      do {
        ++separator_pos;
      } while (separator_pos < line.size() && line[separator_pos] == ' ');
      std::u16string value = base::UTF8ToUTF16(line.substr(separator_pos));
      base::ReplaceFirstSubstringAfterOffset(&value, 0, u"\\n", u"\n");

      FormFieldData field;
      field.set_label(field_type);
      field.set_name(field_type);
      field.set_value(value);
      field.set_form_control_type(FormControlType::kInputText);
      field.set_is_focusable(true);
      test_api(form).Append(field);
    }

    // The first line is always a profile separator, and the last profile is not
    // followed by an explicit separator.
    if ((i > 0 && line == kProfileSeparator) || i == lines.size() - 1) {
      // Reached the end of a profile.  Try to import it.
      std::unique_ptr<FormStructure> form_structure =
          ConstructFormStructureFromFormData(form);
      for (size_t j = 0; j < form_structure->field_count(); ++j) {
        // Set the heuristic type for each field, which is currently serialized
        // into the field's name.
        AutofillField* field =
            const_cast<AutofillField*>(form_structure->field(j));
        FieldType type = TypeNameToFieldType(base::UTF16ToUTF8(field->name()));
        field->set_heuristic_type(GetActiveHeuristicSource(), type);
      }

      // Extract the profile.
      auto extracted_data =
          test_api(*form_data_importer_)
              .ExtractFormData(*form_structure,
                               /*profile_autofill_enabled=*/true,
                               /*payment_methods_autofill_enabled=*/true);
      test_api(*form_data_importer_)
          .ProcessAddressProfileImportCandidates(
              extracted_data.address_profile_import_candidates,
              /*allow_prompt=*/true);
      EXPECT_FALSE(extracted_data.extracted_credit_card);

      // Clear the |form| to start a new profile.
      form.set_fields({});
    }
  }

  std::vector<const AutofillProfile*> imported_profiles =
      test_address_data_manager().GetProfiles();
  // To ensure a consistent order with the output files, sort the profiles by
  // modification date. This corresponds to the order in which the profiles
  // were imported (or updated).
  base::ranges::sort(imported_profiles,
                     [](const AutofillProfile* a, const AutofillProfile* b) {
                       return a->modification_date() < b->modification_date();
                     });
  *merged_profiles = SerializeProfiles(imported_profiles);
}

TEST_P(AutofillMergeTest, DataDrivenMergeProfiles) {
  const bool kIsExpectedToPass = true;
  RunOneDataDrivenTest(GetParam(), GetOutputDirectory(), kIsExpectedToPass);
}

INSTANTIATE_TEST_SUITE_P(All,
                         AutofillMergeTest,
                         testing::ValuesIn(GetTestFiles()));

}  // namespace
}  // namespace autofill
