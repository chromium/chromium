// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/password_manager/core/browser/leak_detection_delegate_helper.h"

#include <memory>

#include "base/strings/utf_string_conversions.h"
#include "base/test/mock_callback.h"
#include "components/password_manager/core/browser/leak_detection_dialog_utils.h"
#include "components/password_manager/core/browser/mock_password_store.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

using autofill::PasswordForm;
using base::ASCIIToUTF16;
using base::BindOnce;
using base::MockCallback;
using base::Unretained;
using testing::_;
using testing::StrictMock;
using testing::WithArg;

namespace password_manager {

namespace {
constexpr char kLeakedPassword[] = "leaked_password";
constexpr char kLeakedUsername[] = "leaked_username";
constexpr char kOtherUsername[] = "other_username";
constexpr char kLeakedOrigin[] = "https://www.leaked_origin.de/login";
constexpr char kOtherOrigin[] = "https://www.other_origin.de/login";

// Creates a |PasswordForm| with the supplied |origin| and |username|. The
// password is always set to |kLeakedPassword|.
PasswordForm CreateForm(base::StringPiece origin, base::StringPiece username) {
  PasswordForm form;
  form.origin = GURL(ASCIIToUTF16(origin));
  form.username_value = ASCIIToUTF16(username);
  form.password_value = ASCIIToUTF16(kLeakedPassword);
  form.signon_realm = form.origin.GetOrigin().spec();
  return form;
}

// Used to mimic the callback of the |PasswordStore|.  Converts the vector of
// |PasswordForm|s to a vector of unique pointers to |PasswordForm|s.
ACTION_P(InvokeConsumerWithPasswordForms, forms) {
  std::vector<std::unique_ptr<autofill::PasswordForm>> results;
  for (const auto& form : forms) {
    results.push_back(std::make_unique<PasswordForm>(form));
  }
  arg0->OnGetPasswordStoreResults(std::move(results));
}

}  // namespace

class LeakDetectionDelegateHelperTest : public testing::Test {
 public:
  LeakDetectionDelegateHelperTest() = default;
  ~LeakDetectionDelegateHelperTest() override = default;

 protected:
  void SetUp() override {
    store_ = new testing::StrictMock<MockPasswordStore>;

    delegate_helper_ =
        std::make_unique<LeakDetectionDelegateHelper>(callback_.Get());
  }

  void TearDown() override {
    store_->ShutdownOnUIThread();
    store_ = nullptr;
  }

  // Initiates determining the credential leak type.
  void InitiateGetCredentialLeakType() {
    delegate_helper_->GetCredentialLeakType(store_.get(), GURL(kLeakedOrigin),
                                            ASCIIToUTF16(kLeakedUsername),
                                            ASCIIToUTF16(kLeakedPassword));
  }

  // Sets the |PasswordForm|s which are retrieve from the |PasswordStore|.
  void SetGetLoginByPasswordConsumerInvocation(
      std::vector<PasswordForm> password_forms) {
    EXPECT_CALL(*store_.get(), GetLoginsByPassword(_, _))
        .WillRepeatedly(
            WithArg<1>(InvokeConsumerWithPasswordForms(password_forms)));
  }

  // Set the expectation for the |CredentialLeakType| in the callback_.
  void SetOnShowLeakDetectionNotificationExpectation(IsSaved is_saved,
                                                     IsReused is_reused) {
    EXPECT_CALL(callback_,
                Run(CreateLeakType(is_saved, is_reused, IsSyncing(true)),
                    GURL(kLeakedOrigin), ASCIIToUTF16(kLeakedUsername)))
        .Times(1);
  }

  MockCallback<LeakDetectionDelegateHelper::LeakTypeReply> callback_;
  scoped_refptr<MockPasswordStore> store_;
  std::unique_ptr<LeakDetectionDelegateHelper> delegate_helper_;
};

// Credentials are neither saved nor is the password reused.
TEST_F(LeakDetectionDelegateHelperTest, NeitherSaveNotReused) {
  std::vector<PasswordForm> password_forms;

  SetGetLoginByPasswordConsumerInvocation(std::move(password_forms));
  SetOnShowLeakDetectionNotificationExpectation(IsSaved(false),
                                                IsReused(false));
  InitiateGetCredentialLeakType();
}

// Credentials are saved but the password is not reused.
TEST_F(LeakDetectionDelegateHelperTest, SavedLeakedCredentials) {
  std::vector<PasswordForm> password_forms = {
      CreateForm(kLeakedOrigin, kLeakedUsername)};

  SetGetLoginByPasswordConsumerInvocation(std::move(password_forms));
  SetOnShowLeakDetectionNotificationExpectation(IsSaved(true), IsReused(false));
  InitiateGetCredentialLeakType();
}

// Credentials are saved and the password is reused on a different origin.
TEST_F(LeakDetectionDelegateHelperTest,
       SavedCredentialsAndReusedPasswordOnOtherOrigin) {
  std::vector<PasswordForm> password_forms = {
      CreateForm(kLeakedOrigin, kLeakedUsername),
      CreateForm(kOtherOrigin, kLeakedUsername)};

  SetGetLoginByPasswordConsumerInvocation(std::move(password_forms));
  SetOnShowLeakDetectionNotificationExpectation(IsSaved(true), IsReused(true));
  InitiateGetCredentialLeakType();
}

// Credentials are saved and the password is reused on the same origin with
// a different username.
TEST_F(LeakDetectionDelegateHelperTest,
       SavedCredentialsAndReusedPasswordWithOtherUsername) {
  std::vector<PasswordForm> password_forms = {
      CreateForm(kLeakedOrigin, kLeakedUsername),
      CreateForm(kLeakedOrigin, kOtherUsername)};

  SetGetLoginByPasswordConsumerInvocation(std::move(password_forms));
  SetOnShowLeakDetectionNotificationExpectation(IsSaved(true), IsReused(true));
  InitiateGetCredentialLeakType();
}

// Credentials are not saved but the password is reused.
TEST_F(LeakDetectionDelegateHelperTest, ReusedPasswordWithOtherUsername) {
  std::vector<PasswordForm> password_forms = {
      CreateForm(kLeakedOrigin, kOtherUsername)};

  SetGetLoginByPasswordConsumerInvocation(std::move(password_forms));
  SetOnShowLeakDetectionNotificationExpectation(IsSaved(false), IsReused(true));
  InitiateGetCredentialLeakType();
}

// Credentials are not saved but the password is reused on a different origin.
TEST_F(LeakDetectionDelegateHelperTest, ReusedPasswordOnOtherOrigin) {
  std::vector<PasswordForm> password_forms = {
      CreateForm(kOtherOrigin, kLeakedUsername)};

  SetGetLoginByPasswordConsumerInvocation(std::move(password_forms));
  SetOnShowLeakDetectionNotificationExpectation(IsSaved(false), IsReused(true));
  InitiateGetCredentialLeakType();
}

// Credentials are not saved but the password is reused with a different
// username on a different origin.
TEST_F(LeakDetectionDelegateHelperTest, ReusedPassword) {
  std::vector<PasswordForm> password_forms = {
      CreateForm(kOtherOrigin, kOtherUsername)};

  SetGetLoginByPasswordConsumerInvocation(std::move(password_forms));
  SetOnShowLeakDetectionNotificationExpectation(IsSaved(false), IsReused(true));
  InitiateGetCredentialLeakType();
}

}  // namespace password_manager
