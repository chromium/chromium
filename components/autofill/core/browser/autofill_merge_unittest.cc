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
#include "base/strings/string_split.h"
#include "base/strings/string_util.h"
#include "base/strings/utf_string_conversions.h"
#include "base/test/task_environment.h"
#include "build/build_config.h"
#include "components/autofill/core/browser/autofill_test_utils.h"
#include "components/autofill/core/browser/autofill_type.h"
#include "components/autofill/core/browser/data_model/autofill_profile_comparator.h"
#include "components/autofill/core/browser/form_data_importer.h"
#include "components/autofill/core/browser/form_structure.h"
#include "components/autofill/core/browser/geo/country_names.h"
#include "components/autofill/core/browser/personal_data_manager.h"
#include "components/autofill/core/browser/test_autofill_client.h"
#include "components/autofill/core/common/autofill_features.h"
#include "components/autofill/core/common/form_data.h"
#include "testing/data_driven_testing/data_driven_test.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "url/gurl.h"

#if BUILDFLAG(IS_APPLE)
#include "base/mac/foundation_util.h"
#endif

namespace autofill {

namespace {
const base::FilePath::CharType kFeatureName[] = FILE_PATH_LITERAL("autofill");
const base::FilePath::CharType kTestName[] = FILE_PATH_LITERAL("merge");
const base::FilePath::CharType kFileNamePattern[] = FILE_PATH_LITERAL("*.in");

const char kFieldSeparator[] = ":";
const char kProfileSeparator[] = "---";

const ServerFieldType kProfileFieldTypes[] = {NAME_FIRST,
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
    base::PathService::Get(base::DIR_SOURCE_ROOT, &dir);
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
  base::mac::ClearAmIBundledCache();
#endif  // BUILDFLAG(IS_APPLE)

  return files;
}

// Serializes the |profiles| into a string.
std::string SerializeProfiles(const std::vector<AutofillProfile*>& profiles) {
  std::string result;
  for (auto* profile : profiles) {
    result += kProfileSeparator;
    result += "\n";
    for (const ServerFieldType& type : kProfileFieldTypes) {
      std::u16string value = profile->GetRawInfo(type);
      result += AutofillType::ServerFieldTypeToString(type);
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

class PersonalDataManagerMock : public PersonalDataManager {
 public:
  PersonalDataManagerMock();

  PersonalDataManagerMock(const PersonalDataManagerMock&) = delete;
  PersonalDataManagerMock& operator=(const PersonalDataManagerMock&) = delete;

  ~PersonalDataManagerMock() override;

  // Reset the saved profiles.
  void Reset();

  // PersonalDataManager:
  std::string SaveImportedProfile(const AutofillProfile& profile) override;
  std::vector<AutofillProfile*> GetProfiles() const override;

 private:
  std::vector<std::unique_ptr<AutofillProfile>> profiles_;
};

PersonalDataManagerMock::PersonalDataManagerMock()
    : PersonalDataManager("en-US", "US") {}

PersonalDataManagerMock::~PersonalDataManagerMock() = default;

void PersonalDataManagerMock::Reset() {
  profiles_.clear();
}

std::string PersonalDataManagerMock::SaveImportedProfile(
    const AutofillProfile& profile) {
  std::vector<AutofillProfile> new_profiles;
  std::string merged_guid = AutofillProfileComparator::MergeProfile(
      profile, profiles_, "en-US", &new_profiles);

  profiles_.clear();
  for (const AutofillProfile& it : new_profiles) {
    profiles_.push_back(std::make_unique<AutofillProfile>(it));
  }
  return merged_guid;
}

std::vector<AutofillProfile*> PersonalDataManagerMock::GetProfiles() const {
  std::vector<AutofillProfile*> result;
  for (const auto& profile : profiles_)
    result.push_back(profile.get());
  return result;
}

}  // namespace

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

  void TearDown() override;

  // DataDrivenTest:
  void GenerateResults(const std::string& input, std::string* output) override;

  // Deserializes a set of Autofill profiles from |profiles|, imports each
  // sequentially, and fills |merged_profiles| with the serialized result.
  void MergeProfiles(const std::string& profiles, std::string* merged_profiles);

  // Deserializes |str| into a field type.
  ServerFieldType StringToFieldType(const std::string& str);

  base::test::TaskEnvironment task_environment_;
  TestAutofillClient autofill_client_;
  PersonalDataManagerMock personal_data_;
  std::unique_ptr<FormDataImporter> form_data_importer_;

 private:
  std::map<std::string, ServerFieldType> string_to_field_type_map_;
};

AutofillMergeTest::AutofillMergeTest()
    : testing::DataDrivenTest(GetTestDataDir(), kFeatureName, kTestName) {
  CountryNames::SetLocaleString("en-US");
  for (size_t i = NO_SERVER_DATA; i < MAX_VALID_FIELD_TYPE; ++i) {
    ServerFieldType field_type = ToSafeServerFieldType(i, MAX_VALID_FIELD_TYPE);
    if (field_type == MAX_VALID_FIELD_TYPE)
      continue;
    string_to_field_type_map_[AutofillType::ServerFieldTypeToString(
        field_type)] = field_type;
  }
}

AutofillMergeTest::~AutofillMergeTest() = default;

void AutofillMergeTest::SetUp() {
  test::DisableSystemServices(nullptr);
  personal_data_.set_auto_accept_address_imports_for_testing(true);
  form_data_importer_ = std::make_unique<FormDataImporter>(
      &autofill_client_,
      /*payments::PaymentsClient=*/nullptr, &personal_data_, "en");
}

void AutofillMergeTest::TearDown() {
  test::ReenableSystemServices();
}

void AutofillMergeTest::GenerateResults(const std::string& input,
                                        std::string* output) {
  MergeProfiles(input, output);
}

void AutofillMergeTest::MergeProfiles(const std::string& profiles,
                                      std::string* merged_profiles) {
  // Start with no saved profiles.
  personal_data_.Reset();

  // Create a test form.
  FormData form;
  form.name = u"MyTestForm";
  form.url = GURL("https://www.example.com/origin.html");
  form.action = GURL("https://www.example.com/action.html");

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
      field.label = field_type;
      field.name = field_type;
      field.value = value;
      field.form_control_type = "text";
      field.is_focusable = true;
      form.fields.push_back(field);
    }

    // The first line is always a profile separator, and the last profile is not
    // followed by an explicit separator.
    if ((i > 0 && line == kProfileSeparator) || i == lines.size() - 1) {
      // Reached the end of a profile.  Try to import it.
      FormStructure form_structure(form);
      for (size_t j = 0; j < form_structure.field_count(); ++j) {
        // Set the heuristic type for each field, which is currently serialized
        // into the field's name.
        AutofillField* field =
            const_cast<AutofillField*>(form_structure.field(j));
        ServerFieldType type =
            StringToFieldType(base::UTF16ToUTF8(field->name));
        field->set_heuristic_type(GetActivePatternSource(), type);
      }

      // Extract the profile.
      auto extracted_data = form_data_importer_->ExtractFormData(
          form_structure,
          /*profile_autofill_enabled=*/true,
          /*payment_methods_autofill_enabled=*/true);
      form_data_importer_->ProcessAddressProfileImportCandidates(
          extracted_data.address_profile_import_candidates,
          /*allow_prompt=*/true);
      EXPECT_FALSE(extracted_data.credit_card_import_candidate);
      EXPECT_FALSE(extracted_data.extracted_upi_id.has_value());

      // Clear the |form| to start a new profile.
      form.fields.clear();
    }
  }

  *merged_profiles = SerializeProfiles(personal_data_.GetProfiles());
}

ServerFieldType AutofillMergeTest::StringToFieldType(const std::string& str) {
  return string_to_field_type_map_[str];
}

TEST_P(AutofillMergeTest, DataDrivenMergeProfiles) {
  const bool kIsExpectedToPass = true;
  RunOneDataDrivenTest(GetParam(), GetOutputDirectory(), kIsExpectedToPass);
}

INSTANTIATE_TEST_SUITE_P(All,
                         AutofillMergeTest,
                         testing::ValuesIn(GetTestFiles()));

}  // namespace autofill
