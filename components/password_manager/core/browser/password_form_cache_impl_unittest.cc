// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/password_manager/core/browser/password_form_cache_impl.h"

#include "base/test/task_environment.h"
#include "components/autofill/core/common/autofill_test_utils.h"
#include "components/autofill/core/common/form_data_test_api.h"
#include "components/password_manager/core/browser/fake_form_fetcher.h"
#include "components/password_manager/core/browser/password_form_digest.h"
#include "components/password_manager/core/browser/password_manager_test_utils.h"
#include "components/password_manager/core/browser/password_save_manager_impl.h"
#include "components/password_manager/core/browser/possible_username_data.h"
#include "components/password_manager/core/browser/stub_password_manager_client.h"
#include "components/password_manager/core/browser/stub_password_manager_driver.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "url/gurl.h"

#if BUILDFLAG(IS_MAC) || BUILDFLAG(IS_LINUX)
#include "components/os_crypt/sync/os_crypt_mocker.h"
#endif

namespace password_manager {
using autofill::FormData;
using autofill::FormFieldData;
using autofill::test::AutofillUnitTestEnvironment;
using autofill::test::CreateTestPasswordFormData;
using autofill::test::MakeFieldRendererId;
using autofill::test::MakeFormRendererId;
using testing::IsEmpty;
using testing::IsNull;
using testing::NotNull;

class PasswordFormCacheTest : public testing::Test {
 public:
  PasswordFormCacheTest() {
#if BUILDFLAG(IS_MAC) || BUILDFLAG(IS_LINUX)
    OSCryptMocker::SetUp();
#endif
  }

  StubPasswordManagerClient& client() { return client_; }

  StubPasswordManagerDriver& driver() { return driver_; }

  FakeFormFetcher& form_fetcher() { return form_fetcher_; }

  PasswordFormCacheImpl& cache() { return password_form_cache_; }

  std::unique_ptr<PasswordFormManager> CreateSubmittedManager(
      const FormData& form) {
    auto form_manager = std::make_unique<PasswordFormManager>(
        &client(), driver().AsWeakPtr(), form, &form_fetcher(),
        std::make_unique<PasswordSaveManagerImpl>(&client()),
        /*metrics_recorder=*/nullptr);

    // Fill values into the fields to save the form.
    FormData filled_form = form;
    EXPECT_EQ(filled_form.fields().size(), 2u);
    test_api(filled_form).field(0).set_value(u"username");
    test_api(filled_form).field(1).set_value(u"password");
    form_manager->ProvisionallySave(
        filled_form, &driver(),
        base::LRUCache<PossibleUsernameFieldIdentifier, PossibleUsernameData>(
            /*max_size=*/2));
    EXPECT_TRUE(form_manager->is_submitted());
    return form_manager;
  }

 private:
  base::test::SingleThreadTaskEnvironment task_environment_;
  AutofillUnitTestEnvironment autofill_test_environment_;
  StubPasswordManagerClient client_;
  StubPasswordManagerDriver driver_;
  FakeFormFetcher form_fetcher_;
  PasswordFormCacheImpl password_form_cache_;
};

// Test that cache is empty upon creation.
TEST_F(PasswordFormCacheTest, IsEmpty) {
  ASSERT_TRUE(cache().IsEmpty());
}

// Tests that the password form cache gets cleared correctly.
TEST_F(PasswordFormCacheTest, ClearFormCache) {
  auto form_manager = std::make_unique<PasswordFormManager>(
      &client(), driver().AsWeakPtr(), CreateTestPasswordFormData(),
      &form_fetcher(), std::make_unique<PasswordSaveManagerImpl>(&client()),
      /*metrics_recorder=*/nullptr);
  cache().AddFormManager(std::move(form_manager));
  ASSERT_FALSE(cache().IsEmpty());

  cache().Clear();
  ASSERT_TRUE(cache().IsEmpty());
}

// Test that cache correctly retrieves the form manager by form or field
// renderer id.
TEST_F(PasswordFormCacheTest, GetMatchedManager) {
  const FormData form = CreateTestPasswordFormData();
  auto form_manager = std::make_unique<PasswordFormManager>(
      &client(), driver().AsWeakPtr(), form, &form_fetcher(),
      std::make_unique<PasswordSaveManagerImpl>(&client()),
      /*metrics_recorder=*/nullptr);
  cache().AddFormManager(std::move(form_manager));
  PasswordFormManager* matched_manager =
      cache().GetMatchedManager(&driver(), form.renderer_id());

  ASSERT_TRUE(matched_manager->DoesManage(form.renderer_id(), &driver()));
  for (const FormFieldData& field : form.fields()) {
    ASSERT_TRUE(matched_manager->DoesManage(field.renderer_id(), &driver()));
    ASSERT_EQ(matched_manager,
              cache().GetMatchedManager(&driver(), field.renderer_id()));
  }
}

// Test that form cache returns `nullptr` if queried using wrong form and field
// ids.
TEST_F(PasswordFormCacheTest, GetMatchedManager_WrongId) {
  const FormData form = CreateTestPasswordFormData();
  auto form_manager = std::make_unique<PasswordFormManager>(
      &client(), driver().AsWeakPtr(), form, &form_fetcher(),
      std::make_unique<PasswordSaveManagerImpl>(&client()),
      /*metrics_recorder=*/nullptr);
  cache().AddFormManager(std::move(form_manager));

  const autofill::FormRendererId form_id = MakeFormRendererId();
  ASSERT_THAT(cache().GetMatchedManager(&driver(), form_id), IsNull());

  const autofill::FieldRendererId field_id = MakeFieldRendererId();
  ASSERT_THAT(cache().GetMatchedManager(&driver(), field_id), IsNull());
}

// Test that the password form cache returns `nullptr` is doesn't contain any
// submitted password form managers.
TEST_F(PasswordFormCacheTest, GetSubmittedManager_NoSubmittedManager) {
  FormData form = CreateTestPasswordFormData();

  auto form_manager = std::make_unique<PasswordFormManager>(
      &client(), driver().AsWeakPtr(), form, &form_fetcher(),
      std::make_unique<PasswordSaveManagerImpl>(&client()),
      /*metrics_recorder=*/nullptr);
  ASSERT_FALSE(form_manager->is_submitted());
  cache().AddFormManager(std::move(form_manager));

  EXPECT_THAT(cache().GetSubmittedManager(), IsNull());

  std::unique_ptr<PasswordFormManager> moved_manager =
      cache().MoveOwnedSubmittedManager();
  EXPECT_THAT(moved_manager.get(), IsNull());
}

// Test that the form cache correctly returns submitted password form manager.
TEST_F(PasswordFormCacheTest, MoveOwnedSubmittedManager) {
  FormData form = CreateTestPasswordFormData();
  cache().AddFormManager(CreateSubmittedManager(form));

  PasswordFormManager* submitted_manager = cache().GetSubmittedManager();
  ASSERT_TRUE(submitted_manager->is_submitted());
  ASSERT_TRUE(submitted_manager->DoesManage(form.renderer_id(), &driver()));
  ASSERT_FALSE(cache().IsEmpty());

  std::unique_ptr<PasswordFormManager> moved_submitted_manager =
      cache().MoveOwnedSubmittedManager();
  ASSERT_TRUE(moved_submitted_manager->is_submitted());
  ASSERT_TRUE(
      moved_submitted_manager->DoesManage(form.renderer_id(), &driver()));
  ASSERT_TRUE(cache().IsEmpty());
}

// Test that the password form cache correctly resets the submitted password
// form manager.
TEST_F(PasswordFormCacheTest, ResetSubmittedManager) {
  cache().AddFormManager(CreateSubmittedManager(CreateTestPasswordFormData()));

  EXPECT_THAT(cache().GetSubmittedManager(), NotNull());
  EXPECT_FALSE(cache().IsEmpty());

  cache().ResetSubmittedManager();

  EXPECT_THAT(cache().GetSubmittedManager(), IsNull());
  EXPECT_TRUE(cache().IsEmpty());
}

// Test that no password form manager is reset by `ResetSubmittedManager` if the
// cache doesn't contain a submitted password form manager.
TEST_F(PasswordFormCacheTest, ResetSubmittedManager_NoSubmittedManager) {
  FormData form = CreateTestPasswordFormData();
  auto form_manager = std::make_unique<PasswordFormManager>(
      &client(), driver().AsWeakPtr(), form, &form_fetcher(),
      std::make_unique<PasswordSaveManagerImpl>(&client()),
      /*metrics_recorder=*/nullptr);
  ASSERT_FALSE(form_manager->is_submitted());
  cache().AddFormManager(std::move(form_manager));

  EXPECT_THAT(cache().GetSubmittedManager(), IsNull());
  EXPECT_FALSE(cache().IsEmpty());

  cache().ResetSubmittedManager();
  EXPECT_FALSE(cache().IsEmpty());
  EXPECT_THAT(cache().GetMatchedManager(&driver(), form.renderer_id()),
              NotNull());
}

// Test that the cache returns the view over internally stored password form
// managers with expected properties.
TEST_F(PasswordFormCacheTest, GetFormManagers) {
  // Check that cache is empty in the beginning.
  EXPECT_THAT(cache().GetFormManagers(), IsEmpty());

  FormData form = CreateTestPasswordFormData();
  auto form_manager = std::make_unique<PasswordFormManager>(
      &client(), driver().AsWeakPtr(), form, &form_fetcher(),
      std::make_unique<PasswordSaveManagerImpl>(&client()),
      /*metrics_recorder=*/nullptr);

  cache().AddFormManager(std::move(form_manager));
  // Check that the size of the data view changed.
  EXPECT_EQ(cache().GetFormManagers().size(), 1u);

  // Check that iterators point to the expected password form managers.
  PasswordFormManager* matched_manager =
      cache().GetMatchedManager(&driver(), form.renderer_id());
  EXPECT_THAT(matched_manager, NotNull());
  EXPECT_EQ(matched_manager, cache().GetFormManagers()[0].get());
}

}  // namespace password_manager
