// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/autofill/core/browser/data_manager/addresses/home_and_work_metadata_store.h"

#include <memory>

#include "base/functional/callback_helpers.h"
#include "base/test/mock_callback.h"
#include "base/test/scoped_feature_list.h"
#include "base/test/task_environment.h"
#include "base/time/time.h"
#include "base/values.h"
#include "components/autofill/core/browser/data_model/addresses/autofill_profile_test_api.h"
#include "components/autofill/core/browser/test_utils/autofill_test_utils.h"
#include "components/autofill/core/browser/webdata/autofill_change.h"
#include "components/autofill/core/common/autofill_features.h"
#include "components/autofill/core/common/autofill_prefs.h"
#include "components/prefs/pref_service.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace autofill {

namespace {

class HomeAndWorkMetadataStoreTest : public testing::Test {
 public:
  HomeAndWorkMetadataStoreTest() : prefs_(test::PrefServiceForTesting()) {}

  PrefService* pref_service() { return prefs_.get(); }

 private:
  base::test::TaskEnvironment task_environment_{
      base::test::TaskEnvironment::TimeSource::MOCK_TIME};
  base::test::ScopedFeatureList feature_{
      features::kAutofillEnableSupportForHomeAndWork};
  std::unique_ptr<PrefService> prefs_;
};

// Tests that any Home and Work metadata persisted with an update
// `ApplyChange()` is restored by `ApplyMetadata()`.
TEST_F(HomeAndWorkMetadataStoreTest, Update_HomeAndWork) {
  base::MockRepeatingClosure on_change;
  HomeAndWorkMetadataStore metadata_store(pref_service(), on_change.Get());

  AutofillProfile profile = test::GetFullProfile();
  test_api(profile).set_record_type(AutofillProfile::RecordType::kAccountHome);
  profile.usage_history().set_use_count(5);
  profile.usage_history().set_use_date(base::Time::Now() - base::Minutes(3));

  EXPECT_CALL(on_change, Run());
  metadata_store.ApplyChange(AutofillProfileChange(
      AutofillProfileChange::UPDATE, profile.guid(), profile));

  AutofillProfile modified_profile = profile;
  modified_profile.usage_history().set_use_count(123);
  modified_profile.usage_history().set_use_date(base::Time::Now() -
                                                base::Minutes(345));
  EXPECT_THAT(metadata_store.ApplyMetadata(
                  std::vector<AutofillProfile>{modified_profile}),
              testing::ElementsAre(profile));
}

// Tests that non Home and Work addresses are not affected by
// `ApplyChange()` and `ApplyMetadata()`.
TEST_F(HomeAndWorkMetadataStoreTest, Update_NonHomeAndWork) {
  base::MockRepeatingClosure on_change;
  HomeAndWorkMetadataStore metadata_store(pref_service(), on_change.Get());

  AutofillProfile profile = test::GetFullProfile();
  profile.usage_history().set_use_count(5);
  profile.usage_history().set_use_date(base::Time::Now() - base::Minutes(3));

  EXPECT_CALL(on_change, Run()).Times(0);
  metadata_store.ApplyChange(AutofillProfileChange(
      AutofillProfileChange::UPDATE, profile.guid(), profile));

  AutofillProfile modified_profile = profile;
  modified_profile.usage_history().set_use_count(123);
  modified_profile.usage_history().set_use_date(base::Time::Now() -
                                                base::Minutes(345));
  EXPECT_THAT(metadata_store.ApplyMetadata(
                  std::vector<AutofillProfile>{modified_profile}),
              testing::ElementsAre(modified_profile));
}

// Tests that after a HIDE_IN_AUTOFILL change, `ApplyMetadata()` removes Home
// and Work profiles.
TEST_F(HomeAndWorkMetadataStoreTest, Remove_HomeAndWork) {
  base::MockRepeatingClosure on_change;
  HomeAndWorkMetadataStore metadata_store(pref_service(), on_change.Get());

  AutofillProfile profile = test::GetFullProfile();
  test_api(profile).set_record_type(AutofillProfile::RecordType::kAccountHome);
  EXPECT_CALL(on_change, Run());
  metadata_store.ApplyChange(AutofillProfileChange(
      AutofillProfileChange::HIDE_IN_AUTOFILL, profile.guid(), profile));

  EXPECT_THAT(
      metadata_store.ApplyMetadata(std::vector<AutofillProfile>{profile}),
      testing::IsEmpty());

  // Once the modification date increases, the address reappears.
  profile.usage_history().set_modification_date(base::Time::Now() +
                                                base::Minutes(1));
  EXPECT_THAT(
      metadata_store.ApplyMetadata(std::vector<AutofillProfile>{profile}),
      testing::ElementsAre(profile));
}

// Tests that non Home and Work addresses are not affected by
// `ApplyChange()` and `ApplyMetadata()`.
TEST_F(HomeAndWorkMetadataStoreTest, Remove_NonHomeAndWork) {
  base::MockRepeatingClosure on_change;
  HomeAndWorkMetadataStore metadata_store(pref_service(), on_change.Get());

  AutofillProfile profile = test::GetFullProfile();
  EXPECT_CALL(on_change, Run()).Times(0);
  metadata_store.ApplyChange(AutofillProfileChange(
      AutofillProfileChange::HIDE_IN_AUTOFILL, profile.guid(), profile));

  EXPECT_THAT(
      metadata_store.ApplyMetadata(std::vector<AutofillProfile>{profile}),
      testing::ElementsAre(profile));
}

// Tests that observers are notified when a pref is changed from outside the
// class, e.g. through sync.
TEST_F(HomeAndWorkMetadataStoreTest, MetadataChangeThroughSync) {
  base::MockRepeatingClosure on_change;
  HomeAndWorkMetadataStore metadata_store(pref_service(), on_change.Get());
  EXPECT_CALL(on_change, Run());
  pref_service()->SetDict(prefs::kAutofillHomeMetadata, base::DictValue());
}

}  // namespace

}  // namespace autofill
